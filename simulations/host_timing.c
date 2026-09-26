#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "host_timing.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int namc_host_timer_init(double *seconds_per_tick)
{
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) { return 0; }
    *seconds_per_tick = 1.0 / (double)frequency.QuadPart;
    return 1;
}

int namc_host_timer_read(uint64_t *ticks)
{
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter) || counter.QuadPart < 0) { return 0; }
    *ticks = (uint64_t)counter.QuadPart;
    return 1;
}

const char *namc_host_timer_name(void) { return "QueryPerformanceCounter"; }
#else
#include <time.h>

int namc_host_timer_init(double *seconds_per_tick)
{
    struct timespec resolution;
    if (clock_getres(CLOCK_MONOTONIC, &resolution) != 0) { return 0; }
    *seconds_per_tick = 1e-9; /* Timestamp representation, not measured resolution. */
    return 1;
}

int namc_host_timer_read(uint64_t *ticks)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0) { return 0; }
    *ticks = (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
    return 1;
}

const char *namc_host_timer_name(void) { return "clock_gettime(CLOCK_MONOTONIC)"; }
#endif
