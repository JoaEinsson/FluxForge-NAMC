#include "namc/linear_plant.h"

#include <math.h>
#include <stddef.h>

static int namc_parameters_valid(const namc_linear_parameters_t *p)
{
    return p != NULL && p->model_version == NAMC_REFERENCE_MODEL_VERSION &&
        isfinite(p->resistance) && p->resistance >= 0.0 &&
        isfinite(p->ld) && p->ld > 0.0 && isfinite(p->lq) && p->lq > 0.0 &&
        isfinite(p->pm_flux) && p->pm_flux >= 0.0 &&
        isfinite(p->inertia) && p->inertia > 0.0 &&
        isfinite(p->friction) && p->friction >= 0.0 && p->pole_pairs > 0U;
}

static int namc_state_valid(const namc_linear_state_t *s)
{
    return s != NULL && isfinite(s->id) && isfinite(s->iq) &&
        isfinite(s->speed) && isfinite(s->angle);
}

namc_result_t namc_linear_flux_torque(const namc_linear_parameters_t *p,
    namc_dq_t current, namc_dq_t *flux, double *torque)
{
    namc_dq_t f;
    double t;
    if (!namc_parameters_valid(p) || flux == NULL || torque == NULL ||
        !isfinite(current.d) || !isfinite(current.q)) {
        return NAMC_INVALID_INPUT;
    }
    f.d = p->ld * current.d + p->pm_flux;
    f.q = p->lq * current.q;
    t = 1.5 * (double)p->pole_pairs * (f.d * current.q - f.q * current.d);
    if (!isfinite(f.d) || !isfinite(f.q) || !isfinite(t)) {
        return NAMC_INVALID_INPUT;
    }
    *flux = f;
    *torque = t;
    return NAMC_OK;
}

namc_result_t namc_linear_derivative(const namc_linear_parameters_t *p,
    const namc_linear_state_t *state, namc_ab_t voltage, double load_torque,
    namc_linear_state_t *derivative)
{
    namc_dq_t dq_voltage, flux, current;
    namc_linear_state_t d;
    double torque, we;
    if (!namc_state_valid(state) || derivative == NULL || !isfinite(load_torque) ||
        namc_park(voltage, state->angle, &dq_voltage) != NAMC_OK) {
        return NAMC_INVALID_INPUT;
    }
    current.d = state->id;
    current.q = state->iq;
    if (namc_linear_flux_torque(p, current, &flux, &torque) != NAMC_OK) {
        return NAMC_INVALID_INPUT;
    }
    we = (double)p->pole_pairs * state->speed;
    d.id = (dq_voltage.d - p->resistance * state->id + we * flux.q) / p->ld;
    d.iq = (dq_voltage.q - p->resistance * state->iq - we * flux.d) / p->lq;
    d.speed = (torque - load_torque - p->friction * state->speed) / p->inertia;
    d.angle = we;
    if (!namc_state_valid(&d)) {
        return NAMC_INVALID_INPUT;
    }
    *derivative = d;
    return NAMC_OK;
}

static namc_linear_state_t namc_offset(namc_linear_state_t s,
    namc_linear_state_t d, double dt)
{
    s.id += dt * d.id;
    s.iq += dt * d.iq;
    s.speed += dt * d.speed;
    s.angle += dt * d.angle;
    return s;
}

namc_result_t namc_linear_step(const namc_linear_parameters_t *p,
    namc_linear_state_t *state, namc_ab_t voltage, double load_torque, double dt)
{
    namc_linear_state_t k1, k2, k3, k4, stage, next, mean;
    const double two_pi = 6.2831853071795864769;
    if (!isfinite(dt) || dt <= 0.0 ||
        namc_linear_derivative(p, state, voltage, load_torque, &k1) != NAMC_OK) {
        return NAMC_INVALID_INPUT;
    }
    stage = namc_offset(*state, k1, dt / 2.0);
    if (namc_linear_derivative(p, &stage, voltage, load_torque, &k2) != NAMC_OK) {
        return NAMC_INVALID_INPUT;
    }
    stage = namc_offset(*state, k2, dt / 2.0);
    if (namc_linear_derivative(p, &stage, voltage, load_torque, &k3) != NAMC_OK) {
        return NAMC_INVALID_INPUT;
    }
    stage = namc_offset(*state, k3, dt);
    if (namc_linear_derivative(p, &stage, voltage, load_torque, &k4) != NAMC_OK) {
        return NAMC_INVALID_INPUT;
    }
    mean.id = k1.id / 6.0 + k2.id / 3.0 + k3.id / 3.0 + k4.id / 6.0;
    mean.iq = k1.iq / 6.0 + k2.iq / 3.0 + k3.iq / 3.0 + k4.iq / 6.0;
    mean.speed = k1.speed / 6.0 + k2.speed / 3.0 + k3.speed / 3.0 + k4.speed / 6.0;
    mean.angle = k1.angle / 6.0 + k2.angle / 3.0 + k3.angle / 3.0 + k4.angle / 6.0;
    next = namc_offset(*state, mean, dt);
    if (!namc_state_valid(&next)) {
        return NAMC_INVALID_INPUT;
    }
    next.angle = fmod(next.angle, two_pi);
    if (next.angle < 0.0) {
        next.angle += two_pi;
    }
    /* Validate the resulting state before committing the transaction. */
    if (namc_linear_derivative(p, &next, voltage, load_torque, &stage) != NAMC_OK) {
        return NAMC_INVALID_INPUT;
    }
    *state = next;
    return NAMC_OK;
}

namc_result_t namc_average_inverter(namc_duty_t duty, double vdc,
    namc_ab_t *voltage)
{
    double mean;
    namc_abc_t phase;
    if (voltage == NULL) {
        return NAMC_INVALID_INPUT;
    }
    voltage->alpha = 0.0;
    voltage->beta = 0.0;
    if (!isfinite(vdc) || vdc <= 0.0 || !isfinite(duty.a) ||
        !isfinite(duty.b) || !isfinite(duty.c) || duty.a < 0.0 || duty.a > 1.0 ||
        duty.b < 0.0 || duty.b > 1.0 || duty.c < 0.0 || duty.c > 1.0 ||
        (duty.enabled != 0 && duty.enabled != 1)) {
        return NAMC_INVALID_INPUT;
    }
    if (duty.enabled == 0) {
        return NAMC_OK;
    }
    mean = (duty.a + duty.b + duty.c) / 3.0;
    phase.a = vdc * (duty.a - mean);
    phase.b = vdc * (duty.b - mean);
    phase.c = vdc * (duty.c - mean);
    return namc_clarke(phase, voltage);
}
