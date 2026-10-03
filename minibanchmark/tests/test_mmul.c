/**
 * test_mmul.c — F9: moltiplicazione di matrici 2D, base (memoria globale) e a tile.
 *
 * C (M x N) = A (M x K) * B (K x N). N = colonne = eff[0], M = righe = eff[1], K fisso.
 * A[i][k] e B[k][j] dipendono solo dalla posizione: C[r][c] non dipende da M e N,
 * quindi il riferimento 1024 x 1024 si calcola una volta per processo (cioè per device)
 * e vale per tutte le configurazioni.
 */

#include "test_kernels.h"

#include <stdio.h>
#include <stdlib.h>

#define MMUL_SIZE  1024u
#define MMUL_K     1024u
#define SEED_A     0x0a11ce5u
#define SEED_B     0x0b0b0b5u
#define REL_TOL    1e-4
#define ABS_TOL    1e-3

typedef struct {
    int tiled;
} mmul_params;

typedef struct {
    float *a, *b, *c;
    cl_mem da, db, dc;
    size_t m, n;
} mmul_state;

static float value_a(size_t i, size_t k) { return bench_hash_float((uint32_t)i, (uint32_t)k, 0, SEED_A, -1.0f, 1.0f); }
static float value_b(size_t k, size_t j) { return bench_hash_float((uint32_t)k, (uint32_t)j, 0, SEED_B, -1.0f, 1.0f); }

/*
 * [D] Cache del riferimento: stato statico del file, calcolato alla prima verifica
 * del processo e mai liberato (il processo figlio termina alla fine della coppia).
 */
static double *reference = NULL;

static int ensure_reference(void)
{
    if (reference != NULL) {
        return BENCH_OK;
    }
    double *ref = calloc((size_t)MMUL_SIZE * MMUL_SIZE, sizeof(double));
    float *a = malloc((size_t)MMUL_SIZE * MMUL_K * sizeof(float));
    float *b = malloc((size_t)MMUL_K * MMUL_SIZE * sizeof(float));
    if (ref == NULL || a == NULL || b == NULL) {
        free(ref);
        free(a);
        free(b);
        return BENCH_ERR_MEMORY;
    }
    for (size_t i = 0; i < MMUL_SIZE; i++)
        for (size_t k = 0; k < MMUL_K; k++)
            a[i * MMUL_K + k] = value_a(i, k);
    for (size_t k = 0; k < MMUL_K; k++)
        for (size_t j = 0; j < MMUL_SIZE; j++)
            b[k * MMUL_SIZE + j] = value_b(k, j);

    /* Ordine i-k-j: accessi contigui a b e ref. Calcolo in double (F9). */
    for (size_t i = 0; i < MMUL_SIZE; i++) {
        double *row = &ref[i * MMUL_SIZE];
        for (size_t k = 0; k < MMUL_K; k++) {
            double aik = a[i * MMUL_K + k];
            const float *brow = &b[k * MMUL_SIZE];
            for (size_t j = 0; j < MMUL_SIZE; j++) {
                row[j] += aik * brow[j];
            }
        }
    }
    free(a);
    free(b);
    reference = ref;
    return BENCH_OK;
}

static size_t tiled_local_mem_bytes(const size_t local_size[3])
{
    return 2 * local_size[0] * local_size[0] * sizeof(float);   /* As + Bs, TS x TS ciascuna */
}

