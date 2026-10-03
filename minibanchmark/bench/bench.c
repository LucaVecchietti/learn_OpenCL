/**
 * bench.c — implementazione della parte comune (F1, poi F3, F4, F6).
 *
 * Stato: passo 3 di B6 (F1, F2, F3, F4 nel processo figlio, F6; F5 in bench_proc.c).
 * I riferimenti (A6, F1, RB-n, CA-n) puntano alla commissione.
 */

#include "bench.h"
#include "bench_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * F1 — helper interni
 * ========================================================================= */

/*
 * Legge una stringa di piattaforma in un buffer allocato (da liberare con free).
 * Restituisce NULL in caso di errore: il chiamante stampa "n/d" o esclude il device.
 */
static char *read_platform_string(cl_platform_id platform, cl_platform_info param)
{
    size_t size = 0;
    if (clGetPlatformInfo(platform, param, 0, NULL, &size) != CL_SUCCESS || size == 0) {
        return NULL;
    }
    char *buffer = malloc(size);
    if (buffer == NULL) {
        return NULL;
    }
    if (clGetPlatformInfo(platform, param, size, buffer, NULL) != CL_SUCCESS) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

/* Come read_platform_string, per le stringhe di un device. */
static char *read_device_string(cl_device_id device, cl_device_info param)
{
    size_t size = 0;
    if (clGetDeviceInfo(device, param, 0, NULL, &size) != CL_SUCCESS || size == 0) {
        return NULL;
    }
    char *buffer = malloc(size);
    if (buffer == NULL) {
        return NULL;
    }
    if (clGetDeviceInfo(device, param, size, buffer, NULL) != CL_SUCCESS) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

/*
 * Interpreta CL_DEVICE_VERSION ("OpenCL <maj>.<min> <specifico del vendor>").
 * Se la stringa manca o non è interpretabile il device è trattato come 1.2 (F1, casi di errore).
 */
static void parse_device_version(const char *version, const char *label,
                                 int *major, int *minor, int report)
{
    if (version != NULL && sscanf(version, "OpenCL %d.%d", major, minor) == 2) {
        return;
    }
    *major = 1;
    *minor = 2;
    if (report) {
        fprintf(stderr, "Avviso: %s: versione OpenCL non interpretabile (\"%s\"), trattato come 1.2\n",
                label, version != NULL ? version : "n/d");
    }
}

/* Marca il device come escluso; si conserva il primo motivo trovato. */
static void exclude_device(bench_device *d, const char *reason)
{
    if (!d->excluded) {
        d->excluded = 1;
        snprintf(d->exclusion_reason, sizeof(d->exclusion_reason), "%s", reason);
    }
}

/*
 * Legge un limite del device. Se la lettura fallisce il device viene escluso
 * con il nome del parametro e il codice di errore (F1, casi di errore).
 *
 * @return 1 se letto, 0 se fallito.
 */
static int read_device_limit(cl_device_id device, cl_device_info param, const char *param_name,
                             size_t size, void *value, bench_device *out)
{
    cl_int err = clGetDeviceInfo(device, param, size, value, NULL);
    if (err != CL_SUCCESS) {
        char reason[BENCH_REASON_MAX];
        snprintf(reason, sizeof(reason), "lettura %s fallita (codice %d)", param_name, err);
        exclude_device(out, reason);
        return 0;
    }
    return 1;
}

#define READ_LIMIT(param, field) \
    read_device_limit(device, param, #param, sizeof(out->field), &out->field, out)

/*
 * Riempie un bench_device a partire da piattaforma e device.
 * Un limite non leggibile, un device non disponibile o senza compilatore
 * non sono errori: il device resta nell'elenco, marcato come escluso.
 *
 * @return 0 se il device è stato descritto (anche se escluso), -1 per errori di allocazione.
 */
static int describe_device(cl_platform_id platform, cl_uint platform_index,
                           cl_device_id device, cl_uint device_index,
                           bench_device *out, int report)
{
    memset(out, 0, sizeof(*out));
    out->platform = platform;
    out->platform_index = platform_index;
    out->device = device;
    out->device_index = device_index;

    /* Etichetta "piattaforma / device": la stessa GPU può comparire su due piattaforme. */
    char *platform_name = read_platform_string(platform, CL_PLATFORM_NAME);
    char *device_name = read_device_string(device, CL_DEVICE_NAME);
    snprintf(out->label, sizeof(out->label), "%s / %s",
             platform_name != NULL ? platform_name : "n/d",
             device_name != NULL ? device_name : "n/d");
    free(platform_name);
    free(device_name);

    /* Versione: decide quale API usare per la command queue (B2). */
    char *version = read_device_string(device, CL_DEVICE_VERSION);
    parse_device_version(version, out->label, &out->version_major, &out->version_minor, report);
    free(version);

    /* Limiti usati da F3/F4. */
    READ_LIMIT(CL_DEVICE_MAX_WORK_GROUP_SIZE, max_work_group_size);
    READ_LIMIT(CL_DEVICE_LOCAL_MEM_SIZE, local_mem_size);
    READ_LIMIT(CL_DEVICE_GLOBAL_MEM_SIZE, global_mem_size);
    READ_LIMIT(CL_DEVICE_MAX_MEM_ALLOC_SIZE, max_mem_alloc_size);

    /* MAX_WORK_ITEM_SIZES ha MAX_WORK_ITEM_DIMENSIONS elementi, che possono essere più di 3:
     * si legge l'array intero e si tengono le prime 3 voci (le mancanti valgono 1). */
    out->max_work_item_sizes[0] = out->max_work_item_sizes[1] = out->max_work_item_sizes[2] = 1;
    if (READ_LIMIT(CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, max_work_item_dims)) {
        if (out->max_work_item_dims == 0) {
            exclude_device(out, "CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS vale 0");
        } else {
            size_t *sizes = malloc(out->max_work_item_dims * sizeof(size_t));
            if (sizes == NULL) {
                return -1;
            }
            if (read_device_limit(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, "CL_DEVICE_MAX_WORK_ITEM_SIZES",
                                  out->max_work_item_dims * sizeof(size_t), sizes, out)) {
                for (cl_uint d = 0; d < out->max_work_item_dims && d < 3; d++) {
                    out->max_work_item_sizes[d] = sizes[d];
                }
            }
            free(sizes);
        }
    }

    /* Disponibilità e compilatore (servono per compilare i .cl dal sorgente). */
    cl_bool available = CL_FALSE;
    if (read_device_limit(device, CL_DEVICE_AVAILABLE, "CL_DEVICE_AVAILABLE",
                          sizeof(available), &available, out) && !available) {
        exclude_device(out, "non disponibile");
    }
    cl_bool compiler_available = CL_FALSE;
    if (read_device_limit(device, CL_DEVICE_COMPILER_AVAILABLE, "CL_DEVICE_COMPILER_AVAILABLE",
                          sizeof(compiler_available), &compiler_available, out) && !compiler_available) {
        exclude_device(out, "nessun compilatore");
    }

    return 0;
}

#undef READ_LIMIT

/* =========================================================================
 * F1 — interfaccia pubblica
 * ========================================================================= */

int bench_discover(bench_device **out, size_t *count, int report)
{
    *out = NULL;
    *count = 0;

    /* 1. Piattaforme. CL_PLATFORM_NOT_FOUND_KHR (-1001) arriva come errore (CA-9). */
    cl_uint platform_count = 0;
    cl_int err = clGetPlatformIDs(0, NULL, &platform_count);
    if (err != CL_SUCCESS || platform_count == 0) {
        if (report) {
            fprintf(stderr, "Nessuna piattaforma OpenCL trovata\n");
        }
        return -1;
    }

    cl_platform_id *platforms = malloc(platform_count * sizeof(cl_platform_id));
    if (platforms == NULL) {
        if (report) {
            fprintf(stderr, "Memoria insufficiente per l'elenco delle piattaforme\n");
        }
        return -1;
    }
    err = clGetPlatformIDs(platform_count, platforms, NULL);
    if (err != CL_SUCCESS) {
        if (report) {
            fprintf(stderr, "clGetPlatformIDs fallita (codice %d)\n", err);
        }
        free(platforms);
        return -1;
    }

    /* 2. Device di ogni piattaforma, accodati in un unico array. */
    bench_device *devices = NULL;
    size_t device_count = 0;
    int rc = 0;

    for (cl_uint p = 0; p < platform_count && rc == 0; p++) {
        cl_uint n = 0;
        err = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, 0, NULL, &n);
        if (err == CL_DEVICE_NOT_FOUND || (err == CL_SUCCESS && n == 0)) {
            if (report) {
                char *name = read_platform_string(platforms[p], CL_PLATFORM_NAME);
                fprintf(stderr, "Piattaforma %s: nessun device\n", name != NULL ? name : "n/d");
                free(name);
            }
            continue;
        }
        if (err != CL_SUCCESS) {
            if (report) {
                fprintf(stderr, "Piattaforma %u: clGetDeviceIDs fallita (codice %d)\n", p, err);
            }
            continue;
        }

        cl_device_id *ids = malloc(n * sizeof(cl_device_id));
        bench_device *grown = realloc(devices, (device_count + n) * sizeof(bench_device));
        if (ids == NULL || grown == NULL) {
            free(ids);
            rc = -1;   /* devices è ancora valido se è fallita solo la realloc */
            if (grown != NULL) {
                devices = grown;
            }
            if (report) {
                fprintf(stderr, "Memoria insufficiente per l'elenco dei device\n");
            }
            break;
        }
        devices = grown;

        err = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, n, ids, NULL);
        if (err != CL_SUCCESS) {
            if (report) {
                fprintf(stderr, "Piattaforma %u: clGetDeviceIDs fallita (codice %d)\n", p, err);
            }
            free(ids);
            continue;
        }

        for (cl_uint d = 0; d < n; d++) {
            if (describe_device(platforms[p], p, ids[d], d, &devices[device_count], report) != 0) {
                if (report) {
                    fprintf(stderr, "Memoria insufficiente durante la lettura dei device\n");
                }
                rc = -1;
                break;
            }
            device_count++;
        }
        free(ids);
    }

    free(platforms);

    if (rc != 0) {
        free(devices);
        return -1;
    }

    /* 3. Serve almeno un device utilizzabile; l'elenco si restituisce comunque per la stampa. */
    size_t usable = 0;
    for (size_t i = 0; i < device_count; i++) {
        if (!devices[i].excluded) {
            usable++;
        }
    }

    if (device_count == 0) {
        free(devices);
        devices = NULL;
    }
    *out = devices;
    *count = device_count;

    if (usable == 0) {
        if (report) {
            fprintf(stderr, "Nessun device OpenCL utilizzabile\n");
        }
        return -1;
    }
    return 0;
}

/* Stampa una riga "etichetta: valore" e libera il valore; NULL → "n/d". */
static void print_owned_string(const char *indent, const char *label, char *value)
{
    printf("%s%-28s: %s\n", indent, label, value != NULL ? value : "n/d");
    free(value);
}

static void print_device_uint(cl_device_id device, cl_device_info param, const char *label,
                              const char *unit)
{
    cl_uint value;
    if (clGetDeviceInfo(device, param, sizeof(value), &value, NULL) == CL_SUCCESS) {
        printf("    %-28s: %u%s\n", label, value, unit);
    } else {
        printf("    %-28s: n/d\n", label);
    }
}

static void print_device_size(cl_device_id device, cl_device_info param, const char *label,
                              const char *unit)
{
    size_t value;
    if (clGetDeviceInfo(device, param, sizeof(value), &value, NULL) == CL_SUCCESS) {
        printf("    %-28s: %zu%s\n", label, value, unit);
    } else {
        printf("    %-28s: n/d\n", label);
    }
}

/* CL_DEVICE_TYPE è una maschera di bit: si riporta il primo tipo riconosciuto. */
static const char *device_type_to_string(cl_device_id device)
{
    cl_device_type type;
    if (clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(type), &type, NULL) != CL_SUCCESS) {
        return "n/d";
    }
    if (type & CL_DEVICE_TYPE_GPU)         return "GPU";
    if (type & CL_DEVICE_TYPE_CPU)         return "CPU";
    if (type & CL_DEVICE_TYPE_ACCELERATOR) return "ACCELERATOR";
    if (type & CL_DEVICE_TYPE_CUSTOM)      return "CUSTOM";
    return "ALTRO";
}

