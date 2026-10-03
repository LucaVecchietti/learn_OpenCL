/**
 * test_selftest.h — test di prova della parte comune (F11).
 */

#ifndef TEST_SELFTEST_H
#define TEST_SELFTEST_H

#include "bench/bench.h"

extern const bench_test SELFTEST_TIMEOUT;   /* CA-13 */
extern const bench_test SELFTEST_LOCALMEM;  /* CA-3 */
extern const bench_test SELFTEST_CRASH;     /* F5: crash di un figlio */
extern const bench_test SELFTEST_HANG;      /* CA-21 */

#endif /* TEST_SELFTEST_H */
