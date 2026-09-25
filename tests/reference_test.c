#include "namc/linear_plant.h"
#include "namc/reference.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); \
    return 0; } } while (0)

static int near(double a, double b, double tolerance)
{
    return isfinite(a) && isfinite(b) && fabs(a - b) <= tolerance;
}

static namc_linear_parameters_t plant_config(void)
{
    const namc_linear_parameters_t p = {
        NAMC_REFERENCE_MODEL_VERSION, 0.4, 0.002, 0.003, 0.05, 0.01, 0.001, 4U
    };
    return p;
}

static namc_current_config_t control_config(void)
{
    const namc_current_config_t c = {
        NAMC_REFERENCE_MODEL_VERSION, 0.00005, 3.0, 4.0, 600.0, 600.0, 500.0,
        0.0015, 0.002, 0.04, 12.0, 12.0, 60.0, 2000.0
    };
    return c;
}

static int disabled(namc_duty_t d)
{
    return d.enabled == 0 && d.a == 0.5 && d.b == 0.5 && d.c == 0.5;
}

static int transforms(void)
{
    const namc_abc_t basis = {1.0, -0.5, -0.5};
    namc_ab_t ab = {0.0, 0.0};
    namc_dq_t dq;
    namc_abc_t restored;
    unsigned int n;
    CHECK(namc_clarke(basis, &ab) == NAMC_OK);
    CHECK(near(ab.alpha, 1.0, 1e-15) && near(ab.beta, 0.0, 1e-15));
    CHECK(namc_park(ab, 1.5707963267948966, &dq) == NAMC_OK);
    CHECK(near(dq.d, 0.0, 1e-15) && near(dq.q, -1.0, 1e-15));
    for (n = 0U; n < 100U; ++n) {
        const double angle = 0.19 * (double)n - 8.0;
        const namc_dq_t current = {-2.5, 4.0}, voltage = {12.0, -3.0};
        namc_ab_t iab, vab;
        namc_abc_t iabc, vabc;
        double power;
        CHECK(namc_inverse_park(current, angle, &iab) == NAMC_OK);
        CHECK(namc_inverse_park(voltage, angle, &vab) == NAMC_OK);
        CHECK(namc_inverse_clarke(iab, &iabc) == NAMC_OK);
        CHECK(namc_inverse_clarke(vab, &vabc) == NAMC_OK);
        CHECK(namc_clarke(iabc, &ab) == NAMC_OK);
        CHECK(namc_park(ab, angle, &dq) == NAMC_OK);
        CHECK(near(dq.d, current.d, 1e-13) && near(dq.q, current.q, 1e-13));
        CHECK(namc_inverse_clarke(ab, &restored) == NAMC_OK);
        CHECK(near(restored.a, iabc.a, 1e-13));
        CHECK(near(restored.b, iabc.b, 1e-13));
        CHECK(near(restored.c, iabc.c, 1e-13));
        power = iabc.a * vabc.a + iabc.b * vabc.b + iabc.c * vabc.c;
        CHECK(near(power, 1.5 * (current.d * voltage.d + current.q * voltage.q), 1e-12));
    }
    ab.alpha = 17.0;
    CHECK(namc_clarke((namc_abc_t){NAN, 0.0, 0.0}, &ab) == NAMC_INVALID_INPUT);
    CHECK(ab.alpha == 17.0);
    CHECK(namc_park(ab, INFINITY, &dq) == NAMC_INVALID_INPUT);
    CHECK(namc_inverse_park((namc_dq_t){DBL_MAX, DBL_MAX}, 0.7, &ab) == NAMC_INVALID_INPUT);
    CHECK(namc_clarke(basis, NULL) == NAMC_INVALID_INPUT);
    return 1;
}