void bench_devices_print(const bench_device *devices, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        const bench_device *d = &devices[i];

        /* I device arrivano in ordine di piattaforma: intestazione al cambio di piattaforma. */
        if (i == 0 || d->platform_index != devices[i - 1].platform_index) {
            printf("\n=======================================================\n");
            printf("Piattaforma %u\n", d->platform_index);
            print_owned_string("  ", "Nome", read_platform_string(d->platform, CL_PLATFORM_NAME));
            print_owned_string("  ", "Vendor", read_platform_string(d->platform, CL_PLATFORM_VENDOR));
            print_owned_string("  ", "Versione", read_platform_string(d->platform, CL_PLATFORM_VERSION));
            printf("=======================================================\n");
        }

        printf("\n  --- Device %u [%s] ---\n", d->device_index, device_type_to_string(d->device));
        if (d->excluded) {
            printf("    escluso: %s\n", d->exclusion_reason);
        }

        print_owned_string("    ", "Nome", read_device_string(d->device, CL_DEVICE_NAME));
        print_owned_string("    ", "Vendor", read_device_string(d->device, CL_DEVICE_VENDOR));
        print_owned_string("    ", "Versione driver", read_device_string(d->device, CL_DRIVER_VERSION));
        print_owned_string("    ", "Versione OpenCL", read_device_string(d->device, CL_DEVICE_VERSION));
        print_owned_string("    ", "Versione OpenCL C", read_device_string(d->device, CL_DEVICE_OPENCL_C_VERSION));

        print_device_uint(d->device, CL_DEVICE_MAX_COMPUTE_UNITS, "Compute unit", "");
        print_device_uint(d->device, CL_DEVICE_MAX_CLOCK_FREQUENCY, "Frequenza massima", " MHz");
        print_device_size(d->device, CL_DEVICE_PROFILING_TIMER_RESOLUTION, "Risoluzione timer profiling", " ns");

        /* Limiti già letti da describe_device (usati da F3/F4). */
        printf("    %-28s: %zu\n", "Max work-group", d->max_work_group_size);
        printf("    %-28s: %zu x %zu x %zu (%u dimensioni)\n", "Max work-item sizes",
               d->max_work_item_sizes[0], d->max_work_item_sizes[1], d->max_work_item_sizes[2],
               d->max_work_item_dims);
        printf("    %-28s: %llu KB\n", "Local memory",
               (unsigned long long)(d->local_mem_size / 1024));
        printf("    %-28s: %llu MB\n", "Global memory",
               (unsigned long long)(d->global_mem_size / (1024 * 1024)));
        printf("    %-28s: %llu MB\n", "Max mem alloc",
               (unsigned long long)(d->max_mem_alloc_size / (1024 * 1024)));
    }
    printf("\n");
}

