#ifndef NAMC_MAGNETIC_PLANT_H
#define NAMC_MAGNETIC_PLANT_H

#include "namc/flux_map.h"
#include "namc/linear_plant.h"

#define NAMC_MAGNETIC_PLANT_VERSION 1U

/* Same state coordinates as the linear oracle, not its magnetic equations. */
typedef namc_linear_state_t namc_magnetic_state_t;

typedef struct namc_magnetic_parameters {
    unsigned int version;
    const namc_flux_map_t *map; /* Hidden plant map, never handed to a controller. */
    double resistance; /* ohm */
    double inertia; /* kg m^2 */
    double friction; /* N m s/rad */
    unsigned int pole_pairs;
} namc_magnetic_parameters_t;

/* Simulation-only current-state plant. Stationary voltage held for fixed-step
 * RK4. Every stage must be inside the map domain; no extrapolation/fallback.
 * Any error leaves output/state unchanged. Units and signs follow Phase 1. */
namc_flux_result_t namc_magnetic_derivative(const namc_magnetic_parameters_t *p,
    const namc_magnetic_state_t *state, namc_ab_t voltage, double load_torque,
    namc_magnetic_state_t *derivative);
namc_flux_result_t namc_magnetic_step(const namc_magnetic_parameters_t *p,
    namc_magnetic_state_t *state, namc_ab_t voltage, double load_torque, double dt);

#endif /* NAMC_MAGNETIC_PLANT_H */
