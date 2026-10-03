/**
 * bench_proc.c — parti dipendenti dal sistema operativo (F5).
 *
 * B2 [P]: il codice specifico del sistema operativo sta solo qui.
 * Passo 2 di B6: solo l'orologio monotonico; processi e timeout arrivano al passo 3.
 */

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#else
#error "sistema operativo non supportato"
#endif

#include "bench_internal.h"

double bench_clock_ns(void)
{
#if defined(_WIN32) || defined(_WIN64)
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1e9 / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
#endif
}
