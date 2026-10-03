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
    void (*run_start)(void *ctx, size_t config_index);
    void (*run_end)(void *ctx, size_t config_index);
    void (*result)(void *ctx, const bench_result *result);
    void (*discard)(void *ctx, const bench_config *config, const char *reason);  /**< rifiutata all'enqueue */
    void (*timeout_os)(void *ctx, const bench_config *config);                   /**< errore durante l'esecuzione */
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

/** Orologio monotonico in ns (bench_proc.c: unica parte dipendente dal sistema operativo). */
double bench_clock_ns(void);

/** F4 — Esegue e misura un test su un device, comunicando gli eventi al reporter. */
void bench_execute_pair(const bench_test *test, const bench_device *device,
                        const bench_options *options, const bench_reporter *reporter);

#endif /* BENCH_INTERNAL_H */
