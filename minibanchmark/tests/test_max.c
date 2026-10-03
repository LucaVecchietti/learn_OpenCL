/**
 * test_max.c — F8: riduzione max, un passaggio sul device (massimo per gruppo),
 * chiusura sull'host fuori dalla misura (Q-33).
 */

#include "test_kernels.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_SEED      0x51ed270bu
#define MAX_POS_SEED  0x2545f491u
#define UNIQUE_MAX    5000.0f   /* massimo unico, così la verifica globale è significativa */

typedef struct {
    float *a;
    float *partial;
    cl_mem da;
    cl_mem dpartial;
    size_t n;
    size_t groups;
} max_state;

static size_t max_local_mem_bytes(const size_t local_size[3])
{
    return local_size[0] * sizeof(float);
}

static int max_setup(bench_run *run)
{
    size_t n = run->eff_size[0];
    if (n > UINT_MAX) {
        fprintf(stderr, "dimensione %zu oltre i limiti di unsigned int\n", n);
        return BENCH_ERR;
    }

    max_state *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return BENCH_ERR_MEMORY;
    }
    run->state = s;
    s->n = n;
    s->groups = n / run->local_size[0];   /* eff è multiplo di local (RB-11) */

    s->a = malloc(n * sizeof(float));
    s->partial = malloc(s->groups * sizeof(float));
    if (s->a == NULL || s->partial == NULL) {
        return BENCH_ERR_MEMORY;
    }

    bench_fill_random_float(s->a, n, MAX_SEED, -1000.0f, 1000.0f);
    uint32_t pos_state = MAX_POS_SEED;
    s->a[bench_random_next(&pos_state) % n] = UNIQUE_MAX;

    int rc = bench_buffer_create(run, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, n * sizeof(float), s->a, &s->da);
    if (rc == BENCH_OK) {
        rc = bench_buffer_create(run, CL_MEM_WRITE_ONLY, s->groups * sizeof(float), NULL, &s->dpartial);
    }
    return rc;
}

static int max_set_args(bench_run *run)
{
    max_state *s = run->state;
    cl_uint n = (cl_uint)s->n;
    cl_int err = clSetKernelArg(run->kernel, 0, sizeof(cl_mem), &s->da);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 1, sizeof(cl_mem), &s->dpartial);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 2, max_local_mem_bytes(run->local_size), NULL);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 3, sizeof(cl_uint), &n);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clSetKernelArg fallita (codice %d)\n", err);
        return BENCH_ERR;
    }
    return BENCH_OK;
}

static int max_verify(bench_run *run, bench_mismatch *mismatch)
{
    max_state *s = run->state;
    int rc = bench_buffer_read(run, s->dpartial, s->groups * sizeof(float), s->partial);
    if (rc != BENCH_OK) {
        return rc;
    }

    /* 1. Ogni parziale è il massimo del suo gruppo (un errore viene localizzato al gruppo). */
    size_t local = run->local_size[0];
    float global_host = -INFINITY, global_device = -INFINITY;
    for (size_t g = 0; g < s->groups; g++) {
        float expected = -INFINITY;
        for (size_t i = g * local; i < (g + 1) * local; i++) {
            if (s->a[i] > expected) expected = s->a[i];
        }
        if (s->partial[g] != expected) {
            mismatch->index = g;
            mismatch->expected = expected;
            mismatch->got = s->partial[g];
            return 1;
        }
        if (expected > global_host) global_host = expected;
        if (s->partial[g] > global_device) global_device = s->partial[g];
    }

    /* 2. Chiusura sull'host: il massimo dei parziali è il massimo globale. */
    if (global_device != global_host) {
        mismatch->index = s->groups;
        mismatch->expected = global_host;
        mismatch->got = global_device;
        return 1;
    }
    return 0;
}

static double max_flop(const size_t eff_size[3])
{
    return (double)eff_size[0];   /* un confronto fmax per elemento, contato come 1 FLOP */
}

static void max_teardown(bench_run *run)
{
    max_state *s = run->state;
    if (s == NULL) {
        return;
    }
    if (s->da != NULL) clReleaseMemObject(s->da);
    if (s->dpartial != NULL) clReleaseMemObject(s->dpartial);
    free(s->a);
    free(s->partial);
    free(s);
    run->state = NULL;
}

const bench_test TEST_MAX = {
    .name = "max",
    .source_path = "minibanchmark/kernels/max.cl",
    .kernel_name = "max_reduce",
    .work_dim = 1,
    .ref_size = { 1u << 24, 1, 1 },
    .shape = BENCH_SHAPE_ANY,
    .setup = max_setup,
    .set_args = max_set_args,
    .local_mem_bytes = max_local_mem_bytes,
    .verify = max_verify,
    .flop = max_flop,
    .teardown = max_teardown,
};