void bench_devices_free(bench_device *devices)
{
    free(devices);
}

/* =========================================================================
 * F2 — Helper per i test
 * ========================================================================= */

/* Codici OpenCL che indicano memoria insufficiente (RB-7). */
static int is_memory_error(cl_int err)
{
    return err == CL_MEM_OBJECT_ALLOCATION_FAILURE || err == CL_OUT_OF_HOST_MEMORY ||
           err == CL_INVALID_BUFFER_SIZE || err == CL_OUT_OF_RESOURCES;
}

int bench_buffer_create(bench_run *run, cl_mem_flags flags, size_t bytes, void *host_ptr, cl_mem *out)
{
    cl_int err;
    *out = clCreateBuffer(run->context, flags, bytes, host_ptr, &err);
    if (err == CL_SUCCESS) {
        return BENCH_OK;
    }
    *out = NULL;
    if (is_memory_error(err)) {
        return BENCH_ERR_MEMORY;
    }
    fprintf(stderr, "clCreateBuffer fallita (codice %d)\n", err);
    return BENCH_ERR;
}

int bench_buffer_read(bench_run *run, cl_mem mem, size_t bytes, void *dst)
{
    cl_int err = clEnqueueReadBuffer(run->queue, mem, CL_TRUE, 0, bytes, dst, 0, NULL, NULL);
    if (err == CL_SUCCESS) {
        return BENCH_OK;
    }
    if (is_memory_error(err)) {
        return BENCH_ERR_MEMORY;
    }
    fprintf(stderr, "clEnqueueReadBuffer fallita (codice %d)\n", err);
    return BENCH_ERR;
}