static int modulation(void)
{
    unsigned int n;
    namc_duty_t duty;
    namc_ab_t result;
    for (n = 0U; n < 360U; ++n) {
        double angle = (double)n * 0.017453292519943295;
        namc_ab_t request = {100.0 * cos(angle), 100.0 * sin(angle)};
        CHECK(namc_modulate(request, 48.0, &duty) == NAMC_OK);
        CHECK(duty.enabled == 1 && duty.a >= 0.0 && duty.a <= 1.0);
        CHECK(duty.b >= 0.0 && duty.b <= 1.0 && duty.c >= 0.0 && duty.c <= 1.0);
        CHECK(namc_average_inverter(duty, 48.0, &result) == NAMC_OK);
        CHECK(near(result.alpha, (48.0 / sqrt(3.0)) * cos(angle), 1e-12));
        CHECK(near(result.beta, (48.0 / sqrt(3.0)) * sin(angle), 1e-12));
        request.alpha *= 0.1;
        request.beta *= 0.1;
        CHECK(namc_modulate(request, 48.0, &duty) == NAMC_OK);
        CHECK(namc_average_inverter(duty, 48.0, &result) == NAMC_OK);
        CHECK(near(result.alpha, request.alpha, 1e-12));
        CHECK(near(result.beta, request.beta, 1e-12));
    }
    CHECK(namc_modulate((namc_ab_t){NAN, 0.0}, 48.0, &duty) == NAMC_INVALID_INPUT);
    CHECK(disabled(duty));
    CHECK(namc_modulate((namc_ab_t){0.0, 0.0}, 0.0, &duty) == NAMC_INVALID_INPUT);
    CHECK(disabled(duty));
    CHECK(namc_modulate((namc_ab_t){DBL_MAX, DBL_MAX}, 48.0, &duty) == NAMC_INVALID_INPUT);
    CHECK(disabled(duty));
    CHECK(namc_average_inverter(duty, 48.0, &result) == NAMC_OK);
    CHECK(result.alpha == 0.0 && result.beta == 0.0);
    duty.a = 1.01;
    CHECK(namc_average_inverter(duty, 48.0, &result) == NAMC_INVALID_INPUT);
    CHECK(result.alpha == 0.0 && result.beta == 0.0);
    return 1;
}

static int plant_physics(void)
{
    namc_linear_parameters_t p = plant_config();
    namc_linear_state_t s = {-2.0, 3.0, 10.0, 0.4}, derivative;
    namc_dq_t flux, current = {-2.0, 3.0}, v = {4.0, 6.0};
    namc_ab_t voltage;
    double torque, input_power, stored_power, losses;
    CHECK(namc_linear_flux_torque(&p, current, &flux, &torque) == NAMC_OK);
    CHECK(near(flux.d, 0.046, 1e-14) && near(flux.q, 0.009, 1e-14));
    CHECK(near(torque, 0.936, 1e-13));
    CHECK(namc_inverse_park(v, s.angle, &voltage) == NAMC_OK);
    CHECK(namc_linear_derivative(&p, &s, voltage, 0.2, &derivative) == NAMC_OK);
    CHECK(near(derivative.id, 2580.0, 1e-10));
    CHECK(near(derivative.iq, 986.6666666666667, 1e-10));
    CHECK(near(derivative.speed, 72.6, 1e-11));
    CHECK(near(derivative.angle, 40.0, 1e-12));
    input_power = 1.5 * (v.d * s.id + v.q * s.iq);
    stored_power = 1.5 * (p.ld * s.id * derivative.id + p.lq * s.iq * derivative.iq) +
                   p.inertia * s.speed * derivative.speed;
    losses = 1.5 * p.resistance * (s.id * s.id + s.iq * s.iq) +
             p.friction * s.speed * s.speed + 0.2 * s.speed;
    CHECK(near(input_power, stored_power + losses, 1e-11));
    p.model_version = 99U;
    CHECK(namc_linear_step(&p, &s, voltage, 0.0, 0.00005) == NAMC_INVALID_INPUT);
    CHECK(s.id == -2.0 && s.iq == 3.0 && s.speed == 10.0 && s.angle == 0.4);
    p = plant_config();
    p.ld = 0.0;
    CHECK(namc_linear_flux_torque(&p, current, &flux, &torque) == NAMC_INVALID_INPUT);
    p = plant_config();
    CHECK(namc_linear_step(&p, &s, (namc_ab_t){INFINITY, 0.0}, 0.0, 1e-5) == NAMC_INVALID_INPUT);
    CHECK(s.id == -2.0 && s.iq == 3.0);
    CHECK(namc_linear_step(&p, &s, voltage, 0.0, NAN) == NAMC_INVALID_INPUT);
    CHECK(namc_linear_step(&p, &s, voltage, 0.0, DBL_MAX) == NAMC_INVALID_INPUT);
    CHECK(s.id == -2.0 && s.angle == 0.4);
    return 1;
}

