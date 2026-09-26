#ifndef NAMC_CURRENT_TUNING_H
#define NAMC_CURRENT_TUNING_H

#include "namc/flux_map.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Experimental offline design aid; no stability, hardware or ABI promise. */
#define NAMC_CURRENT_TUNING_VERSION 1U

typedef enum namc_tuning_result {
    NAMC_TUNING_OK = 0,
    NAMC_TUNING_INVALID_INPUT,
    NAMC_TUNING_MODEL_REJECTED,
    NAMC_TUNING_INFEASIBLE
} namc_tuning_result_t;

#define NAMC_TUNING_BANDWIDTH_CAP 1U
#define NAMC_TUNING_SAMPLE_CAP 2U
#define NAMC_TUNING_KP_CAP 4U
#define NAMC_TUNING_KI_CAP 8U
#define NAMC_TUNING_VOLTAGE_CAP 16U

typedef struct namc_tuning_request {
    unsigned int version;
    namc_dq_t operating_current; /* A, explicit design point, not plant truth. */
    double electrical_speed; /* rad/s at design point. */
    double bus_voltage; /* V at design point. */
    double resistance; /* ohm, positive independent estimate, not identified here. */
    double requested_bandwidth; /* rad/s, design parameter, not achieved bandwidth. */
    double min_bandwidth, max_bandwidth; /* rad/s, caller's admissible range. */
    double max_rate_sample; /* dimensionless local rate*Ts ceiling, (0, 0.25]. */
    double max_kp; /* V/A, each axis. */
    double max_ki; /* V/(A s), each axis. */
    double design_error; /* A, dq vector-error budget around the design point. */
    double voltage_fraction; /* (0,1], fraction of estimated voltage headroom. */
} namc_tuning_request_t;

typedef struct namc_tuning_candidate {
    double kp_d, kp_q, ki_d, ki_q;
    double bandwidth; /* rad/s after caps. */
    namc_flux_jacobian_t jacobian; /* Original full local J; no symmetrization. */
    double inverse_norm; /* ||J^-1||_inf, 1/H. */
    double coupling_factor; /* ||J^-1 diag(Jdd,Jqq)||_inf. */
    double rate_sample; /* Ts * (R*inverse_norm + bandwidth*coupling_factor). */
    double voltage_headroom; /* V, before voltage_fraction. */
    unsigned int caps; /* Caps below the requested bandwidth; may include ties. */
} namc_tuning_candidate_t;

/* Offline only, one bounded lookup and two guarded solves, no allocation or
 * gain-search iteration. base supplies read-only independent sample time/hard limits.
 * Returns gains only: never changes base, map, PI state, or active gains.
 * Output is unchanged on every failure; caller must not activate stale output.
 * Apply only before enabling a reset controller, then validate closed-loop
 * behavior separately. This local design is not runtime gain scheduling. */
NAMC_CORE_API namc_tuning_result_t namc_current_tune(
    const namc_flux_map_t *map, const namc_current_config_t *base,
    const namc_tuning_request_t *request, namc_tuning_candidate_t *out);

#ifdef __cplusplus
}
#endif
#endif
