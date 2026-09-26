#ifndef NAMC_MODEL_CURRENT_H
#define NAMC_MODEL_CURRENT_H

#include "namc/flux_map.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NAMC_MODEL_CURRENT_VERSION 1U

typedef enum namc_model_failure_policy {
    NAMC_MODEL_DISABLE = 0,
    NAMC_MODEL_NOMINAL_FALLBACK = 1
} namc_model_failure_policy_t;

typedef struct namc_model_current_config {
    unsigned int version;
    /* Independently supplied gains, nominal fallback priors and hard limits.
     * Neither limits nor gains are inferred from the map in this first slice. */
    namc_current_config_t nominal;
    const namc_flux_map_t *map; /* Accepted controller model, never plant truth. */
    namc_model_failure_policy_t failure_policy;
} namc_model_current_config_t;

typedef struct namc_model_current_state {
    namc_current_state_t pi;
    int fallback_latched; /* Once entered, only an explicit reset permits map use. */
    namc_flux_result_t model_failure; /* First rejected lookup, retained until reset. */
} namc_model_current_state_t;

NAMC_CORE_API void namc_model_current_reset(namc_model_current_state_t *state);

/* Fixed-gain PI with coupled flux-based rotational compensation at measured
 * currents. No derivatives, fit, gain scheduling, or allocation in this path.
 * A lookup failure either disables/latches or explicitly latches nominal PI
 * fallback, resetting integrals once. Fallback is not a bumpless transition
 * or hardware safety claim. Measurement/hard-limit failures always disable.
 * Map storage must remain valid and immutable as required by flux_map.h. */
NAMC_CORE_API namc_result_t namc_model_current_step(
    const namc_model_current_config_t *config, namc_model_current_state_t *state,
    namc_dq_t reference, namc_abc_t measured_current, double electrical_angle,
    double electrical_speed, double vdc, namc_duty_t *out);

#ifdef __cplusplus
}
#endif
#endif