static int analytic_dynamics(void)
{
    namc_linear_parameters_t p = plant_config();
    namc_linear_state_t s = {0.0, 0.0, 0.0, 0.0};
    unsigned int n;
    double analytic;
    /* No PM, no saliency: torque is zero and the standstill d-axis is an RL circuit. */
    p.pm_flux = 0.0;
    p.lq = p.ld;
    for (n = 0U; n < 100U; ++n) {
        CHECK(namc_linear_step(&p, &s, (namc_ab_t){2.0, 0.0}, 0.0, 0.0001) == NAMC_OK);
    }
    analytic = (2.0 / p.resistance) * (1.0 - exp(-p.resistance * 0.01 / p.ld));
    CHECK(near(s.id, analytic, 3e-9));
    CHECK(s.iq == 0.0 && s.speed == 0.0);
    s = (namc_linear_state_t){0.0, 0.0, -10.0, 0.0};
    p.friction = 0.1;
    for (n = 0U; n < 100U; ++n) {
        CHECK(namc_linear_step(&p, &s, (namc_ab_t){0.0, 0.0}, 0.0, 0.001) == NAMC_OK);
    }
    CHECK(near(s.speed, -10.0 * exp(-1.0), 4e-9));
    CHECK(s.angle >= 0.0 && s.angle < 6.2831853071795864769);
    /* An isotropic lossless inductor has di_alpha/beta = v_alpha/beta / L,
     * even while its arbitrary rotor frame rotates. This checks RK stages. */
    p.resistance = 0.0;
    p.friction = 0.0;
    s = (namc_linear_state_t){0.0, 0.0, 10.0, 0.2};
    for (n = 0U; n < 100U; ++n) {
        CHECK(namc_linear_step(&p, &s, (namc_ab_t){2.0, 3.0}, 0.0, 0.0001) == NAMC_OK);
    }
    {
        namc_ab_t current;
        CHECK(namc_inverse_park((namc_dq_t){s.id, s.iq}, s.angle, &current) == NAMC_OK);
        CHECK(near(current.alpha, 10.0, 1e-8) && near(current.beta, 15.0, 1e-8));
        CHECK(near(s.speed, 10.0, 1e-12) && near(s.angle, 0.6, 1e-12));
    }
    return 1;
}

