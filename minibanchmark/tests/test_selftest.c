/**
 * test_selftest.c — test di prova della parte comune (F11).
 *
 * Registrati in tests.c solo con -DBENCH_SELFTEST (timeout, local memory, crash)
 * e -DBENCH_SELFTEST_HANG (blocco fuori dall'esecuzione del kernel).
 */

#include "test_selftest.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SELFTEST_SOURCE "minibanchmark/kernels/selftest.cl"
#define SELFTEST_SIZE   (1u << 20)

typedef struct {
    float *host;
    cl_mem out;
    size_t n;
} selftest_state;

/* Buffer di output comune a tutti i test di prova. */
static int selftest_setup(bench_run *run)
{
    selftest_state *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return BENCH_ERR_MEMORY;
    }
    run->state = s;
    s->n = run->eff_size[0];
    s->host = malloc(s->n * sizeof(float));
    if (s->host == NULL) {
        return BENCH_ERR_MEMORY;
    }
    return bench_buffer_create(run, CL_MEM_WRITE_ONLY, s->n * sizeof(float), NULL, &s->out);
}

static void selftest_teardown(bench_run *run)
{
    selftest_state *s = run->state;
    if (s == NULL) {
        return;
    }
    if (s->out != NULL) clReleaseMemObject(s->out);
    free(s->host);
    free(s);
    run->state = NULL;
}

static double selftest_flop(const size_t eff_size[3])
{
    return (double)eff_size[0];
}

/* ---- selftest_timeout (CA-13) ---- */

static int timeout_set_args(bench_run *run)
{
    selftest_state *s = run->state;
    cl_uint iterations = UINT_MAX;
    cl_int err = clSetKernelArg(run->kernel, 0, sizeof(cl_mem), &s->out);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 1, sizeof(cl_uint), &iterations);
    return err == CL_SUCCESS ? BENCH_OK : BENCH_ERR;
}

static int timeout_verify(bench_run *run, bench_mismatch *mismatch)
{
    (void)run;
    (void)mismatch;
    return 0;   /* non si arriva mai qui: l'esecuzione viene fermata prima */
}

const bench_test SELFTEST_TIMEOUT = {
    .name = "selftest_timeout",
    .source_path = SELFTEST_SOURCE,
    .kernel_name = "selftest_timeout",
    .work_dim = 1,
    .ref_size = { SELFTEST_SIZE, 1, 1 },
    .timeout_s = 1.0,   /* sotto i ~2 s del TDR delle GPU su Windows */
    .setup = selftest_setup,
    .set_args = timeout_set_args,
    .verify = timeout_verify,
    .flop = selftest_flop,
    .teardown = selftest_teardown,
};

/* ---- selftest_localmem (CA-3) ---- */

static size_t localmem_bytes(const size_t local_size[3])
{
    return local_size[0] * 128;   /* oltre 32 KB già da L = 256, oltre 64 KB da L = 512 */
}

static int localmem_set_args(bench_run *run)
{
    selftest_state *s = run->state;
    cl_uint n = (cl_uint)s->n;
    cl_int err = clSetKernelArg(run->kernel, 0, sizeof(cl_mem), &s->out);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 1, localmem_bytes(run->local_size), NULL);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 2, sizeof(cl_uint), &n);
    return err == CL_SUCCESS ? BENCH_OK : BENCH_ERR;
}

static int localmem_verify(bench_run *run, bench_mismatch *mismatch)
{
    selftest_state *s = run->state;
    int rc = bench_buffer_read(run, s->out, s->n * sizeof(float), s->host);
    if (rc != BENCH_OK) {
        return rc;
    }
    for (size_t i = 0; i < s->n; i++) {
        if (s->host[i] != (float)i) {
            mismatch->index = i;
            mismatch->expected = (double)(float)i;
            mismatch->got = s->host[i];
            return 1;
        }
    }
    return 0;
}

const bench_test SELFTEST_LOCALMEM = {
    .name = "selftest_localmem",
    .source_path = SELFTEST_SOURCE,
    .kernel_name = "selftest_localmem",
    .work_dim = 1,
    .ref_size = { SELFTEST_SIZE, 1, 1 },
    .setup = selftest_setup,
    .set_args = localmem_set_args,
    .local_mem_bytes = localmem_bytes,
    .verify = localmem_verify,
    .flop = selftest_flop,
    .teardown = selftest_teardown,
};

/* ---- selftest_crash (F5: crash di un figlio) ---- */

static int crash_setup(bench_run *run)
{
    (void)run;
    volatile int *null_pointer = NULL;
    *null_pointer = 0;   /* accesso non valido voluto: il processo figlio termina */
    return BENCH_ERR;
}

const bench_test SELFTEST_CRASH = {
    .name = "selftest_crash",
    .source_path = SELFTEST_SOURCE,
    .kernel_name = "selftest_localmem",
    .work_dim = 1,
    .ref_size = { SELFTEST_SIZE, 1, 1 },
    .setup = crash_setup,
    .set_args = localmem_set_args,
    .local_mem_bytes = localmem_bytes,
    .verify = localmem_verify,
    .flop = selftest_flop,
    .teardown = selftest_teardown,
};

/* ---- selftest_hang (CA-21) ---- */

static int hang_setup(bench_run *run)
{
    /* Attesa attiva di 130 s in C portabile: oltre i 120 s del controllo di inattività. */
    time_t start = time(NULL);
    while (difftime(time(NULL), start) < 130.0) {
    }
    return selftest_setup(run);
}

const bench_test SELFTEST_HANG = {
    .name = "selftest_hang",
    .source_path = SELFTEST_SOURCE,
    .kernel_name = "selftest_localmem",
    .work_dim = 1,
    .ref_size = { SELFTEST_SIZE, 1, 1 },
    .setup = hang_setup,
    .set_args = localmem_set_args,
    .local_mem_bytes = localmem_bytes,
    .verify = localmem_verify,
    .flop = selftest_flop,
    .teardown = selftest_teardown,
};
