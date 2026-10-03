/**
 * bench_internal.h — tipi e funzioni interne alla parte comune (non usati dai test).
 *
 * Il "reporter" separa l'esecuzione (F4) da chi ne raccoglie i risultati:
 * oggi (passo 2 di B6) è il raccoglitore del padre nello stesso processo,
 * con F5 diventerà il protocollo testuale del processo figlio.
 */

#ifndef BENCH_INTERNAL_H
#define BENCH_INTERNAL_H

#include "bench.h"

#define BENCH_MAX_DISCARD_REASONS 8

/** Una configurazione di work-group generata da F3. */
typedef struct {
    size_t local[3];
    size_t eff[3];          /**< dimensione effettiva (RB-11) */
    double discarded_pct;   /**< 1 - prodotto(eff) / prodotto(ref), in frazione */
} bench_config;

/** Configurazioni scartate, raggruppate per motivo (RB-2). */
typedef struct {
    size_t generated;   /**< configurazioni valide da misurare */
    size_t discarded;   /**< totale scartate */
    size_t reason_count;
    struct {
        size_t count;
        char   reason[BENCH_REASON_MAX];
    } reasons[BENCH_MAX_DISCARD_REASONS];
} bench_discard_summary;

typedef enum {
    BENCH_STATUS_OK = 0,
    BENCH_STATUS_WRONG      /**< ERRATO (RB-5) */
} bench_status;

/** Risultato di una configurazione misurata (F4). */
typedef struct {
    cl_uint        work_dim;
    size_t         local[3];
    size_t         eff[3];
    double         discarded_pct;
    double         median_ns;
    double         flop;
    bench_status   status;
    int            host_timing; /**< 1 = tempo dall'orologio host (profiling inaffidabile, RB-3) */
    bench_mismatch mismatch;    /**< valido solo se status == BENCH_STATUS_WRONG */
} bench_result;

/** Destinatario degli eventi di F4. Tutti i campi funzione sono obbligatori. */
typedef struct {
    void *ctx;
    void (*configs)(void *ctx, const bench_discard_summary *summary);
    void (*run_start)(void *ctx, const bench_config *config);   /**< subito prima dell'enqueue */
    void (*run_end)(void *ctx, const bench_config *config);     /**< subito dopo la fine dell'attesa */
    void (*result)(void *ctx, const bench_result *result);
    void (*discard)(void *ctx, const bench_config *config, const char *reason);  /**< rifiutata all'enqueue */
    void (*timeout_os)(void *ctx, const bench_config *config);                   /**< errore durante l'esecuzione */
    void (*timeout)(void *ctx, const bench_config *config, double limit_s);      /**< tempo massimo superato (F5) */
    void (*skip)(void *ctx, const char *reason);                                  /**< test saltato su questo device */
} bench_reporter;

/**
 * F3 — Genera le configurazioni di work-group per un test su un device (kernel già compilato).
 *
 * @return BENCH_OK (anche con 0 configurazioni), BENCH_ERR_MEMORY, oppure BENCH_ERR con
 *         skip_reason compilato (test da saltare su questo device).
 */
int bench_configs_generate(const bench_test *test, const bench_device *device, cl_kernel kernel,
                           bench_config **configs, size_t *count,
                           bench_discard_summary *summary,
                           char *skip_reason, size_t skip_reason_len);

/**
 * F2 — Controlla un descrittore (usato da padre e figlio).
 * @return 1 se valido, 0 se no (reason compilato).
 */
int bench_validate_test(const bench_test *const *tests, size_t index, char *reason, size_t len);

/* -------------------------------------------------------------------------
 * F5 — bench_proc.c
 * ------------------------------------------------------------------------- */

/** Argomento interno che identifica il processo figlio. */
#define BENCH_CHILD_ARG "--bench-child"

/** 1 se il processo è stato lanciato come figlio (argv[1] == BENCH_CHILD_ARG). */
int bench_is_child(int argc, char **argv);

/** Corpo del processo figlio: esegue F4 per una coppia e scrive il protocollo su stdout. */
int bench_child_main(int argc, char **argv,
                     const bench_test *const *tests, size_t test_count,
                     const bench_options *options);

/**
 * Padre: esegue una coppia test/device in un processo figlio, applicando il tempo massimo
 * (RB-10) e il controllo di inattività (RB-12), e inoltra gli eventi al reporter.
 */
void bench_supervise_pair(size_t test_index, const bench_test *test, const bench_device *device,
                          const bench_options *options, const bench_reporter *reporter);

/** Orologio monotonico in ns (bench_proc.c: unica parte dipendente dal sistema operativo). */
double bench_clock_ns(void);

/** F4 — Esegue e misura un test su un device, comunicando gli eventi al reporter. */
void bench_execute_pair(const bench_test *test, const bench_device *device,
                        const bench_options *options, const bench_reporter *reporter);

#endif /* BENCH_INTERNAL_H */