static int controller_guards(void)
{
    namc_current_config_t c = control_config();
    namc_current_state_t state;
    namc_duty_t duty;
    const namc_abc_t zero = {0.0, 0.0, 0.0};
    const namc_dq_t reference = {0.0, 5.0};
    unsigned int n;
    namc_current_reset(&state);
    CHECK(namc_current_step(&c, &state, reference, zero, 0.0, 0.0, 48.0, &duty) == NAMC_OK);
    CHECK(duty.enabled == 1);
    {
        namc_ab_t voltage;
        c.current_limit = 1.0;
        namc_current_reset(&state);
        CHECK(namc_current_step(&c, &state, (namc_dq_t){0.0, 100.0}, zero,
            0.0, 0.0, 48.0, &duty) == NAMC_OK);
        CHECK(namc_average_inverter(duty, 48.0, &voltage) == NAMC_OK);
        CHECK(near(voltage.alpha, 0.0, 1e-12) && near(voltage.beta, 4.0, 1e-12));
        c = control_config();
    }
    CHECK(namc_current_step(&c, &state, reference, zero, NAN, 0.0, 48.0, &duty) == NAMC_INVALID_INPUT);
    CHECK(disabled(duty));
    CHECK(namc_current_step(&c, &state, reference, zero, 0.0, 0.0, 48.0, &duty) == NAMC_FAULT_LATCHED);
    CHECK(disabled(duty));
    for (n = 0U; n < 9U; ++n) {
        namc_abc_t measured = zero;
        double vdc = 48.0, speed = 0.0;
        c = control_config();
        namc_current_reset(&state);
        switch (n) {
            case 0U: measured = (namc_abc_t){13.0, -6.5, -6.5}; break;
            case 1U: vdc = 0.0; break;
            case 2U: speed = 2001.0; break;
            case 3U: c.model_version = 0U; break;
            case 4U: c.ld = 0.0; break;
            case 5U: state.integral_q = INFINITY; break;
            case 6U: measured.c = 1.0; break;
            case 7U: c.ki_q = DBL_MAX; break;
            default: measured.a = NAN; break;
        }
        CHECK(namc_current_step(&c, &state, reference, measured, 0.0, speed, vdc, &duty) != NAMC_OK);
        CHECK(disabled(duty) && state.faulted == 1);
    }
    c = control_config();
    namc_current_reset(&state);
    CHECK(namc_current_step(&c, &state, reference, zero, 0.0, 0.0, 48.0, NULL) == NAMC_INVALID_INPUT);
    CHECK(state.faulted == 1);
    namc_current_reset(&state);
    CHECK(namc_current_step(&c, &state, reference, zero, 0.0, 0.0, 48.0, &duty) == NAMC_OK);
    {
        namc_ab_t ab;
        namc_abc_t phase;
        namc_dq_t dq;
        namc_current_reset(&state);
        CHECK(namc_inverse_park((namc_dq_t){-2.0, 3.0}, 0.4, &ab) == NAMC_OK);
        CHECK(namc_inverse_clarke(ab, &phase) == NAMC_OK);
        CHECK(namc_current_step(&c, &state, (namc_dq_t){-2.0, 3.0}, phase,
            0.4, 40.0, 48.0, &duty) == NAMC_OK);
        CHECK(namc_average_inverter(duty, 48.0, &ab) == NAMC_OK);
        CHECK(namc_park(ab, 0.4, &dq) == NAMC_OK);
        CHECK(near(dq.d, -0.24, 1e-12) && near(dq.q, 1.48, 1e-12));
    }
    return 1;
}

static int saturation_recovery(void)
{
    namc_current_config_t c = control_config();
    namc_linear_parameters_t p = plant_config();
    namc_linear_state_t s = {0.0, 0.0, 0.0, 0.0};
    namc_current_state_t state;
    unsigned int n;
    p.resistance = 5.0;
    p.pm_flux = 0.0;
    p.lq = p.ld;
    namc_current_reset(&state);
    for (n = 0U; n < 4000U; ++n) {
        namc_dq_t reference = {0.0, n < 2000U ? 100.0 : 0.0};
        namc_ab_t measured, voltage;
        namc_abc_t phase;
        namc_duty_t duty;
        CHECK(namc_inverse_park((namc_dq_t){s.id, s.iq}, s.angle, &measured) == NAMC_OK);
        CHECK(namc_inverse_clarke(measured, &phase) == NAMC_OK);
        CHECK(namc_current_step(&c, &state, reference, phase, s.angle, 0.0, 12.0, &duty) == NAMC_OK);
        CHECK(fabs(state.integral_d) <= 12.0 / sqrt(3.0) + 1e-12);
        CHECK(fabs(state.integral_q) <= 12.0 / sqrt(3.0) + 1e-12);
        CHECK(namc_average_inverter(duty, 12.0, &voltage) == NAMC_OK);
        CHECK(namc_linear_step(&p, &s, voltage, 0.0, c.sample_time) == NAMC_OK);
        CHECK(hypot(s.id, s.iq) < c.current_limit);
        if (n == 1999U) {
            CHECK(near(s.iq, (12.0 / sqrt(3.0)) / p.resistance, 1e-6));
        }
    }
    CHECK(fabs(s.id) < 0.001 && fabs(s.iq) < 0.001);
    return 1;
}

int main(void)
{
    if (!transforms() || !modulation() || !plant_physics() ||
        !analytic_dynamics() || !controller_guards() || !saturation_recovery()) {
        return 1;
    }
    puts("Linear reference: transforms, power, dynamics, limits and recovery passed.");
    return 0;
}
