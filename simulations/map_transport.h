#ifndef NAMC_SIM_MAP_TRANSPORT_H
#define NAMC_SIM_MAP_TRANSPORT_H

#include "namc/flux_map.h"

/* Host-only private stdin bridge for the Python JSON importer. Not a public
 * serialization format. The single map owns static storage for this process. */
int namc_sim_read_map(namc_flux_map_t *map);
void namc_sim_print_map(const namc_flux_map_t *map);

#endif
