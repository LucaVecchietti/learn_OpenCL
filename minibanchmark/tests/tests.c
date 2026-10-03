/**
 * tests.c — registro dei test.
 *
 * Per aggiungere un test: scrivere il .cl, definire il descrittore
 * (qui per la famiglia 1D, in un test_*.c per gli altri) e aggiungerlo all'array.
 */

#include "tests.h"

/* Passo 1 di B6: nessun test ancora.
 * C11 non ammette array vuoti: si usa un elemento NULL e il conteggio a 0. */
const bench_test *const BENCH_TESTS[] = {
    NULL
};

const size_t BENCH_TESTS_COUNT = 0;
