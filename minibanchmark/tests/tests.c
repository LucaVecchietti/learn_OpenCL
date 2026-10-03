/**
 * tests.c — registro dei test.
 *
 * Per aggiungere un test 1D elemento per elemento: scrivere il .cl, una funzione op_*,
 * i parametri e il descrittore qui sotto, e aggiungerlo all'array (CA-10).
 * Gli altri test hanno il descrittore in un test_*.c e si registrano solo nell'array.
 */

#include "tests.h"
#include "bench/bench_elementwise.h"
#include "test_kernels.h"
#include "test_selftest.h"

#define KERNELS "minibanchmark/kernels/"
#define REF_1D  (1u << 24)   /* 16.777.216 elementi (Q-31) */

/* ---- Famiglia 1D: riferimento sull'host, in float ---- */

static float op_add(float a, float b) { return a + b; }
static float op_sub(float a, float b) { return a - b; }
static float op_mul(float a, float b) { return a * b; }
static float op_div(float a, float b) { return a / b; }

static const bench_elementwise_params P_ADD = { op_add, 1e-6, 1e-6, -1000.0f, 1000.0f, 0 };
static const bench_elementwise_params P_SUB = { op_sub, 1e-6, 1e-6, -1000.0f, 1000.0f, 0 };
static const bench_elementwise_params P_MUL = { op_mul, 1e-6, 1e-6, -1000.0f, 1000.0f, 0 };
/* OpenCL ammette fino a 2,5 ULP sulla divisione (~3e-7 relativo). */
static const bench_elementwise_params P_DIV = { op_div, 1e-6, 1e-6, -1000.0f, 1000.0f, 1 };

static const bench_test TEST_ADD = BENCH_ELEMENTWISE_TEST("add", KERNELS "add.cl", "add", REF_1D, &P_ADD);
static const bench_test TEST_SUB = BENCH_ELEMENTWISE_TEST("sub", KERNELS "sub.cl", "sub", REF_1D, &P_SUB);
static const bench_test TEST_MUL = BENCH_ELEMENTWISE_TEST("mul", KERNELS "mul.cl", "mul", REF_1D, &P_MUL);
static const bench_test TEST_DIV = BENCH_ELEMENTWISE_TEST("div", KERNELS "div.cl", "div", REF_1D, &P_DIV);

/* ---- Registro ---- */

const bench_test *const BENCH_TESTS[] = {
    &TEST_ADD,
    &TEST_SUB,
    &TEST_MUL,
    &TEST_DIV,
    &TEST_MAX,
    &TEST_MMUL,
    &TEST_MMUL_TILED,
    &TEST_STENCIL3D,
#ifdef BENCH_SELFTEST
    &SELFTEST_TIMEOUT,
    &SELFTEST_LOCALMEM,
    &SELFTEST_CRASH,
#endif
#ifdef BENCH_SELFTEST_HANG
    &SELFTEST_HANG,
#endif
};

const size_t BENCH_TESTS_COUNT = sizeof(BENCH_TESTS) / sizeof(BENCH_TESTS[0]);
