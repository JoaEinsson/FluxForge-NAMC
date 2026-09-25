#ifndef NAMC_LINEAR_PLANT_H
#define NAMC_LINEAR_PLANT_H

#include "namc/reference.h"

/* Simulator/test-only truth. This include directory is NOT part of namc_core. */
typedef struct namc_linear_parameters {
    unsigned int model_version;
    double resistance; /* ohm per phase */
    double ld, lq; /* H */
    double pm_flux; /* Wb */
    double inertia; /* kg m^2 */
    double friction; /* N m s/rad, viscous */
    unsigned int pole_pairs;
} namc_linear_parameters_t;

typedef struct namc_linear_state {
    double id, iq; /* A */
    double speed; /* mechanical rad/s */
    double angle; /* electrical rad; integrated with p * mechanical speed */
} namc_linear_state_t;

/* All errors leave caller state/output unchanged, except inverter output,
 * which is set to zero on failure. load_torque is signed N m opposing positive
 * rotation. Stationary voltage is held over each fixed-step RK4 interval. */
namc_result_t namc_linear_flux_torque(const namc_linear_parameters_t *p,
    namc_dq_t current, namc_dq_t *flux, double *torque);
namc_result_t namc_linear_derivative(const namc_linear_parameters_t *p,
    const namc_linear_state_t *state, namc_ab_t voltage, double load_torque,
    namc_linear_state_t *derivative);
namc_result_t namc_linear_step(const namc_linear_parameters_t *p,
    namc_linear_state_t *state, namc_ab_t voltage, double load_torque, double dt);
namc_result_t namc_average_inverter(namc_duty_t duty, double vdc,
    namc_ab_t *voltage);

#endif /* NAMC_LINEAR_PLANT_H */
