#ifndef NAMC_REFERENCE_H
#define NAMC_REFERENCE_H

#include "namc/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Experimental simulation baseline, not a stable API or nonlinear model.
 * Amplitude-invariant, three-wire abc -> alpha/beta -> rotor d/q.
 * Angles are electrical radians; currents A, voltages V, time s. */
#define NAMC_REFERENCE_MODEL_VERSION 1U

typedef enum namc_result {
    NAMC_OK = 0,
    NAMC_INVALID_INPUT,
    NAMC_LIMIT_EXCEEDED,
    NAMC_FAULT_LATCHED,
    NAMC_MODEL_REJECTED
} namc_result_t;

typedef struct namc_abc { double a, b, c; } namc_abc_t;
typedef struct namc_ab { double alpha, beta; } namc_ab_t;
typedef struct namc_dq { double d, q; } namc_dq_t;
typedef struct namc_duty {
    double a, b, c;
    int enabled; /* Exactly 0 or 1. Disabled is not a hardware safe-state claim. */
} namc_duty_t;

/* Transforms project out zero sequence and leave output unchanged on error. */
NAMC_CORE_API namc_result_t namc_clarke(namc_abc_t in, namc_ab_t *out);
NAMC_CORE_API namc_result_t namc_inverse_clarke(namc_ab_t in, namc_abc_t *out);
NAMC_CORE_API namc_result_t namc_park(namc_ab_t in, double angle, namc_dq_t *out);
NAMC_CORE_API namc_result_t namc_inverse_park(namc_dq_t in, double angle,
                                            namc_ab_t *out);

/* Radially limit to the SVPWM linear circle vdc/sqrt(3), then inject common
 * mode. All failures return neutral duties with enabled=0 if out is non-null. */
NAMC_CORE_API namc_result_t namc_modulate(namc_ab_t voltage, double vdc,
                                        namc_duty_t *out);

typedef struct namc_current_config {
    unsigned int model_version;
    double sample_time;
    double kp_d, kp_q, ki_d, ki_q, antiwindup_gain;
    double ld, lq, pm_flux; /* Controller priors: H, H, Wb; never plant truth. */
    double current_limit; /* A, both phase peak and dq vector magnitude. */
    double min_bus_voltage, max_bus_voltage;
    double electrical_speed_limit; /* rad/s, absolute. */
} namc_current_config_t;

typedef struct namc_current_state {
    double integral_d, integral_q; /* V */
    int faulted;
} namc_current_state_t;

/* Explicit reset; the caller must resolve the cause before re-enabling. */
NAMC_CORE_API void namc_current_reset(namc_current_state_t *state);

/* PI + nominal decoupling, vector reference/voltage limits and back-calculation.
 * Invalid input, excessive measured current/speed or invalid bus latches a
 * fault; subsequent calls remain disabled until reset. Only measured signals
 * and caller-owned priors enter this API. Balanced phase currents required. */
NAMC_CORE_API namc_result_t namc_current_step(
    const namc_current_config_t *config, namc_current_state_t *state,
    namc_dq_t reference, namc_abc_t measured_current, double electrical_angle,
    double electrical_speed, double vdc, namc_duty_t *out);

#ifdef __cplusplus
}
#endif
#endif /* NAMC_REFERENCE_H */
