#include "namc/magnetic_plant.h"

#include <math.h>

static int namc_magnetic_state_valid(const namc_magnetic_state_t *s)
{
    return s != NULL && isfinite(s->id) && isfinite(s->iq) &&
        isfinite(s->speed) && isfinite(s->angle);
}

namc_flux_result_t namc_magnetic_derivative(const namc_magnetic_parameters_t *p,
    const namc_magnetic_state_t *state, namc_ab_t voltage, double load_torque,
    namc_magnetic_state_t *derivative)
{
    namc_dq_t v, current, flux, rhs, rate;
    namc_flux_jacobian_t j;
    namc_magnetic_state_t result;
    namc_flux_result_t status;
    double we, torque;
    if (p == NULL || p->version != NAMC_MAGNETIC_PLANT_VERSION ||
        !isfinite(p->resistance) || p->resistance < 0.0 ||
        !isfinite(p->inertia) || p->inertia <= 0.0 ||
        !isfinite(p->friction) || p->friction < 0.0 || p->pole_pairs == 0U ||
        !namc_magnetic_state_valid(state) || derivative == NULL || !isfinite(load_torque) ||
        namc_park(voltage, state->angle, &v) != NAMC_OK) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    current = (namc_dq_t){state->id, state->iq};
    status = namc_flux_map_lookup(p->map, current, &flux, &j);
    if (status != NAMC_FLUX_OK) {
        return status;
    }
    we = (double)p->pole_pairs * state->speed;
    rhs.d = v.d - p->resistance * current.d + we * flux.q;
    rhs.q = v.q - p->resistance * current.q - we * flux.d;
    status = namc_flux_solve(j, p->map->limits.min_rcond, rhs, &rate);
    if (status != NAMC_FLUX_OK) {
        return status;
    }
    status = namc_flux_torque(flux, current, p->pole_pairs, &torque);
    if (status != NAMC_FLUX_OK) {
        return status;
    }
    result.id = rate.d;
    result.iq = rate.q;
    result.speed = (torque - load_torque - p->friction * state->speed) / p->inertia;
    result.angle = we;
    if (!namc_magnetic_state_valid(&result)) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    *derivative = result;
    return NAMC_FLUX_OK;
}

static namc_magnetic_state_t namc_magnetic_offset(namc_magnetic_state_t state,
    namc_magnetic_state_t rate, double dt)
{
    state.id += dt * rate.id;
    state.iq += dt * rate.iq;
    state.speed += dt * rate.speed;
    state.angle += dt * rate.angle;
    return state;
}

namc_flux_result_t namc_magnetic_step(const namc_magnetic_parameters_t *p,
    namc_magnetic_state_t *state, namc_ab_t voltage, double load_torque, double dt)
{
    namc_magnetic_state_t k1, k2, k3, k4, stage, mean, next;
    namc_flux_result_t result;
    const double two_pi = 6.2831853071795864769;
    if (!isfinite(dt) || dt <= 0.0) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    result = namc_magnetic_derivative(p, state, voltage, load_torque, &k1);
    if (result != NAMC_FLUX_OK) {
        return result;
    }
    stage = namc_magnetic_offset(*state, k1, dt / 2.0);
    result = namc_magnetic_derivative(p, &stage, voltage, load_torque, &k2);
    if (result != NAMC_FLUX_OK) {
        return result;
    }
    stage = namc_magnetic_offset(*state, k2, dt / 2.0);
    result = namc_magnetic_derivative(p, &stage, voltage, load_torque, &k3);
    if (result != NAMC_FLUX_OK) {
        return result;
    }
    stage = namc_magnetic_offset(*state, k3, dt);
    result = namc_magnetic_derivative(p, &stage, voltage, load_torque, &k4);
    if (result != NAMC_FLUX_OK) {
        return result;
    }
    mean.id = k1.id / 6.0 + k2.id / 3.0 + k3.id / 3.0 + k4.id / 6.0;
    mean.iq = k1.iq / 6.0 + k2.iq / 3.0 + k3.iq / 3.0 + k4.iq / 6.0;
    mean.speed = k1.speed / 6.0 + k2.speed / 3.0 + k3.speed / 3.0 + k4.speed / 6.0;
    mean.angle = k1.angle / 6.0 + k2.angle / 3.0 + k3.angle / 3.0 + k4.angle / 6.0;
    next = namc_magnetic_offset(*state, mean, dt);
    if (!namc_magnetic_state_valid(&next)) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    next.angle = fmod(next.angle, two_pi);
    if (next.angle < 0.0) {
        next.angle += two_pi;
    }
    result = namc_magnetic_derivative(p, &next, voltage, load_torque, &stage);
    if (result != NAMC_FLUX_OK) {
        return result;
    }
    *state = next;
    return NAMC_FLUX_OK;
}
