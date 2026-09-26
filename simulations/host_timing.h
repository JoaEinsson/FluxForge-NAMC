#ifndef NAMC_HOST_TIMING_H
#define NAMC_HOST_TIMING_H

#include <stdint.h>

/* Host diagnostics only. Never linked into the portable core or plant. */
int namc_host_timer_init(double *seconds_per_tick);
int namc_host_timer_read(uint64_t *ticks);
const char *namc_host_timer_name(void);

#endif
