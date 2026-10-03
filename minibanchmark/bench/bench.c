/**
 * bench.c — implementazione della parte comune (F1, poi F3, F4, F6).
 *
 * Stato: passo 1 di B6, F1 implementata.
 * I riferimenti (A6, F1, RB-n, CA-n) puntano alla commissione.
 */

#include "bench.h"

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
 * Punto di ingresso
 * ========================================================================= */

int bench_main(int argc, char **argv,
               const bench_test *const *tests, size_t test_count,
               const bench_options *options)
{
    (void)argc; (void)argv; (void)tests; (void)test_count; (void)options;

    /* Passo 1 di B6: solo F1. I passi successivi aggiungono validazione (F2),
     * esecuzione (F4/F5) e riepilogo (F6). */
    bench_device *devices = NULL;
    size_t device_count = 0;

    int rc = bench_discover(&devices, &device_count, 1);
    if (devices != NULL) {
        bench_devices_print(devices, device_count);
    }
    bench_devices_free(devices);

    return rc == 0 ? 0 : 1;   /* F6: 1 solo per errori fatali */
}
