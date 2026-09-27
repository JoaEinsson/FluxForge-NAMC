#ifndef NAMC_FLUX_ID_H
#define NAMC_FLUX_ID_H

#include "namc/resistance_id.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Experimental observable-only point acquisition, not a fitted/active map.
 * SI, amplitude-invariant dq. Caller-owned state must not be modified active. */
#define NAMC_FLUX_ID_VERSION 1U

typedef enum namc_fi_phase {
    NAMC_FI_IDLE = 0, NAMC_FI_SETTLING, NAMC_FI_SAMPLING,
    NAMC_FI_COMPLETE, NAMC_FI_DEGRADED, NAMC_FI_FAULT
} namc_fi_phase_t;

typedef enum namc_fi_reason {
    NAMC_FI_NONE = 0, NAMC_FI_CONFIG, NAMC_FI_OBSERVATION, NAMC_FI_LIMIT,
    NAMC_FI_SOURCE, NAMC_FI_UNSETTLED, NAMC_FI_TRACKING, NAMC_FI_UNOBSERVABLE,
    NAMC_FI_RANGE, NAMC_FI_OFFSET, NAMC_FI_UNCERTAINTY, NAMC_FI_VALIDATION,
    NAMC_FI_TIMEOUT, NAMC_FI_ABORTED, NAMC_FI_REFERENCE
} namc_fi_reason_t;

typedef struct namc_fi_config {
    unsigned int version;
    double sample_time;
    namc_dq_t target_current;
    double speeds[3]; /* Electrical rad/s: positive, negative, distinct positive holdout. */
    unsigned int settle_samples, measure_samples, max_samples;
    double resistance, resistance_uncertainty; /* Independent accepted estimate, ohm. */
    double current_uncertainty, voltage_uncertainty, speed_uncertainty;
    /* Independent allowances for omitted dynamics/alignment (V), and a local
     * Lipschitz bound |delta(psi_axis)| <= gradient_bound * |delta(i_dq)| (H).
     * Neither is learned from hidden truth or verified by this procedure. */
    double model_voltage_error, gradient_bound;
    double max_current_span, max_voltage_span, max_speed_span;
    double max_tracking_error, max_speed_error;
    double min_abs_speed, min_speed_separation;
    double max_flux, max_flux_uncertainty, max_offset_voltage, validation_tolerance;
    double max_origin_q_flux;
} namc_fi_config_t;

typedef struct namc_fi_limits {
    double max_current, max_voltage, max_electrical_speed, max_apparent_energy;
} namc_fi_limits_t;

typedef struct namc_fi_window {
    unsigned int count;
    /* Coordinates: id, iq, vd, vq, electrical speed. */
    double mean[5], minimum[5], maximum[5];
} namc_fi_window_t;

typedef struct namc_fi_result {
    namc_dq_t flux, uncertainty; /* Wb, conditional absolute bounds at target current. */
    namc_dq_t voltage_offset, holdout_residual; /* V, original vd/vq signs. */
    int pm_reference; /* Only an accepted origin point with positive aligned d flux. */
} namc_fi_result_t;

typedef struct namc_fi_state {
    unsigned int version;
    namc_fi_config_t config;
    namc_fi_limits_t limits;
    namc_fi_phase_t phase;
    namc_fi_reason_t reason;
    unsigned int stage, stage_samples, elapsed_samples;
    double apparent_energy;
    namc_rs_voltage_source_t voltage_source;
    namc_fi_window_t windows[3];
    namc_fi_result_t result;
} namc_fi_state_t;

NAMC_CORE_API namc_result_t namc_fi_start(const namc_fi_config_t *config,
    const namc_fi_limits_t *limits, namc_fi_state_t *state);
/* REQUESTS only: route both through independent actuator/shaft supervision.
 * Return 0 requires stopping excitation, not continuing a zero-reference PI. */
NAMC_CORE_API int namc_fi_request(const namc_fi_state_t *state,
    namc_dq_t *current, double *electrical_speed);
NAMC_CORE_API namc_fi_phase_t namc_fi_observe(namc_fi_state_t *state,
    const namc_rs_observation_t *observation);
NAMC_CORE_API void namc_fi_abort(namc_fi_state_t *state);

#ifdef __cplusplus
}
#endif
#endif
