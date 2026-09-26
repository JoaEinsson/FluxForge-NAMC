#ifndef NAMC_SIM_MAP_TRANSPORT_H
#define NAMC_SIM_MAP_TRANSPORT_H

#include "namc/flux_map.h"

/* Host-only private stdin bridge for the Python JSON importer. Not a public
 * serialization format. Slots 0 and 1 have disjoint static storage for plant
 * and controller, respectively. Read each slot at most once per process. */
int namc_sim_read_map(namc_flux_map_t *map, unsigned int slot);
int namc_sim_map_end(void);
void namc_sim_print_map(const namc_flux_map_t *map);

#endif
