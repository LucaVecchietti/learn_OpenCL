/**
 * bench_elementwise.c — famiglia generica di test 1D "elemento per elemento" (F7).
 */

#include "bench_elementwise.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* Seed fissi: gli input sono identici a ogni esecuzione. */
#define SEED_A    0x1234567u
#define SEED_B    0x89abcdeu
#define SEED_SIGN 0x2468aceu

typedef struct {
    float *a, *b, *c;       /* host: input e output riletto */
    cl_mem da, db, dc;      /* device */
    size_t n;
} elementwise_state;

int bench_elementwise_setup(bench_run *run)
{
    const bench_elementwise_params *p = run->user;
    size_t n = run->eff_size[0];

    if (n > UINT_MAX) {
        fprintf(stderr, "dimensione %zu oltre i limiti di unsigned int\n", n);
        return BENCH_ERR;
    }

    elementwise_state *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return BENCH_ERR_MEMORY;
    }
    run->state = s;
    s->n = n;

    s->a = malloc(n * sizeof(float));
    s->b = malloc(n * sizeof(float));
    s->c = malloc(n * sizeof(float));
    if (s->a == NULL || s->b == NULL || s->c == NULL) {
        return BENCH_ERR_MEMORY;    /* teardown libera quanto allocato */
    }

    bench_fill_random_float(s->a, n, SEED_A, p->min, p->max);
    if (p->b_nonzero) {
        /* B in ±[0.5, max]: modulo casuale, segno casuale. */
        bench_fill_random_float(s->b, n, SEED_B, 0.5f, p->max);
        uint32_t sign = SEED_SIGN;
        for (size_t i = 0; i < n; i++) {
            if (bench_random_next(&sign) & 1u) {
                s->b[i] = -s->b[i];
            }
        }
    } else {
        bench_fill_random_float(s->b, n, SEED_B, p->min, p->max);
    }

    int rc = bench_buffer_create(run, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, n * sizeof(float), s->a, &s->da);
    if (rc == BENCH_OK) {
        rc = bench_buffer_create(run, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, n * sizeof(float), s->b, &s->db);
    }
    if (rc == BENCH_OK) {
        rc = bench_buffer_create(run, CL_MEM_WRITE_ONLY, n * sizeof(float), NULL, &s->dc);
    }
    return rc;
}

int bench_elementwise_set_args(bench_run *run)
{
    elementwise_state *s = run->state;
    cl_uint n = (cl_uint)s->n;

    cl_int err = clSetKernelArg(run->kernel, 0, sizeof(cl_mem), &s->da);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 1, sizeof(cl_mem), &s->db);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 2, sizeof(cl_mem), &s->dc);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 3, sizeof(cl_uint), &n);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clSetKernelArg fallita (codice %d)\n", err);
        return BENCH_ERR;
    }
    return BENCH_OK;
}

int bench_elementwise_verify(bench_run *run, bench_mismatch *mismatch)
{
    const bench_elementwise_params *p = run->user;
    elementwise_state *s = run->state;

    int rc = bench_buffer_read(run, s->dc, s->n * sizeof(float), s->c);
    if (rc != BENCH_OK) {
        return rc;
    }

    for (size_t i = 0; i < s->n; i++) {
        float expected = p->op(s->a[i], s->b[i]);
        if (!bench_float_close(expected, s->c[i], p->rel_tol, p->abs_tol)) {
            mismatch->index = i;
            mismatch->expected = expected;
            mismatch->got = s->c[i];
            return 1;
        }
    }
    return 0;
}

double bench_elementwise_flop(const size_t eff_size[3])
{
    return (double)eff_size[0];
}

void bench_elementwise_teardown(bench_run *run)
{
    elementwise_state *s = run->state;
    if (s == NULL) {
        return;
    }
    if (s->da != NULL) clReleaseMemObject(s->da);
    if (s->db != NULL) clReleaseMemObject(s->db);
    if (s->dc != NULL) clReleaseMemObject(s->dc);
    free(s->a);
    free(s->b);
    free(s->c);
    free(s);
    run->state = NULL;
}
