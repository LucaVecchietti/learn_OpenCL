/**
 * bench.h — parte comune portabile del benchmark OpenCL.
 *
 * Commissione: docs/commissioni/commissione_Refactor-minibanchmark.md
 *
 * Regola (RB-9): questo header non conosce nessun test specifico.
 * Per portarlo in un altro progetto si copiano bench/ e main.c,
 * e si scrivono i propri tests.h / tests.c.
 */

#ifndef BENCH_H
#define BENCH_H

/* B2 [P]: C11 obbligatorio. */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error "bench richiede C11: compilare con -std=c11"
#endif

/* B2 [P]: header OpenCL 3.0; la versione reale del device si legge a runtime.
 * CL_USE_DEPRECATED_OPENCL_1_2_APIS serve per clCreateCommandQueue (device 1.x). */
#define CL_TARGET_OPENCL_VERSION 300
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.h>

#include <stddef.h>

/* =========================================================================
 * F1 — Scoperta piattaforme e device
 * ========================================================================= */

#define BENCH_LABEL_MAX  256
#define BENCH_REASON_MAX 128

/**
 * Un device scoperto, con tutto ciò che serve a F3/F4.
 */
typedef struct {
    cl_uint        platform_index;      /**< indice nella lista di clGetPlatformIDs */
    cl_uint        device_index;        /**< indice nella lista di clGetDeviceIDs della piattaforma */
    cl_platform_id platform;
    cl_device_id   device;
    char           label[BENCH_LABEL_MAX];  /**< "nome piattaforma / nome device" */

    int            version_major;       /**< da CL_DEVICE_VERSION ("OpenCL <maj>.<min> ...") */
    int            version_minor;

    /* Limiti usati da F3/F4 */
    size_t         max_work_group_size;     /**< CL_DEVICE_MAX_WORK_GROUP_SIZE (size_t!) */
    cl_uint        max_work_item_dims;      /**< CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS */
    size_t         max_work_item_sizes[3];  /**< prime 3 voci di CL_DEVICE_MAX_WORK_ITEM_SIZES */
    cl_ulong       local_mem_size;
    cl_ulong       global_mem_size;
    cl_ulong       max_mem_alloc_size;

    int            excluded;                        /**< 1 = nessun test gira su questo device */
    char           exclusion_reason[BENCH_REASON_MAX];
} bench_device;

/**
 * Enumera tutte le piattaforme e tutti i device (CL_DEVICE_TYPE_ALL).
 *
 * @param out     Riceve un array allocato di device (liberare con bench_devices_free).
 * @param count   Riceve il numero di elementi di *out.
 * @param report  1 = segnala su stderr piattaforme senza device e avvisi (padre);
 *                0 = silenziosa (processo figlio).
 *
 * @return 0 se c'è almeno un device utilizzabile; negativo altrimenti
 *         (nessuna piattaforma, nessun device utilizzabile, errore di allocazione).
 */
int bench_discover(bench_device **out, size_t *count, int report);

/**
 * Stampa su stdout piattaforme e device con le loro caratteristiche
 * (stile di src/platform_info.c). I device esclusi compaiono con "escluso: motivo".
 */
void bench_devices_print(const bench_device *devices, size_t count);

/** Libera l'array restituito da bench_discover. Accetta NULL. */
void bench_devices_free(bench_device *devices);

/* =========================================================================
 * F2 — Modello di test (contratto; implementazione al passo 2 di B6)
 * ========================================================================= */

typedef enum {
    BENCH_SHAPE_ANY = 0,    /**< qualsiasi forma */
    BENCH_SHAPE_EQUAL       /**< tutti i lati uguali (quadrate / cubiche) */
} bench_shape;

/** Primo elemento diverso trovato dalla verifica (RB-5). */
typedef struct {
    size_t index;
    double expected;
    double got;
} bench_mismatch;

/**
 * Contesto di una configurazione, passato alle funzioni del test.
 * [D] pubblico per ora; i campi diversi da `state` sono in sola lettura per il test.
 */
typedef struct {
    cl_context       context;
    cl_command_queue queue;
    cl_kernel        kernel;
    cl_device_id     device;
    cl_uint          work_dim;
    size_t           eff_size[3];    /**< dimensione effettiva (RB-11) */
    size_t           local_size[3];  /**< work-group della configurazione */
    void            *state;          /**< di proprietà del test: creato in setup, liberato in teardown */
} bench_run;

typedef struct bench_test bench_test;

struct bench_test {
    const char *name;
    const char *source_path;
    const char *kernel_name;
    const char *build_options;       /**< NULL = nessuna */
    cl_uint     work_dim;            /**< 1, 2 o 3 */
    size_t      ref_size[3];         /**< dimensione di riferimento (RB-11) */
    bench_shape shape;
    double      timeout_s;           /**< 0 = valore globale */
    const void *user;                /**< dati statici del test */

    int    (*setup)(bench_run *run);
    int    (*set_args)(bench_run *run);
    size_t (*local_mem_bytes)(const size_t local_size[3]);  /**< NULL se non usa local memory */
    int    (*verify)(bench_run *run, bench_mismatch *mismatch); /**< 0 ok, 1 ERRATO, <0 errore */
    double (*flop)(const size_t eff_size[3]);
    void   (*teardown)(bench_run *run);
};

/** Opzioni globali (impostate nel main). */
typedef struct {
    double timeout_s;       /**< tempo massimo per esecuzione (RB-10) */
    int    warmup_runs;     /**< esecuzioni a vuoto (RB-4) */
    int    measured_runs;   /**< esecuzioni misurate (RB-4) */
    double inactivity_s;    /**< controllo di inattività (RB-12) */
} bench_options;

#define BENCH_OPTIONS_DEFAULT { .timeout_s = 10.0, .warmup_runs = 1, .measured_runs = 5, .inactivity_s = 120.0 }

/**
 * Punto di ingresso unico: decide se fare da padre o da figlio (F5) ed esegue tutto.
 *
 * @return codice di uscita del programma: 0 esecuzione completa, 1 errore fatale (F6).
 */
int bench_main(int argc, char **argv,
               const bench_test *const *tests, size_t test_count,
               const bench_options *options);

#endif /* BENCH_H */
