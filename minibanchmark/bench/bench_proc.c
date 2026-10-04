/**
 * bench_proc.c — F5: processi figli, protocollo padre/figlio, tempo massimo e inattività.
 *
 * B2 [P]: il codice specifico del sistema operativo sta solo qui, nella sezione
 * "Livello di sistema" (Windows / Linux). Il resto del file è C portabile.
 *
 * Protocollo (figlio → padre, su stdout del figlio): una riga per messaggio, campi separati
 * da tab, numeri in formato C con il punto decimale. La formattazione all'italiana la fa il padre.
 *   DEVICE     <etichetta>
 *   CONFIGS    <generate> <scartate>
 *   DISCARD    <numero> <motivo>
 *   RUN_START  <x> <y> <z>
 *   RUN_END    <x> <y> <z>
 *   RESULT     <dim> <lx> <ly> <lz> <ex> <ey> <ez> <%scartata> <mediana_ns> <flop> <OK|ERRATO> <host_timing> <indice> <atteso> <ottenuto>
 *   REJECT     <x> <y> <z> <motivo>
 *   TIMEOUT_OS <x> <y> <z>
 *   SKIP       <motivo>
 *   DONE
 */

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#else
#error "sistema operativo non supportato"
#endif

#include "bench_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Livello di sistema
 * ========================================================================= */

#define LINE_MAX_LEN 1024

typedef struct {
#if defined(_WIN32) || defined(_WIN64)
    HANDLE process;
    HANDLE read_pipe;
#else
    pid_t  pid;
    int    read_fd;
#endif
    char   buffer[LINE_MAX_LEN * 4];
    size_t buffered;
    int    eof;
} child_process;

double bench_clock_ns(void)
{
#if defined(_WIN32) || defined(_WIN64)
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1e9 / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
#endif
}

/*
 * Avvia lo stesso eseguibile come figlio con gli argomenti dati, con lo stdout collegato
 * a una pipe (lo stderr resta sulla console).
 * @return 0 se avviato, altrimenti codice di errore di sistema (reason compilato).
 */
static int child_spawn(child_process *child, const char *const *args, size_t arg_count,
                       char *reason, size_t reason_len)
{
    memset(child, 0, sizeof(*child));

#if defined(_WIN32) || defined(_WIN64)
    char exe[MAX_PATH];
    DWORD exe_len = GetModuleFileNameA(NULL, exe, sizeof(exe));
    if (exe_len == 0 || exe_len >= sizeof(exe)) {
        snprintf(reason, reason_len, "GetModuleFileNameA fallita (codice %lu)", GetLastError());
        return -1;
    }

    /* Riga di comando: percorso tra virgolette (può contenere spazi), poi gli argomenti. */
    char command[MAX_PATH + 256];
    int pos = snprintf(command, sizeof(command), "\"%s\"", exe);
    for (size_t i = 0; i < arg_count && pos > 0 && (size_t)pos < sizeof(command); i++) {
        pos += snprintf(command + pos, sizeof(command) - (size_t)pos, " %s", args[i]);
    }

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE read_pipe = NULL, write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        snprintf(reason, reason_len, "CreatePipe fallita (codice %lu)", GetLastError());
        return -1;
    }
    /* Solo l'estremità di scrittura va ereditata: altrimenti la fine del flusso non arriva mai. */
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = write_pipe;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi;
    if (!CreateProcessA(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        snprintf(reason, reason_len, "CreateProcessA fallita (codice %lu)", GetLastError());
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return -1;
    }
    CloseHandle(write_pipe);
    CloseHandle(pi.hThread);
    child->process = pi.hProcess;
    child->read_pipe = read_pipe;
    return 0;
#else
    char exe[4096];
    ssize_t exe_len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (exe_len <= 0) {
        snprintf(reason, reason_len, "readlink(/proc/self/exe) fallita (errno %d)", errno);
        return -1;
    }
    exe[exe_len] = '\0';

    char *argv[16];
    if (arg_count + 2 > sizeof(argv) / sizeof(argv[0])) {
        snprintf(reason, reason_len, "troppi argomenti per il processo figlio");
        return -1;
    }
    argv[0] = exe;
    for (size_t i = 0; i < arg_count; i++) {
        argv[i + 1] = (char *)args[i];
    }
    argv[arg_count + 1] = NULL;

    int fds[2];
    if (pipe(fds) != 0) {
        snprintf(reason, reason_len, "pipe fallita (errno %d)", errno);
        return -1;
    }
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) {
        snprintf(reason, reason_len, "fork fallita (errno %d)", errno);
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        /* fork seguito subito da exec: il padre ha già inizializzato OpenCL (F5, assunzioni). */
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execv(exe, argv);
        _exit(127);
    }
    close(fds[1]);
    child->pid = pid;
    child->read_fd = fds[0];
    return 0;
