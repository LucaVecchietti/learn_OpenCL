/**
 * test_stencil3d.c — F10: stencil 3D a 7 punti, bordi copiati invariati (Q-36).
 *
 * Il bordo dipende dalla dimensione effettiva, quindi il riferimento si calcola
 * per ogni configurazione (niente cache, a differenza di mmul).
 */

#include "test_kernels.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define STENCIL_SIZE 256u
#define STENCIL_SEED 0x5eed3d0u
#define C0           0.4
#define C1           0.1
#define REL_TOL      1e-5
#define ABS_TOL      1e-6

typedef struct {
    float *in, *out;
    cl_mem din, dout;
    size_t x, y, z;
} stencil_state;

static int stencil_setup(bench_run *run)
{
    size_t X = run->eff_size[0], Y = run->eff_size[1], Z = run->eff_size[2];
    if (X < 3 || Y < 3 || Z < 3) {
        fprintf(stderr, "griglia %zux%zux%zu senza celle interne\n", X, Y, Z);
        return BENCH_ERR;
    }
    if (X * Y * Z > UINT_MAX) {
        fprintf(stderr, "griglia %zux%zux%zu oltre i limiti di unsigned int\n", X, Y, Z);
        return BENCH_ERR;
    }

    stencil_state *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        return BENCH_ERR_MEMORY;
    }
    run->state = s;
    s->x = X;
    s->y = Y;
    s->z = Z;

    size_t cells = X * Y * Z;
    s->in = malloc(cells * sizeof(float));
    s->out = malloc(cells * sizeof(float));
    if (s->in == NULL || s->out == NULL) {
        return BENCH_ERR_MEMORY;
    }
    for (size_t z = 0; z < Z; z++)
        for (size_t y = 0; y < Y; y++)
            for (size_t x = 0; x < X; x++)
                s->in[(z * Y + y) * X + x] =
                    bench_hash_float((uint32_t)x, (uint32_t)y, (uint32_t)z, STENCIL_SEED, -1.0f, 1.0f);

    int rc = bench_buffer_create(run, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cells * sizeof(float), s->in, &s->din);
    if (rc == BENCH_OK) {
        rc = bench_buffer_create(run, CL_MEM_WRITE_ONLY, cells * sizeof(float), NULL, &s->dout);
    }
    return rc;
}

static int stencil_set_args(bench_run *run)
{
    stencil_state *s = run->state;
    cl_uint X = (cl_uint)s->x, Y = (cl_uint)s->y, Z = (cl_uint)s->z;
    cl_int err = clSetKernelArg(run->kernel, 0, sizeof(cl_mem), &s->din);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 1, sizeof(cl_mem), &s->dout);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 2, sizeof(cl_uint), &X);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 3, sizeof(cl_uint), &Y);
    if (err == CL_SUCCESS) err = clSetKernelArg(run->kernel, 4, sizeof(cl_uint), &Z);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clSetKernelArg fallita (codice %d)\n", err);
        return BENCH_ERR;
    }
    return BENCH_OK;
}

static int stencil_verify(bench_run *run, bench_mismatch *mismatch)
{
    stencil_state *s = run->state;
    size_t X = s->x, Y = s->y, Z = s->z;
    int rc = bench_buffer_read(run, s->dout, X * Y * Z * sizeof(float), s->out);
    if (rc != BENCH_OK) {
        return rc;
    }

    size_t sy = X, sz = X * Y;
    for (size_t z = 0; z < Z; z++) {
        for (size_t y = 0; y < Y; y++) {
            for (size_t x = 0; x < X; x++) {
                size_t i = z * sz + y * sy + x;
                double expected;
                if (x == 0 || y == 0 || z == 0 || x == X - 1 || y == Y - 1 || z == Z - 1) {
                    expected = s->in[i];
                } else {
                    expected = C0 * s->in[i] +
                               C1 * ((double)s->in[i - 1] + s->in[i + 1] + s->in[i - sy] +
                                     s->in[i + sy] + s->in[i - sz] + s->in[i + sz]);
                }
                if (!bench_float_close(expected, s->out[i], REL_TOL, ABS_TOL)) {
                    mismatch->index = i;
                    mismatch->expected = expected;
                    mismatch->got = s->out[i];
                    return 1;
                }
            }
        }
    }
    return 0;
}

static double stencil_flop(const size_t eff_size[3])
{
    /* 6 somme + 2 moltiplicazioni per cella interna; il bordo non conta. */
    return 8.0 * (double)(eff_size[0] - 2) * (double)(eff_size[1] - 2) * (double)(eff_size[2] - 2);
}

static void stencil_teardown(bench_run *run)
{
    stencil_state *s = run->state;
    if (s == NULL) {
        return;
    }
    if (s->din != NULL) clReleaseMemObject(s->din);
    if (s->dout != NULL) clReleaseMemObject(s->dout);
    free(s->in);
    free(s->out);
    free(s);
    run->state = NULL;
}

const bench_test TEST_STENCIL3D = {
    .name = "stencil3d",
    .source_path = "minibanchmark/kernels/stencil3d.cl",
    .kernel_name = "stencil3d",
    .work_dim = 3,
    .ref_size = { STENCIL_SIZE, STENCIL_SIZE, STENCIL_SIZE },
    .shape = BENCH_SHAPE_ANY,
    .setup = stencil_setup,
    .set_args = stencil_set_args,
    .verify = stencil_verify,
    .flop = stencil_flop,
    .teardown = stencil_teardown,
};
