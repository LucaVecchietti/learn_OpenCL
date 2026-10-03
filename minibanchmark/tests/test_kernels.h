/**
 * test_kernels.h — test di minibanchmark con descrittore proprio (non della famiglia 1D).
 */

#ifndef TEST_KERNELS_H
#define TEST_KERNELS_H

#include "bench/bench.h"

extern const bench_test TEST_MAX;         /* F8: riduzione */
extern const bench_test TEST_MMUL;        /* F9: 2D, memoria globale */
extern const bench_test TEST_MMUL_TILED;  /* F9: 2D, local memory a tile */
extern const bench_test TEST_STENCIL3D;   /* F10: 3D, stencil a 7 punti */

#endif /* TEST_KERNELS_H */