#endif
}

/* Legge i byte disponibili senza bloccare oltre timeout_ns. @return >0 letti, 0 nessuno, -1 fine flusso. */
static int child_read_some(child_process *child, double timeout_ns)
{
    size_t space = sizeof(child->buffer) - child->buffered;
    if (space == 0) {
        return -1;   /* riga troppo lunga: protocollo rotto */
    }
#if defined(_WIN32) || defined(_WIN64)
    double deadline = bench_clock_ns() + timeout_ns;
    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(child->read_pipe, NULL, 0, NULL, &available, NULL)) {
            return -1;   /* ERROR_BROKEN_PIPE: il figlio ha chiuso lo stdout */
        }
        if (available > 0) {
            DWORD to_read = available < space ? available : (DWORD)space;
            DWORD read = 0;
            if (!ReadFile(child->read_pipe, child->buffer + child->buffered, to_read, &read, NULL) || read == 0) {
                return -1;
            }
            child->buffered += read;
            return (int)read;
        }
        if (bench_clock_ns() >= deadline) {
            return 0;
        }
        Sleep(5);   /* [D] intervallo di attesa */
    }
#else
    int timeout_ms = timeout_ns <= 0 ? 0 : (int)(timeout_ns / 1e6) + 1;
    struct pollfd pfd = { child->read_fd, POLLIN, 0 };
    int ready = poll(&pfd, 1, timeout_ms);
    if (ready < 0) {
        return errno == EINTR ? 0 : -1;
    }
    if (ready == 0) {
        return 0;
    }
    ssize_t n = read(child->read_fd, child->buffer + child->buffered, space);
    if (n <= 0) {
        return -1;
    }
    child->buffered += (size_t)n;
    return (int)n;
#endif
}

/*
 * Legge una riga completa (senza '\n' e '\r' finali) entro timeout_ns.
 * @return 1 riga letta, 0 tempo scaduto, -1 fine del flusso.
 */
static int child_read_line(child_process *child, char *line, size_t line_len, double timeout_ns)
{
    double deadline = bench_clock_ns() + timeout_ns;
    for (;;) {
        char *newline = memchr(child->buffer, '\n', child->buffered);
        if (newline != NULL) {
            size_t length = (size_t)(newline - child->buffer);
            size_t copy = length < line_len - 1 ? length : line_len - 1;
            memcpy(line, child->buffer, copy);
            line[copy] = '\0';
            if (copy > 0 && line[copy - 1] == '\r') {
                line[copy - 1] = '\0';   /* stdout in modalità testo su Windows */
            }
            child->buffered -= length + 1;
            memmove(child->buffer, newline + 1, child->buffered);
            return 1;
        }
        if (child->eof) {
            return -1;
        }
        double remaining = deadline - bench_clock_ns();
        if (remaining <= 0) {
            return 0;
        }
        int rc = child_read_some(child, remaining);
        if (rc < 0) {
            child->eof = 1;
        } else if (rc == 0) {
            return 0;
        }
    }
}

static void child_kill(child_process *child)
{
#if defined(_WIN32) || defined(_WIN64)
    TerminateProcess(child->process, 1);
#else
    kill(child->pid, SIGKILL);
#endif
}

/* Attende la fine del figlio, ne restituisce il codice di uscita e libera le risorse. */
static unsigned long child_wait(child_process *child)
{
#if defined(_WIN32) || defined(_WIN64)
    DWORD code = 0;
    WaitForSingleObject(child->process, INFINITE);
    GetExitCodeProcess(child->process, &code);
    CloseHandle(child->process);
    CloseHandle(child->read_pipe);
    return code;
#else
    int status = 0;
    while (waitpid(child->pid, &status, 0) < 0 && errno == EINTR) {
    }
    close(child->read_fd);
    if (WIFEXITED(status)) {
        return (unsigned long)WEXITSTATUS(status);
    }
    return WIFSIGNALED(status) ? 128ul + (unsigned long)WTERMSIG(status) : 255ul;
#endif
}

