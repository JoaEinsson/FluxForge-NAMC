#ifndef NAMC_RESISTANCE_ID_H
#define NAMC_RESISTANCE_ID_H

#include "namc/reference.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Experimental C API, no ABI/serialization commitment. All quantities SI. */
#define NAMC_RESISTANCE_ID_VERSION 1U
#define NAMC_RESISTANCE_MAX_SAMPLES 1000000U

typedef enum namc_rs_phase {
    NAMC_RS_IDLE = 0, NAMC_RS_SETTLING, NAMC_RS_SAMPLING,
    NAMC_RS_COMPLETE, NAMC_RS_DEGRADED, NAMC_RS_FAULT
} namc_rs_phase_t;

typedef enum namc_rs_reason {
    NAMC_RS_NONE = 0, NAMC_RS_BAD_CONFIG, NAMC_RS_BAD_OBSERVATION,
    NAMC_RS_LIMIT, NAMC_RS_UNTRUSTED_VOLTAGE, NAMC_RS_UNSETTLED,
    NAMC_RS_TRACKING, NAMC_RS_NO_EXCITATION, NAMC_RS_RANGE,
    NAMC_RS_OFFSET, NAMC_RS_UNCERTAINTY, NAMC_RS_VALIDATION,
    NAMC_RS_TIMEOUT, NAMC_RS_ABORTED
} namc_rs_reason_t;

typedef enum namc_rs_voltage_source {
    NAMC_RS_COMMAND_ONLY = 0, NAMC_RS_IDEAL_TERMINAL,
    NAMC_RS_MEASURED_TERMINAL, NAMC_RS_CALIBRATED_RECONSTRUCTION
} namc_rs_voltage_source_t;

typedef struct namc_rs_config {
    unsigned int version;
    double sample_time;
    double levels[3]; /* d-axis A: two distinct positive fit levels, negative holdout. */
    unsigned int settle_samples, measure_samples, max_samples;
    double min_current_separation;
    double max_current_span, max_voltage_span;
    double max_tracking_error, max_quadrature_current, max_quadrature_voltage;
    double min_resistance, max_resistance;
    double max_offset_voltage, max_resistance_uncertainty, validation_voltage_tolerance;
    /* Declared absolute sensor/reconstruction error bounds, not inferred noise. */
    double current_uncertainty, voltage_uncertainty;
} namc_rs_config_t;

typedef struct namc_rs_limits {
    /* Independently supplied, copied at start, never adapted by the identifier. */
    double max_current, max_voltage, max_electrical_speed;
    double max_apparent_energy; /* J: integral of 1.5*|v_dq|*|i_dq|, not thermal proof. */
} namc_rs_limits_t;

typedef struct namc_rs_observation {
    unsigned int sequence; /* Exactly 1,2,...; one aligned interval per call. */
    namc_dq_t current, terminal_voltage;
    double electrical_speed;
    namc_rs_voltage_source_t voltage_source;
} namc_rs_observation_t;

typedef struct namc_rs_window {
    unsigned int count;
    double mean_current, mean_voltage;
    double min_current, max_current, min_voltage, max_voltage;
} namc_rs_window_t;

typedef struct namc_rs_result {
    double resistance, lower, upper; /* ohm; conditional measurement bounds, not probability. */
    double offset_voltage, validation_residual;
} namc_rs_result_t;

typedef struct namc_rs_state {
    /* Caller-owned storage, initialize with start; do not mutate while active. */
    unsigned int version;
    namc_rs_config_t config;
    namc_rs_limits_t limits;
    namc_rs_phase_t phase;
    namc_rs_reason_t reason;
    unsigned int stage, elapsed_samples, stage_samples;
    double apparent_energy;
    namc_rs_voltage_source_t voltage_source;
    namc_rs_window_t windows[3];
    namc_rs_result_t result; /* Nonzero result published only on COMPLETE. */
} namc_rs_state_t;

/* Explicit start/restart; no borrowed model, plant, map or resistance input. */
NAMC_CORE_API namc_result_t namc_rs_start(const namc_rs_config_t *config,
    const namc_rs_limits_t *limits, namc_rs_state_t *state);
/* Returns a current REQUEST, never PWM. Caller must enforce independent guards
 * and disable the actuator when this returns 0, including COMPLETE/DEGRADED. */
NAMC_CORE_API int namc_rs_request(const namc_rs_state_t *state, namc_dq_t *request);
NAMC_CORE_API namc_rs_phase_t namc_rs_observe(namc_rs_state_t *state,
    const namc_rs_observation_t *observation);
NAMC_CORE_API void namc_rs_abort(namc_rs_state_t *state);

#ifdef __cplusplus
}
#endif
#endif
