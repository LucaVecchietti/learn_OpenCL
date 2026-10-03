/**
 * bench_elementwise.h — famiglia generica di test 1D "elemento per elemento" (F7, Q-20).
 *
 * Per kernel con firma:
 *   __kernel void k(__global const float *A, __global const float *B,
 *                   __global float *C, const unsigned int n)
 * che calcolano C[i] = op(A[i], B[i]). Il test fornisce solo op e le tolleranze.
 */

#ifndef BENCH_ELEMENTWISE_H
#define BENCH_ELEMENTWISE_H

#include "bench.h"

typedef struct {
    float (*op)(float a, float b);  /**< riferimento calcolato sull'host, in float */
    double rel_tol;
    double abs_tol;
    float  min;                     /**< intervallo degli input */
    float  max;
    int    b_nonzero;               /**< 1 = B in ±[0.5, max], mai zero né vicino (div) */
} bench_elementwise_params;

int    bench_elementwise_setup(bench_run *run);
int    bench_elementwise_set_args(bench_run *run);
int    bench_elementwise_verify(bench_run *run, bench_mismatch *mismatch);
double bench_elementwise_flop(const size_t eff_size[3]);
void   bench_elementwise_teardown(bench_run *run);

/** Descrittore di un test della famiglia (1 FLOP per elemento). */
#define BENCH_ELEMENTWISE_TEST(name_, path_, kernel_, ref_size_, params_) { \
    .name = (name_),                                                         \
    .source_path = (path_),                                                  \
    .kernel_name = (kernel_),                                                \
    .work_dim = 1,                                                           \
    .ref_size = { (ref_size_), 1, 1 },                                       \
    .shape = BENCH_SHAPE_ANY,                                                \
    .user = (params_),                                                       \
    .setup = bench_elementwise_setup,                                        \
    .set_args = bench_elementwise_set_args,                                  \
    .verify = bench_elementwise_verify,                                      \
    .flop = bench_elementwise_flop,                                          \
    .teardown = bench_elementwise_teardown,                                  \
}

#endif /* BENCH_ELEMENTWISE_H */
