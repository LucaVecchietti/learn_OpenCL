/**
 * main.c — punto di ingresso di minibanchmark (F11).
 * Identico in ogni progetto che usa bench/: cambiano solo tests.h / tests.c.
 */

#include "bench/bench.h"
#include "tests/tests.h"

int main(int argc, char **argv)
{
    bench_options options = BENCH_OPTIONS_DEFAULT; /* 10 s, 1+5 esecuzioni, 120 s inattività */
    return bench_main(argc, argv, BENCH_TESTS, BENCH_TESTS_COUNT, &options);
}