uint32_t bench_random_next(uint32_t *state)
{
    uint32_t x = *state != 0 ? *state : 1u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void bench_fill_random_float(float *dst, size_t n, uint32_t seed, float min, float max)
{
    uint32_t state = seed;
    for (size_t i = 0; i < n; i++) {
        float unit = (float)(bench_random_next(&state) >> 8) * (1.0f / 16777216.0f);  /* [0, 1) */
        dst[i] = min + (max - min) * unit;
    }
}

int bench_float_close(double expected, double got, double rel_tol, double abs_tol)
{
    double diff = fabs(got - expected);
    return diff <= abs_tol + rel_tol * fabs(expected);   /* falso anche se uno dei due è NaN */
}

int bench_compare_float(const float *expected, const float *got, size_t n,
                        double rel_tol, double abs_tol, bench_mismatch *mismatch)
{
    for (size_t i = 0; i < n; i++) {
        if (!bench_float_close(expected[i], got[i], rel_tol, abs_tol)) {
            mismatch->index = i;
            mismatch->expected = expected[i];
            mismatch->got = got[i];
            return 1;
        }
    }
    return 0;
}

/* =========================================================================
 * F2 — Validazione dei descrittori
 * ========================================================================= */

/*
 * Controlla un descrittore prima di eseguire qualsiasi test (F2, casi di errore).
 * @return 1 se valido, 0 se no (reason compilato).
 */
int bench_validate_test(const bench_test *const *tests, size_t index, char *reason, size_t len)
{
    const bench_test *t = tests[index];

    if (t == NULL)                                     { snprintf(reason, len, "descrittore NULL"); return 0; }
    if (t->name == NULL || t->name[0] == '\0')         { snprintf(reason, len, "campo name mancante"); return 0; }
    if (t->source_path == NULL || !t->source_path[0])  { snprintf(reason, len, "campo source_path mancante"); return 0; }
    if (t->kernel_name == NULL || !t->kernel_name[0])  { snprintf(reason, len, "campo kernel_name mancante"); return 0; }
    if (t->work_dim < 1 || t->work_dim > 3)            { snprintf(reason, len, "work_dim %u fuori da 1-3", t->work_dim); return 0; }
    for (cl_uint d = 0; d < t->work_dim; d++) {
        if (t->ref_size[d] == 0)                       { snprintf(reason, len, "ref_size[%u] vale 0", d); return 0; }
    }
    if (t->timeout_s < 0)                              { snprintf(reason, len, "timeout_s negativo"); return 0; }
    if (t->setup == NULL)                              { snprintf(reason, len, "funzione setup mancante"); return 0; }
    if (t->set_args == NULL)                           { snprintf(reason, len, "funzione set_args mancante"); return 0; }
    if (t->verify == NULL)                             { snprintf(reason, len, "funzione verify mancante"); return 0; }
    if (t->flop == NULL)                               { snprintf(reason, len, "funzione flop mancante"); return 0; }
    if (t->teardown == NULL)                           { snprintf(reason, len, "funzione teardown mancante"); return 0; }

    for (size_t j = 0; j < index; j++) {
        if (tests[j] != NULL && tests[j]->name != NULL && strcmp(tests[j]->name, t->name) == 0) {
            snprintf(reason, len, "nome duplicato");
            return 0;
        }
    }
    return 1;
}

/* =========================================================================
 * F3 — Generazione delle configurazioni
 * ========================================================================= */

static void add_discard(bench_discard_summary *summary, const char *reason)
{
    summary->discarded++;
    for (size_t i = 0; i < summary->reason_count; i++) {
        if (strcmp(summary->reasons[i].reason, reason) == 0) {
            summary->reasons[i].count++;
            return;
        }
    }
    if (summary->reason_count < BENCH_MAX_DISCARD_REASONS) {
        summary->reasons[summary->reason_count].count = 1;
        snprintf(summary->reasons[summary->reason_count].reason, BENCH_REASON_MAX, "%s", reason);
        summary->reason_count++;
    }
}

/* Valuta una forma candidata: la scarta (con motivo) o la accoda. */
static int consider_shape(const bench_test *test, const bench_device *device, cl_ulong static_local_mem,
                          const size_t local[3], bench_config **configs, size_t *count, size_t *capacity,
                          bench_discard_summary *summary)
{
    if (test->local_mem_bytes != NULL) {
        cl_ulong needed = (cl_ulong)test->local_mem_bytes(local) + static_local_mem;
        if (needed > device->local_mem_size) {
            add_discard(summary, "local memory insufficiente");
            return BENCH_OK;
        }
    }

    if (*count == *capacity) {
        size_t new_capacity = *capacity != 0 ? *capacity * 2 : 32;
        bench_config *grown = realloc(*configs, new_capacity * sizeof(bench_config));
        if (grown == NULL) {
            return BENCH_ERR_MEMORY;
        }
        *configs = grown;
        *capacity = new_capacity;
    }

    bench_config *c = &(*configs)[(*count)++];
    double ref_total = 1.0, eff_total = 1.0;
    for (int d = 0; d < 3; d++) {
        c->local[d] = local[d];
        if ((cl_uint)d < test->work_dim) {
            /* RB-11: multiplo più grande di local che non supera il riferimento. */
            c->eff[d] = (test->ref_size[d] / local[d]) * local[d];
            ref_total *= (double)test->ref_size[d];
            eff_total *= (double)c->eff[d];
        } else {
            c->eff[d] = 1;
        }
    }
    c->discarded_pct = 1.0 - eff_total / ref_total;
    summary->generated++;
    return BENCH_OK;
}

int bench_configs_generate(const bench_test *test, const bench_device *device, cl_kernel kernel,
                           bench_config **configs, size_t *count,
                           bench_discard_summary *summary,
                           char *skip_reason, size_t skip_reason_len)
{
    *configs = NULL;
    *count = 0;
    memset(summary, 0, sizeof(*summary));

    if (test->work_dim > device->max_work_item_dims) {
        snprintf(skip_reason, skip_reason_len, "il device supporta %u dimensioni, il test ne usa %u",
                 device->max_work_item_dims, test->work_dim);
        return BENCH_ERR;
    }

    size_t kernel_max = 0;
    cl_int err = clGetKernelWorkGroupInfo(kernel, device->device, CL_KERNEL_WORK_GROUP_SIZE,
                                          sizeof(kernel_max), &kernel_max, NULL);
    if (err != CL_SUCCESS || kernel_max == 0) {
        snprintf(skip_reason, skip_reason_len, "clGetKernelWorkGroupInfo(CL_KERNEL_WORK_GROUP_SIZE) fallita (codice %d)", err);
        return BENCH_ERR;
    }

    cl_ulong static_local_mem = 0;
    err = clGetKernelWorkGroupInfo(kernel, device->device, CL_KERNEL_LOCAL_MEM_SIZE,
                                   sizeof(static_local_mem), &static_local_mem, NULL);
    if (err != CL_SUCCESS) {
        snprintf(skip_reason, skip_reason_len, "clGetKernelWorkGroupInfo(CL_KERNEL_LOCAL_MEM_SIZE) fallita (codice %d)", err);
        return BENCH_ERR;
    }

    /* Dimensione minima: multiplo preferito del kernel; se non disponibile, 1 con avviso. */
    size_t p = 0;
    err = clGetKernelWorkGroupInfo(kernel, device->device, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                                   sizeof(p), &p, NULL);
    if (err != CL_SUCCESS || p == 0) {
        fprintf(stderr, "Avviso: multiplo preferito non disponibile per %s su %s, uso 1\n",
                test->name, device->label);
        p = 1;
    }

    size_t capacity = 0;
    int rc = BENCH_OK;
    size_t limit[3];
    for (int d = 0; d < 3; d++) {
        limit[d] = 1;
        if ((cl_uint)d < test->work_dim) {
            limit[d] = device->max_work_item_sizes[d];
            if (limit[d] > test->ref_size[d]) limit[d] = test->ref_size[d];
            if (limit[d] > kernel_max)        limit[d] = kernel_max;
        }
    }

    if (test->work_dim == 1) {
        /* 1D: tutti i multipli di p (RB-2). */
        for (size_t l = p; l <= limit[0] && rc == BENCH_OK; l += p) {
            size_t local[3] = { l, 1, 1 };
            rc = consider_shape(test, device, static_local_mem, local, configs, count, &capacity, summary);
        }
    } else {
        /* 2D/3D: lati potenze di 2, prodotto multiplo di p entro il massimo del kernel (RB-2). */
        for (size_t z = 1; z <= limit[2] && rc == BENCH_OK; z *= 2) {
            for (size_t y = 1; y <= limit[1] && rc == BENCH_OK; y *= 2) {
                for (size_t x = 1; x <= limit[0] && rc == BENCH_OK; x *= 2) {
                    size_t product = x * y * z;
                    if (product > kernel_max || product % p != 0) {
                        continue;
                    }
                    if (test->shape == BENCH_SHAPE_EQUAL &&
                        (x != y || (test->work_dim == 3 && y != z))) {
                        continue;
                    }
                    size_t local[3] = { x, y, z };
                    rc = consider_shape(test, device, static_local_mem, local, configs, count, &capacity, summary);
                }
            }
        }
    }

    if (rc != BENCH_OK) {
        free(*configs);
        *configs = NULL;
        *count = 0;
        snprintf(skip_reason, skip_reason_len, "memoria insufficiente");
    }
    return rc;
}

/* =========================================================================
 * F4 — Esecuzione e misura
 * ========================================================================= */

/* Legge un file di testo intero (sorgente .cl). NULL se non esiste o non è leggibile. */
static char *read_text_file(const char *path, size_t *length)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    char *buffer = NULL;
    long size = -1;
    if (fseek(f, 0, SEEK_END) == 0) {
        size = ftell(f);
    }
    if (size >= 0 && fseek(f, 0, SEEK_SET) == 0) {
        buffer = malloc((size_t)size + 1);
        if (buffer != NULL && fread(buffer, 1, (size_t)size, f) != (size_t)size) {
            free(buffer);
            buffer = NULL;
        }
    }
    fclose(f);
    if (buffer != NULL) {
        buffer[size] = '\0';
        *length = (size_t)size;
    }
    return buffer;
}

static void print_build_log(cl_program program, const bench_test *test, const bench_device *device)
{
    size_t size = 0;
    char *log = NULL;
    if (clGetProgramBuildInfo(program, device->device, CL_PROGRAM_BUILD_LOG, 0, NULL, &size) == CL_SUCCESS &&
        size > 0 && (log = malloc(size)) != NULL &&
        clGetProgramBuildInfo(program, device->device, CL_PROGRAM_BUILD_LOG, size, log, NULL) == CL_SUCCESS) {
        fprintf(stderr, "Log di compilazione di %s su %s:\n%s\n", test->name, device->label, log);
    } else {
        fprintf(stderr, "Log di compilazione di %s su %s non disponibile\n", test->name, device->label);
    }
    free(log);
}