/* =========================================================================
 * Figlio: il reporter scrive il protocollo su stdout
 * ========================================================================= */

/* I motivi non devono contenere separatori del protocollo. */
static void sanitize(char *out, size_t len, const char *text)
{
    snprintf(out, len, "%s", text);
    for (char *p = out; *p != '\0'; p++) {
        if (*p == '\t' || *p == '\n' || *p == '\r') {
            *p = ' ';
        }
    }
}

static void emit(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    fputc('\n', stdout);
    fflush(stdout);   /* B4: senza fflush il padre vedrebbe RUN_START in ritardo */
}

static void child_configs(void *ctx, const bench_discard_summary *summary)
{
    (void)ctx;
    emit("CONFIGS\t%zu\t%zu", summary->generated, summary->discarded);
    for (size_t i = 0; i < summary->reason_count; i++) {
        char reason[BENCH_REASON_MAX];
        sanitize(reason, sizeof(reason), summary->reasons[i].reason);
        emit("DISCARD\t%zu\t%s", summary->reasons[i].count, reason);
    }
}

static void child_run_start(void *ctx, const bench_config *c)
{
    (void)ctx;
    emit("RUN_START\t%zu\t%zu\t%zu", c->local[0], c->local[1], c->local[2]);
}

static void child_run_end(void *ctx, const bench_config *c)
{
    (void)ctx;
    emit("RUN_END\t%zu\t%zu\t%zu", c->local[0], c->local[1], c->local[2]);
}

static void child_result(void *ctx, const bench_result *r)
{
    (void)ctx;
    emit("RESULT\t%u\t%zu\t%zu\t%zu\t%zu\t%zu\t%zu\t%.17g\t%.17g\t%.17g\t%s\t%d\t%zu\t%.17g\t%.17g",
         r->work_dim, r->local[0], r->local[1], r->local[2], r->eff[0], r->eff[1], r->eff[2],
         r->discarded_pct, r->median_ns, r->flop,
         r->status == BENCH_STATUS_OK ? "OK" : "ERRATO", r->host_timing,
         r->mismatch.index, r->mismatch.expected, r->mismatch.got);
}

static void child_discard(void *ctx, const bench_config *c, const char *reason)
{
    (void)ctx;
    char clean[BENCH_REASON_MAX];
    sanitize(clean, sizeof(clean), reason);
    emit("REJECT\t%zu\t%zu\t%zu\t%s", c->local[0], c->local[1], c->local[2], clean);
}

static void child_timeout_os(void *ctx, const bench_config *c)
{
    (void)ctx;
    emit("TIMEOUT_OS\t%zu\t%zu\t%zu", c->local[0], c->local[1], c->local[2]);
}

static void child_timeout(void *ctx, const bench_config *c, double limit_s)
{
    (void)ctx; (void)c; (void)limit_s;   /* lo decide solo il padre */
}

static void child_skip(void *ctx, const char *reason)
{
    (void)ctx;
    char clean[BENCH_REASON_MAX];
    sanitize(clean, sizeof(clean), reason);
    emit("SKIP\t%s", clean);
}

int bench_is_child(int argc, char **argv)
{
    return argc >= 2 && strcmp(argv[1], BENCH_CHILD_ARG) == 0;
}

/* Converte un indice passato come argomento; -1 se non valido. */
static long parse_index(const char *text)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);
    return (end != text && *end == '\0' && value >= 0) ? value : -1;
}