static int mmul_setup(bench_run *run)
{
    const mmul_params *p = run->user;
    if (p->tiled && MMUL_K % run->local_size[0] != 0) {
        fprintf(stderr, "K = %u non multiplo del lato della tile %zu\n", MMUL_K, run->local_size[0]);
        return BENCH_ERR;
    }

    mmul_state *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return BENCH_ERR_MEMORY;
    }
    run->state = s;
    s->n = run->eff_size[0];
    s->m = run->eff_size[1];

    s->a = malloc(s->m * MMUL_K * sizeof(float));
    s->b = malloc(MMUL_K * s->n * sizeof(float));
    s->c = malloc(s->m * s->n * sizeof(float));
    if (s->a == NULL || s->b == NULL || s->c == NULL) {
        return BENCH_ERR_MEMORY;
    }
    for (size_t i = 0; i < s->m; i++)
        for (size_t k = 0; k < MMUL_K; k++)
            s->a[i * MMUL_K + k] = value_a(i, k);
    for (size_t k = 0; k < MMUL_K; k++)
        for (size_t j = 0; j < s->n; j++)
            s->b[k * s->n + j] = value_b(k, j);

    int rc = bench_buffer_create(run, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, s->m * MMUL_K * sizeof(float), s->a, &s->da);
    if (rc == BENCH_OK) {
        rc = bench_buffer_create(run, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, MMUL_K * s->n * sizeof(float), s->b, &s->db);
    }
    if (rc == BENCH_OK) {
        rc = bench_buffer_create(run, CL_MEM_WRITE_ONLY, s->m * s->n * sizeof(float), NULL, &s->dc);
    }
    return rc;
}

static int mmul_set_args(bench_run *run)
{
    const mmul_params *p = run->user;
    mmul_state *s = run->state;
    cl_uint m = (cl_uint)s->m, n = (cl_uint)s->n, k = MMUL_K;

    cl_int err = clSetKernelArg(run->kernel, 0, sizeof(cl_mem), &s->da);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 1, sizeof(cl_mem), &s->db);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 2, sizeof(cl_mem), &s->dc);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 3, sizeof(cl_uint), &m);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 4, sizeof(cl_uint), &n);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 5, sizeof(cl_uint), &k);
    if (p->tiled) {
        size_t tile_bytes = run->local_size[0] * run->local_size[0] * sizeof(float);
        if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 6, tile_bytes, NULL);
        if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 7, tile_bytes, NULL);
    }
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clSetKernelArg fallita (codice %d)\n", err);
        return BENCH_ERR;
    }
    return BENCH_OK;
}

static int mmul_verify(bench_run *run, bench_mismatch *mismatch)
{
    mmul_state *s = run->state;
    int rc = bench_buffer_read(run, s->dc, s->m * s->n * sizeof(float), s->c);
    if (rc == BENCH_OK) {
        rc = ensure_reference();
    }
    if (rc != BENCH_OK) {
        return rc;
    }

    for (size_t r = 0; r < s->m; r++) {
        for (size_t c = 0; c < s->n; c++) {
            double expected = reference[r * MMUL_SIZE + c];
            float got = s->c[r * s->n + c];
            if (!bench_float_close(expected, got, REL_TOL, ABS_TOL)) {
                mismatch->index = r * s->n + c;
                mismatch->expected = expected;
                mismatch->got = got;
                return 1;
            }
        }
    }
    return 0;
}

static double mmul_flop(const size_t eff_size[3])
{
    return 2.0 * (double)eff_size[1] * (double)eff_size[0] * (double)MMUL_K;
}

static void mmul_teardown(bench_run *run)
{
    mmul_state *s = run->state;
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

static const mmul_params BASE = { 0 };
static const mmul_params TILED = { 1 };

const bench_test TEST_MMUL = {
    .name = "mmul",
    .source_path = "minibanchmark/kernels/mmul.cl",
    .kernel_name = "mmul",
    .work_dim = 2,
    .ref_size = { MMUL_SIZE, MMUL_SIZE, 1 },
    .shape = BENCH_SHAPE_ANY,
    .user = &BASE,
    .setup = mmul_setup,
    .set_args = mmul_set_args,
    .verify = mmul_verify,
    .flop = mmul_flop,
    .teardown = mmul_teardown,
};

const bench_test TEST_MMUL_TILED = {
    .name = "mmul_tiled",
    .source_path = "minibanchmark/kernels/mmul_tiled.cl",
    .kernel_name = "mmul_tiled",
    .work_dim = 2,
    .ref_size = { MMUL_SIZE, MMUL_SIZE, 1 },
    .shape = BENCH_SHAPE_EQUAL,   /* tile quadrate (CA-17) */
    .user = &TILED,
    .setup = mmul_setup,
    .set_args = mmul_set_args,
    .local_mem_bytes = tiled_local_mem_bytes,
    .verify = mmul_verify,
    .flop = mmul_flop,
    .teardown = mmul_teardown,
};