static void report_skip(const bench_reporter *rep, const char *format, ...)
{
    char reason[BENCH_REASON_MAX];
    va_list args;
    va_start(args, format);
    vsnprintf(reason, sizeof(reason), format, args);
    va_end(args);
    rep->skip(rep->ctx, reason);
}

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

typedef enum { MEASURE_CONTINUE, MEASURE_STOP } measure_outcome;

/*
 * Controllo di plausibilità del profiling (RB-3, Q-40): alcuni driver (OpenCLOn12) restituiscono
 * timestamp errati. Si decide una volta per coppia test/device alla prima esecuzione misurata:
 * se l'host misura almeno PLAUSIBILITY_MIN_HOST_NS e il profiling è meno di 1/PLAUSIBILITY_RATIO
 * del tempo host, il profiling è inaffidabile e si usa il tempo host (enqueue → fine attesa).
 * Sotto la soglia di tempo host l'overhead domina e il confronto non è significativo.
 */
#define PLAUSIBILITY_RATIO       50.0
#define PLAUSIBILITY_MIN_HOST_NS 500000.0   /* 0,5 ms */

typedef enum { TIMING_UNKNOWN, TIMING_PROFILING, TIMING_HOST } timing_mode;

/* Misura una configurazione: setup, 1 a vuoto + N misurate, mediana, verifica, teardown (F4 punto 5). */
static measure_outcome measure_config(const bench_test *test, const bench_options *options,
                                      const bench_reporter *rep, cl_context context, cl_command_queue queue,
                                      cl_kernel kernel, cl_device_id device,
                                      const bench_config *config,
                                      const char *device_label, timing_mode *timing)
{
    bench_run run = {
        .context = context, .queue = queue, .kernel = kernel, .device = device,
        .work_dim = test->work_dim, .user = test->user, .state = NULL,
    };
    memcpy(run.eff_size, config->eff, sizeof(run.eff_size));
    memcpy(run.local_size, config->local, sizeof(run.local_size));

    measure_outcome outcome = MEASURE_STOP;
    int measured = options->measured_runs > 0 ? options->measured_runs : 1;
    int warmup = options->warmup_runs > 0 ? options->warmup_runs : 0;
    double *times = malloc((size_t)measured * sizeof(double));
    if (times == NULL) {
        report_skip(rep, "memoria insufficiente");
        return MEASURE_STOP;
    }

    int rc = test->setup(&run);
    if (rc != BENCH_OK) {
        report_skip(rep, rc == BENCH_ERR_MEMORY ? "memoria insufficiente" : "errore nella preparazione dei dati");
        goto done;
    }
    if (test->set_args(&run) != BENCH_OK) {
        report_skip(rep, "errore nell'impostazione degli argomenti del kernel");
        goto done;
    }

    for (int r = 0; r < warmup + measured; r++) {
        cl_event event = NULL;
        rep->run_start(rep->ctx, config);
        double host_start = bench_clock_ns();
        cl_int err = clEnqueueNDRangeKernel(queue, kernel, test->work_dim, NULL,
                                            run.eff_size, run.local_size, 0, NULL, &event);
        if (err != CL_SUCCESS) {
            rep->run_end(rep->ctx, config);
            if (is_memory_error(err)) {
                report_skip(rep, "memoria insufficiente (codice %d)", err);
                goto done;
            }
            char reason[BENCH_REASON_MAX];
            snprintf(reason, sizeof(reason), "rifiutata da clEnqueueNDRangeKernel (codice %d)", err);
            rep->discard(rep->ctx, config, reason);
            outcome = MEASURE_CONTINUE;
            goto done;
        }

        err = clWaitForEvents(1, &event);
        double host_ns = bench_clock_ns() - host_start;
        cl_int status = CL_COMPLETE;
        if (err == CL_SUCCESS) {
            err = clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(status), &status, NULL);
        }
        rep->run_end(rep->ctx, config);
        if (err != CL_SUCCESS || status < 0) {
            /* Errore durante l'esecuzione (TDR, device perso): conta come TIMEOUT (RB-10, Q-26). */
            clReleaseEvent(event);
            rep->timeout_os(rep->ctx, config);
            goto done;
        }

        cl_ulong start = 0, end = 0;
        err = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(start), &start, NULL);
        if (err == CL_SUCCESS) {
            err = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(end), &end, NULL);
        }
        if (err != CL_SUCCESS) {
            clReleaseEvent(event);
            report_skip(rep, "profiling non disponibile (codice %d)", err);
            goto done;
        }
        double profiling_ns = (double)(end - start);

        /* Si decide alla prima esecuzione misurata: in quella a vuoto il tempo host include
         * il trasferimento iniziale dei buffer e falserebbe il confronto. */
        if (*timing == TIMING_UNKNOWN && r >= warmup) {
            if (host_ns >= PLAUSIBILITY_MIN_HOST_NS && profiling_ns * PLAUSIBILITY_RATIO < host_ns) {
                *timing = TIMING_HOST;
                fprintf(stderr, "Avviso: %s: profiling inaffidabile (%.4f ms contro %.4f ms misurati dall'host), "
                        "uso il tempo host\n", device_label, profiling_ns / 1e6, host_ns / 1e6);
            } else {
                *timing = TIMING_PROFILING;
            }
        }
        if (r >= warmup) {
            times[r - warmup] = (*timing == TIMING_HOST) ? host_ns : profiling_ns;
        }
        clReleaseEvent(event);
    }

    qsort(times, (size_t)measured, sizeof(double), compare_double);
    double median = (measured % 2 == 1)
        ? times[measured / 2]
        : (times[measured / 2 - 1] + times[measured / 2]) / 2.0;

    bench_result result = {
        .work_dim = test->work_dim,
        .discarded_pct = config->discarded_pct,
        .median_ns = median,
        .flop = test->flop(run.eff_size),
        .status = BENCH_STATUS_OK,
        .host_timing = (*timing == TIMING_HOST),
    };
    memcpy(result.local, config->local, sizeof(result.local));
    memcpy(result.eff, config->eff, sizeof(result.eff));

    rc = test->verify(&run, &result.mismatch);
    if (rc < 0) {
        report_skip(rep, rc == BENCH_ERR_MEMORY ? "memoria insufficiente nella verifica" : "errore nella verifica");
        goto done;
    }
    if (rc == 1) {
        result.status = BENCH_STATUS_WRONG;
    }
    rep->result(rep->ctx, &result);
    outcome = MEASURE_CONTINUE;

done:
    test->teardown(&run);   /* sempre, anche dopo un setup fallito a metà */
    free(times);
    return outcome;
}