int bench_child_main(int argc, char **argv,
                     const bench_test *const *tests, size_t test_count,
                     const bench_options *options)
{
    if (argc != 5) {
        fprintf(stderr, "uso interno: %s <test> <piattaforma> <device>\n", BENCH_CHILD_ARG);
        return 2;
    }
    long test_index = parse_index(argv[2]);
    long platform_index = parse_index(argv[3]);
    long device_index = parse_index(argv[4]);
    char reason[BENCH_REASON_MAX];

    if (test_index < 0 || (size_t)test_index >= test_count ||
        !bench_validate_test(tests, (size_t)test_index, reason, sizeof(reason))) {
        emit("SKIP\ttest non valido nel processo figlio");
        emit("DONE");
        return 0;
    }

    bench_device *devices = NULL;
    size_t device_count = 0;
    bench_discover(&devices, &device_count, 0);   /* silenziosa: i messaggi li ha già dati il padre */

    const bench_device *device = NULL;
    for (size_t i = 0; i < device_count; i++) {
        if ((long)devices[i].platform_index == platform_index && (long)devices[i].device_index == device_index) {
            device = &devices[i];
        }
    }
    if (device == NULL) {
        emit("SKIP\tdevice non trovato nel processo figlio");
    } else {
        char label[BENCH_LABEL_MAX];
        sanitize(label, sizeof(label), device->label);
        emit("DEVICE\t%s", label);   /* il padre controlla che sia lo stesso device (F1) */

        bench_reporter reporter = {
            .ctx = NULL,
            .configs = child_configs,
            .run_start = child_run_start,
            .run_end = child_run_end,
            .result = child_result,
            .discard = child_discard,
            .timeout_os = child_timeout_os,
            .timeout = child_timeout,
            .skip = child_skip,
        };
        bench_execute_pair(tests[test_index], device, options, &reporter);
    }

    bench_devices_free(devices);
    emit("DONE");
    return 0;
}

/* =========================================================================
 * Padre: supervisione di una coppia test/device
 * ========================================================================= */

/* Divide una riga del protocollo nei campi separati da tab (modifica la riga). */
static size_t split_fields(char *line, char **fields, size_t max_fields)
{
    size_t count = 0;
    char *p = line;
    while (count < max_fields) {
        fields[count++] = p;
        char *tab = strchr(p, '\t');
        if (tab == NULL) {
            break;
        }
        *tab = '\0';
        p = tab + 1;
    }
    return count;
}

static size_t to_size(const char *text)
{
    return (size_t)strtoull(text, NULL, 10);
}

static void fields_to_config(char **fields, bench_config *config)
{
    memset(config, 0, sizeof(*config));
    config->local[0] = to_size(fields[0]);
    config->local[1] = to_size(fields[1]);
    config->local[2] = to_size(fields[2]);
}