void bench_execute_pair(const bench_test *test, const bench_device *device,
                        const bench_options *options, const bench_reporter *rep)
{
    cl_context context = NULL;
    cl_command_queue queue = NULL;
    cl_program program = NULL;
    cl_kernel kernel = NULL;
    char *source = NULL;
    bench_config *configs = NULL;
    size_t config_count = 0;
    timing_mode timing = TIMING_UNKNOWN;
    cl_int err;

    context =clCreateContext(NULL, 1, &device->device, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        report_skip(rep, "clCreateContext fallita (codice %d)", err);
        goto cleanup;
    }

    /* B2: API della queue scelta dalla versione del device; profiling sempre attivo (RB-3). */
    if (device->version_major >= 2) {
        cl_queue_properties properties[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };
        queue = clCreateCommandQueueWithProperties(context, device->device, properties, &err);
    } else {
        queue = clCreateCommandQueue(context, device->device, CL_QUEUE_PROFILING_ENABLE, &err);
    }
    if (err != CL_SUCCESS) {
        report_skip(rep, "creazione della command queue fallita (codice %d)", err);
        goto cleanup;
    }

    size_t source_length = 0;
    source = read_text_file(test->source_path, &source_length);
    if (source == NULL) {
        report_skip(rep, "file non trovato: %s", test->source_path);
        goto cleanup;
    }
    const char *sources[] = { source };
    program = clCreateProgramWithSource(context, 1, sources, &source_length, &err);
    if (err != CL_SUCCESS) {
        report_skip(rep, "clCreateProgramWithSource fallita (codice %d)", err);
        goto cleanup;
    }
    err = clBuildProgram(program, 1, &device->device, test->build_options, NULL, NULL);
    if (err != CL_SUCCESS) {
        print_build_log(program, test, device);
        report_skip(rep, "compilazione fallita (codice %d)", err);
        goto cleanup;
    }
    kernel = clCreateKernel(program, test->kernel_name, &err);
    if (err != CL_SUCCESS) {
        report_skip(rep, "kernel %s non creabile (codice %d)", test->kernel_name, err);
        goto cleanup;
    }

    bench_discard_summary summary;
    char reason[BENCH_REASON_MAX];
    if (bench_configs_generate(test, device, kernel, &configs, &config_count,
                               &summary, reason, sizeof(reason)) != BENCH_OK) {
        report_skip(rep, "%s", reason);
        goto cleanup;
    }
    rep->configs(rep->ctx, &summary);
    if (config_count == 0) {
        report_skip(rep, "nessuna configurazione valida");
        goto cleanup;
    }

    for (size_t c = 0; c < config_count; c++) {
        if (measure_config(test, options, rep, context, queue, kernel, device->device,
                           &configs[c], device->label, &timing) == MEASURE_STOP) {
            break;
        }
    }

cleanup:
    free(configs);
    free(source);
    if (kernel != NULL)  clReleaseKernel(kernel);
    if (program != NULL) clReleaseProgram(program);
    if (queue != NULL)   clReleaseCommandQueue(queue);
    if (context != NULL) clReleaseContext(context);
}

/* =========================================================================
 * Formattazione all'italiana (F6 [D]: funzione propria, non il locale di sistema)
 * ========================================================================= */

/* Intero con separatore delle migliaia: 16777216 → "16.777.216". */
static void format_count(char *out, size_t len, unsigned long long value)
{
    char digits[32];
    int n = snprintf(digits, sizeof(digits), "%llu", value);
    size_t pos = 0;
    for (int i = 0; i < n && pos + 1 < len; i++) {
        if (i > 0 && (n - i) % 3 == 0 && pos + 1 < len) {
            out[pos++] = '.';
        }
        out[pos++] = digits[i];
    }
    out[pos] = '\0';
}