void bench_supervise_pair(size_t test_index, const bench_test *test, const bench_device *device,
                          const bench_options *options, const bench_reporter *rep)
{
    char reason[BENCH_REASON_MAX];
    char arg_test[32], arg_platform[32], arg_device[32];
    snprintf(arg_test, sizeof(arg_test), "%zu", test_index);
    snprintf(arg_platform, sizeof(arg_platform), "%u", device->platform_index);
    snprintf(arg_device, sizeof(arg_device), "%u", device->device_index);
    const char *args[] = { BENCH_CHILD_ARG, arg_test, arg_platform, arg_device };

    fflush(stdout);
    child_process child;
    char spawn_error[80];
    if (child_spawn(&child, args, sizeof(args) / sizeof(args[0]), spawn_error, sizeof(spawn_error)) != 0) {
        char message[BENCH_REASON_MAX];
        snprintf(message, sizeof(message), "impossibile avviare il processo figlio: %s", spawn_error);
        rep->skip(rep->ctx, message);
        return;
    }

    double timeout_s = test->timeout_s > 0 ? test->timeout_s : options->timeout_s;
    double timeout_ns = timeout_s * 1e9;
    double inactivity_ns = options->inactivity_s * 1e9;

    int done = 0, killed = 0, in_run = 0;
    /* Fase raggiunta dal figlio, per spiegare un'uscita anomala (es. driver che va in crash
     * compilando un sorgente con errori: succede con OpenCLOn12). */
    const char *phase = "all'avvio";
    bench_config running;
    memset(&running, 0, sizeof(running));
    double run_started = 0.0;
    double last_message = bench_clock_ns();
    char line[LINE_MAX_LEN];

    while (!done && !killed) {
        /* Durante un'esecuzione vale il tempo massimo (RB-10), altrimenti l'inattività (RB-12). */
        double now = bench_clock_ns();
        double wait = in_run ? (run_started + timeout_ns - now) : (last_message + inactivity_ns - now);
        int rc = wait > 0 ? child_read_line(&child, line, sizeof(line), wait) : 0;

        if (rc < 0) {
            break;   /* fine del flusso */
        }
        if (rc == 0) {
            child_kill(&child);
            killed = 1;
            if (in_run) {
                rep->timeout(rep->ctx, &running, timeout_s);
            } else {
                rep->skip(rep->ctx, "il processo non risponde");
            }
            break;
        }

        last_message = bench_clock_ns();
        char *f[16];
        size_t n = split_fields(line, f, 16);
        const char *kind = f[0];

        if (strcmp(kind, "DONE") == 0) {
            done = 1;
        } else if (strcmp(kind, "RUN_START") == 0 && n >= 4) {
            fields_to_config(f + 1, &running);
            in_run = 1;
            run_started = last_message;
        } else if (strcmp(kind, "RUN_END") == 0) {
            in_run = 0;
        } else if (strcmp(kind, "DEVICE") == 0 && n >= 2) {
            phase = "durante la compilazione del kernel";
            if (strcmp(f[1], device->label) != 0) {
                child_kill(&child);
                killed = 1;
                snprintf(reason, sizeof(reason), "device diverso nel processo figlio (%s)", f[1]);
                rep->skip(rep->ctx, reason);
            }
        } else if (strcmp(kind, "CONFIGS") == 0 && n >= 3) {
            /* Le righe DISCARD seguono subito: si raccolgono prima di inoltrare il riepilogo. */
            bench_discard_summary summary;
            memset(&summary, 0, sizeof(summary));
            summary.generated = to_size(f[1]);
            summary.discarded = to_size(f[2]);
            size_t pending = summary.discarded;
            while (pending > 0 && summary.reason_count < BENCH_MAX_DISCARD_REASONS) {
                if (child_read_line(&child, line, sizeof(line), inactivity_ns) != 1) {
                    break;
                }
                char *d[4];
                if (split_fields(line, d, 4) < 3 || strcmp(d[0], "DISCARD") != 0) {
                    break;
                }
                size_t count = to_size(d[1]);
                summary.reasons[summary.reason_count].count = count;
                snprintf(summary.reasons[summary.reason_count].reason, BENCH_REASON_MAX, "%s", d[2]);
                summary.reason_count++;
                pending = count < pending ? pending - count : 0;
            }
            last_message = bench_clock_ns();
            phase = "durante preparazione, misura o verifica";
            rep->configs(rep->ctx, &summary);
        } else if (strcmp(kind, "RESULT") == 0 && n >= 16) {
            bench_result r;
            memset(&r, 0, sizeof(r));
            r.work_dim = (cl_uint)to_size(f[1]);
            for (int d = 0; d < 3; d++) {
                r.local[d] = to_size(f[2 + d]);
                r.eff[d] = to_size(f[5 + d]);
            }
            r.discarded_pct = strtod(f[8], NULL);
            r.median_ns = strtod(f[9], NULL);
            r.flop = strtod(f[10], NULL);
            r.status = strcmp(f[11], "OK") == 0 ? BENCH_STATUS_OK : BENCH_STATUS_WRONG;
            r.host_timing = atoi(f[12]);
            r.mismatch.index = to_size(f[13]);
            r.mismatch.expected = strtod(f[14], NULL);
            r.mismatch.got = strtod(f[15], NULL);
            rep->result(rep->ctx, &r);
        } else if (strcmp(kind, "REJECT") == 0 && n >= 5) {
            bench_config c;
            fields_to_config(f + 1, &c);
            rep->discard(rep->ctx, &c, f[4]);
        } else if (strcmp(kind, "TIMEOUT_OS") == 0 && n >= 4) {
            bench_config c;
            fields_to_config(f + 1, &c);
            rep->timeout_os(rep->ctx, &c);
        } else if (strcmp(kind, "SKIP") == 0 && n >= 2) {
            rep->skip(rep->ctx, f[1]);
        } else {
            fprintf(stderr, "Avviso: riga del processo figlio non riconosciuta: %s\n", line);
        }
        fflush(stdout);
    }

    unsigned long code = child_wait(&child);
    if (!done && !killed) {
        snprintf(reason, sizeof(reason), "processo terminato in modo anomalo %s (codice 0x%lX)", phase, code);
        rep->skip(rep->ctx, reason);
    }
}