/* Decimale con migliaia e virgola: 1234.5678, 2 → "1.234,57". */
static void format_decimal(char *out, size_t len, double value, int decimals)
{
    char raw[64];
    snprintf(raw, sizeof(raw), "%.*f", decimals, value < 0 ? -value : value);
    char *dot = strchr(raw, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
    char integer[32];
    format_count(integer, sizeof(integer), strtoull(raw, NULL, 10));

    /* Composizione a mano: segno, parte intera, virgola, decimali. */
    size_t pos = 0;
    const char *parts[4] = { value < 0 ? "-" : "", integer, dot != NULL ? "," : "", dot != NULL ? dot + 1 : "" };
    for (int i = 0; i < 4; i++) {
        for (const char *ch = parts[i]; *ch != '\0' && pos + 1 < len; ch++) {
            out[pos++] = *ch;
        }
    }
    out[pos] = '\0';
}

/* Percentuale scartata da una frazione: 3.8147e-6 → "0,00038". */
static void format_percent(char *out, size_t len, double fraction)
{
    double pct = fraction * 100.0;
    if (pct <= 0.0) {
        snprintf(out, len, "0");
    } else if (pct >= 1.0) {
        format_decimal(out, len, pct, 2);
    } else {
        snprintf(out, len, "%.2g", pct);
        char *dot = strchr(out, '.');
        if (dot != NULL) {
            *dot = ',';
        }
    }
}

static void format_ms(char *out, size_t len, double ns)
{
    char number[32];
    format_decimal(number, sizeof(number), ns / 1e6, 4);
    snprintf(out, len, "%s ms", number);
}

/* FLOPS con unità scalata (F6 [D]). */
static void format_flops(char *out, size_t len, double flops)
{
    const char *unit = "MFLOPS";
    double value = flops / 1e6;
    if (flops >= 1e12) {
        unit = "TFLOPS";
        value = flops / 1e12;
    } else if (flops >= 1e9) {
        unit = "GFLOPS";
        value = flops / 1e9;
    }
    char number[32];
    format_decimal(number, sizeof(number), value, 2);
    snprintf(out, len, "%s %s", number, unit);
}

/* Forma di un work-group o di una dimensione: "96", "16x8", "4x4x4". */
static void format_shape(char *out, size_t len, const size_t v[3], cl_uint work_dim)
{
    if (work_dim == 1) {
        snprintf(out, len, "%zu", v[0]);
    } else if (work_dim == 2) {
        snprintf(out, len, "%zux%zu", v[0], v[1]);
    } else {
        snprintf(out, len, "%zux%zux%zu", v[0], v[1], v[2]);
    }
}

static double result_flops(const bench_result *r)
{
    return r->median_ns > 0 ? r->flop / (r->median_ns * 1e-9) : 0.0;
}

/* =========================================================================
 * Raccolta dei risultati (padre) e riepilogo (F6)
 * ========================================================================= */

typedef struct {
    int           skipped;
    char          skip_reason[BENCH_REASON_MAX];
    size_t        timeouts;
    size_t        discarded;
    bench_result *results;
    size_t        result_count;
    size_t        result_capacity;
} pair_outcome;

typedef struct {
    const bench_test *const *tests;
    size_t                   test_count;
    const int               *test_valid;
    const char             (*test_reasons)[BENCH_REASON_MAX];
    const bench_device      *devices;
    size_t                   device_count;
    pair_outcome            *pairs;          /* [test * device_count + device] */
    size_t                   current_test;
    size_t                   current_device;
    int                      incomplete;     /* allocazione fallita durante la raccolta */
} collector;

static pair_outcome *current_pair(collector *c)
{
    return &c->pairs[c->current_test * c->device_count + c->current_device];
}

static const char *current_label(const collector *c)
{
    return c->devices[c->current_device].label;
}

static void collector_configs(void *ctx, const bench_discard_summary *summary)
{
    collector *c = ctx;
    current_pair(c)->discarded += summary->discarded;
    printf("  %zu configurazioni generate, %zu scartate\n", summary->generated, summary->discarded);
    for (size_t i = 0; i < summary->reason_count; i++) {
        printf("    - %zu scartate: %s\n", summary->reasons[i].count, summary->reasons[i].reason);
    }
}

static void collector_run_event(void *ctx, const bench_config *config)
{
    /* Il tempo massimo lo misura bench_supervise_pair sui messaggi del figlio. */
    (void)ctx;
    (void)config;
}

static void collector_result(void *ctx, const bench_result *r)
{
    collector *c = ctx;
    pair_outcome *pair = current_pair(c);
    const bench_test *test = c->tests[c->current_test];

    if (pair->result_count == pair->result_capacity) {
        size_t capacity = pair->result_capacity != 0 ? pair->result_capacity * 2 : 16;
        bench_result *grown = realloc(pair->results, capacity * sizeof(bench_result));
        if (grown == NULL) {
            c->incomplete = 1;
        } else {
            pair->results = grown;
            pair->result_capacity = capacity;
        }
    }
    if (pair->result_count < pair->result_capacity) {
        pair->results[pair->result_count++] = *r;
    }

    char shape[64], processed[48], total[48], pct[32], time[48], flops[48];
    size_t ref[3] = { test->ref_size[0], test->ref_size[1], test->ref_size[2] };
    unsigned long long eff_total = 1, ref_total = 1;
    for (cl_uint d = 0; d < r->work_dim; d++) {
        eff_total *= r->eff[d];
        ref_total *= ref[d];
    }
    format_shape(shape, sizeof(shape), r->local, r->work_dim);
    format_count(processed, sizeof(processed), eff_total);
    format_count(total, sizeof(total), ref_total);
    format_percent(pct, sizeof(pct), r->discarded_pct);
    format_ms(time, sizeof(time), r->median_ns);
    format_flops(flops, sizeof(flops), result_flops(r));

    printf("  [%s] %s  L=%s  dati %s/%s (scartati %s%%)  mediana %s  %s  ",
           current_label(c), test->name, shape, processed, total, pct, time, flops);
    if (r->host_timing) {
        printf("[tempo host: profiling inaffidabile]  ");
    }
    if (r->status == BENCH_STATUS_OK) {
        printf("OK\n");
    } else {
        printf("ERRATO (indice %zu: atteso %.9g, ottenuto %.9g)\n",
               r->mismatch.index, r->mismatch.expected, r->mismatch.got);
    }
}

static void collector_discard(void *ctx, const bench_config *config, const char *reason)
{
    collector *c = ctx;
    current_pair(c)->discarded++;
    char shape[64];
    format_shape(shape, sizeof(shape), config->local, c->tests[c->current_test]->work_dim);
    printf("  [%s] %s  L=%s  scartata: %s\n", current_label(c), c->tests[c->current_test]->name, shape, reason);
}

static void collector_timeout_os(void *ctx, const bench_config *config)
{
    collector *c = ctx;
    current_pair(c)->timeouts++;
    char shape[64];
    format_shape(shape, sizeof(shape), config->local, c->tests[c->current_test]->work_dim);
    printf("  [%s] %s  L=%s  TIMEOUT (fermato dal sistema operativo); configurazioni rimanenti saltate\n",
           current_label(c), c->tests[c->current_test]->name, shape);
}

static void collector_timeout(void *ctx, const bench_config *config, double limit_s)
{
    collector *c = ctx;
    current_pair(c)->timeouts++;
    char shape[64], limit[32];
    format_shape(shape, sizeof(shape), config->local, c->tests[c->current_test]->work_dim);
    format_decimal(limit, sizeof(limit), limit_s, 1);
    printf("  [%s] %s  L=%s  TIMEOUT (oltre %s s); configurazioni rimanenti saltate\n",
           current_label(c), c->tests[c->current_test]->name, shape, limit);
}

static void collector_skip(void *ctx, const char *reason)
{
    collector *c = ctx;
    pair_outcome *pair = current_pair(c);
    pair->skipped = 1;
    snprintf(pair->skip_reason, sizeof(pair->skip_reason), "%s", reason);
    printf("  [%s] %s  saltato: %s\n", current_label(c), c->tests[c->current_test]->name, reason);
}

/* Migliore misura valida di una coppia (RB-6): FLOPS più alti, a parità tempo minore. */
static const bench_result *best_result(const pair_outcome *pair)
{
    const bench_result *best = NULL;
    for (size_t i = 0; i < pair->result_count; i++) {
        const bench_result *r = &pair->results[i];
        if (r->status != BENCH_STATUS_OK) {
            continue;
        }
        if (best == NULL || result_flops(r) > result_flops(best) ||
            (result_flops(r) == result_flops(best) && r->median_ns < best->median_ns)) {
            best = r;
        }
    }
    return best;
}

static void print_best_line(const char *prefix, const char *name, const bench_test *test, const bench_result *r)
{
    char shape[64], flops[48], time[48], processed[48], pct[32];
    unsigned long long eff_total = 1;
    for (cl_uint d = 0; d < r->work_dim; d++) {
        eff_total *= r->eff[d];
    }
    (void)test;
    format_shape(shape, sizeof(shape), r->local, r->work_dim);
    format_flops(flops, sizeof(flops), result_flops(r));
    format_ms(time, sizeof(time), r->median_ns);
    format_count(processed, sizeof(processed), eff_total);
    format_percent(pct, sizeof(pct), r->discarded_pct);
    printf("%s%-55s L=%-10s %16s  %14s  dati %s (scartati %s%%)%s\n",
           prefix, name, shape, flops, time, processed, pct,
           r->host_timing ? "  [tempo host]" : "");
}

/* Motivo per cui una coppia non ha una misura valida. */
static const char *missing_reason(const pair_outcome *pair)
{
    if (pair->skipped)                         return pair->skip_reason;
    if (pair->timeouts > 0)                    return "TIMEOUT";
    if (pair->result_count > 0)                return "solo misure ERRATO";
    return "nessuna misura";
}

typedef struct {
    size_t              device;
    const bench_result *best;
} ranked_device;

static int compare_ranked(const void *a, const void *b)
{
    double fa = result_flops(((const ranked_device *)a)->best);
    double fb = result_flops(((const ranked_device *)b)->best);
    return (fa < fb) - (fa > fb);   /* decrescente */
}

static void print_summary(const collector *c)
{
    printf("\n#######################################################\n");
    printf("RIEPILOGO\n");
    printf("#######################################################\n");
    if (c->incomplete) {
        printf("Attenzione: riepilogo incompleto (memoria insufficiente durante la raccolta)\n");
    }

    /* 1. Per test: classifica dei device per FLOPS (Q-29). */
    printf("\n--- Per test ---\n");
    ranked_device *ranking = malloc((c->device_count > 0 ? c->device_count : 1) * sizeof(ranked_device));
    for (size_t t = 0; t < c->test_count; t++) {
        const bench_test *test = c->tests[t];
        printf("\n== %s ==\n", test != NULL && test->name != NULL ? test->name : "(senza nome)");
        if (!c->test_valid[t]) {
            printf("  escluso: %s\n", c->test_reasons[t]);
            continue;
        }
        size_t ranked = 0;
        for (size_t d = 0; d < c->device_count && ranking != NULL; d++) {
            const bench_result *best = best_result(&c->pairs[t * c->device_count + d]);
            if (best != NULL) {
                ranking[ranked].device = d;
                ranking[ranked].best = best;
                ranked++;
            }
        }
        if (ranked == 0) {
            printf("  nessun device valido\n");
            continue;
        }
        qsort(ranking, ranked, sizeof(ranked_device), compare_ranked);
        for (size_t i = 0; i < ranked; i++) {
            print_best_line(i == 0 ? "  * " : "    ", c->devices[ranking[i].device].label, test, ranking[i].best);
        }
    }
    free(ranking);

    /* 2. Per device: configurazione migliore di ciascun test. */
    printf("\n--- Per device ---\n");
    for (size_t d = 0; d < c->device_count; d++) {
        const bench_device *device = &c->devices[d];
        printf("\n== %s ==\n", device->label);
        if (device->excluded) {
            printf("  escluso: %s\n", device->exclusion_reason);
            continue;
        }
        for (size_t t = 0; t < c->test_count; t++) {
            if (!c->test_valid[t]) {
                continue;
            }
            const pair_outcome *pair = &c->pairs[t * c->device_count + d];
            const bench_result *best = best_result(pair);
            if (best != NULL) {
                print_best_line("  ", c->tests[t]->name, c->tests[t], best);
            } else {
                printf("  %-55s %s\n", c->tests[t]->name, missing_reason(pair));
            }
        }
    }

    /* 3. Conteggi. */
    size_t measured = 0, ok = 0, wrong = 0, timeouts = 0, discarded = 0, skipped = 0;
    for (size_t i = 0; i < c->test_count * c->device_count; i++) {
        const pair_outcome *pair = &c->pairs[i];
        measured += pair->result_count;
        for (size_t r = 0; r < pair->result_count; r++) {
            if (pair->results[r].status == BENCH_STATUS_OK) ok++; else wrong++;
        }
        timeouts += pair->timeouts;
        discarded += pair->discarded;
        skipped += pair->skipped ? 1 : 0;
    }
    printf("\n--- Conteggi ---\n");
    printf("  configurazioni misurate: %zu (OK %zu, ERRATO %zu), TIMEOUT %zu, scartate %zu\n",
           measured, ok, wrong, timeouts, discarded);
    printf("  coppie test/device saltate: %zu\n\n", skipped);
}

/* =========================================================================
 * Punto di ingresso
 * ========================================================================= */

int bench_main(int argc, char **argv,
               const bench_test *const *tests, size_t test_count,
               const bench_options *options)
{
    /* F5: lo stesso eseguibile, rilanciato con l'argomento interno, fa da processo figlio. */
    if (bench_is_child(argc, argv)) {
        return bench_child_main(argc, argv, tests, test_count, options);
    }

    /* F1 */
    bench_device *devices = NULL;
    size_t device_count = 0;
    int rc = bench_discover(&devices, &device_count, 1);
    if (devices != NULL) {
        bench_devices_print(devices, device_count);
    }
    if (rc != 0) {
        bench_devices_free(devices);
        return 1;
    }

    /* F2: validazione dei descrittori prima di eseguire qualsiasi test. */
    if (test_count == 0) {
        fprintf(stderr, "Nessun test registrato\n");
        bench_devices_free(devices);
        return 1;
    }
    int *test_valid = calloc(test_count, sizeof(int));
    char (*test_reasons)[BENCH_REASON_MAX] = calloc(test_count, sizeof(*test_reasons));
    pair_outcome *pairs = calloc(test_count * device_count, sizeof(pair_outcome));
    if (test_valid == NULL || test_reasons == NULL || pairs == NULL) {
        fprintf(stderr, "Memoria insufficiente\n");
        free(test_valid);
        free(test_reasons);
        free(pairs);
        bench_devices_free(devices);
        return 1;
    }

    size_t valid_count = 0;
    for (size_t t = 0; t < test_count; t++) {
        test_valid[t] = bench_validate_test(tests, t, test_reasons[t], BENCH_REASON_MAX);
        if (!test_valid[t]) {
            fprintf(stderr, "Test %zu (%s) escluso: %s\n", t,
                    tests[t] != NULL && tests[t]->name != NULL ? tests[t]->name : "senza nome",
                    test_reasons[t]);
        } else {
            valid_count++;
        }
    }
    if (valid_count == 0) {
        fprintf(stderr, "Nessun test valido\n");
    }

    collector col = {
        .tests = tests, .test_count = test_count,
        .test_valid = test_valid, .test_reasons = (const char (*)[BENCH_REASON_MAX])test_reasons,
        .devices = devices, .device_count = device_count,
        .pairs = pairs,
    };
    bench_reporter reporter = {
        .ctx = &col,
        .configs = collector_configs,
        .run_start = collector_run_event,
        .run_end = collector_run_event,
        .result = collector_result,
        .discard = collector_discard,
        .timeout_os = collector_timeout_os,
        .timeout = collector_timeout,
        .skip = collector_skip,
    };

    for (size_t t = 0; t < test_count; t++) {
        if (!test_valid[t]) {
            continue;
        }
        const bench_test *test = tests[t];

        /* F4: il file .cl si controlla una volta sola, per tutti i device (CA-8). */
        FILE *f = fopen(test->source_path, "rb");
        if (f == NULL) {
            fprintf(stderr, "Test %s: file non trovato: %s\n", test->name, test->source_path);
            for (size_t d = 0; d < device_count; d++) {
                pair_outcome *pair = &pairs[t * device_count + d];
                pair->skipped = 1;
                snprintf(pair->skip_reason, sizeof(pair->skip_reason), "file non trovato: %s", test->source_path);
            }
            continue;
        }
        fclose(f);

        for (size_t d = 0; d < device_count; d++) {
            if (devices[d].excluded) {
                continue;
            }
            col.current_test = t;
            col.current_device = d;
            printf("\n== %s su %s (mediana di %d esecuzioni, solo esecuzione kernel) ==\n",
                   test->name, devices[d].label, options->measured_runs);
            fflush(stdout);
            bench_supervise_pair(t, test, &devices[d], options, &reporter);
            fflush(stdout);
        }
    }

    /* F6 */
    print_summary(&col);

    for (size_t i = 0; i < test_count * device_count; i++) {
        free(pairs[i].results);
    }
    free(pairs);
    free(test_valid);
    free(test_reasons);
    bench_devices_free(devices);

    return valid_count > 0 ? 0 : 1;
}
