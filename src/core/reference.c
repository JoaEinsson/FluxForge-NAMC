#include "namc/reference.h"
#include "namc/model_current.h"

#include <math.h>
#include <stddef.h>

static const double namc_sqrt3 = 1.7320508075688772935;

static int namc_finite_ab(namc_ab_t v)
{
    return isfinite(v.alpha) && isfinite(v.beta);
}

static int namc_finite_dq(namc_dq_t v)
{
    return isfinite(v.d) && isfinite(v.q);
}

static double namc_clip(double x, double low, double high)
{
    return fmin(high, fmax(low, x));
}

static void namc_disable(namc_duty_t *out)
{
    if (out != NULL) {
        out->a = 0.5;
        out->b = 0.5;
        out->c = 0.5;
        out->enabled = 0;
    }
}

namc_result_t namc_clarke(namc_abc_t in, namc_ab_t *out)
{
    namc_ab_t v;
    if (out == NULL || !isfinite(in.a) || !isfinite(in.b) || !isfinite(in.c)) {
        return NAMC_INVALID_INPUT;
    }
    v.alpha = (2.0 / 3.0) * in.a - in.b / 3.0 - in.c / 3.0;
    v.beta = in.b / namc_sqrt3 - in.c / namc_sqrt3;
    if (!namc_finite_ab(v)) {
        return NAMC_INVALID_INPUT;
    }
    *out = v;
    return NAMC_OK;
}

namc_result_t namc_inverse_clarke(namc_ab_t in, namc_abc_t *out)
{
    namc_abc_t v;
    if (out == NULL || !namc_finite_ab(in)) {
        return NAMC_INVALID_INPUT;
    }
    v.a = in.alpha;
    v.b = -0.5 * in.alpha + (namc_sqrt3 / 2.0) * in.beta;
    v.c = -0.5 * in.alpha - (namc_sqrt3 / 2.0) * in.beta;
    if (!isfinite(v.b) || !isfinite(v.c)) {
        return NAMC_INVALID_INPUT;
    }
    *out = v;
    return NAMC_OK;
}

namc_result_t namc_park(namc_ab_t in, double angle, namc_dq_t *out)
{
    namc_dq_t v;
    double c, s;
    if (out == NULL || !namc_finite_ab(in) || !isfinite(angle)) {
        return NAMC_INVALID_INPUT;
    }
    c = cos(angle);
    s = sin(angle);
    v.d = c * in.alpha + s * in.beta;
    v.q = -s * in.alpha + c * in.beta;
    if (!namc_finite_dq(v)) {
        return NAMC_INVALID_INPUT;
    }
    *out = v;
    return NAMC_OK;
}

namc_result_t namc_inverse_park(namc_dq_t in, double angle, namc_ab_t *out)
{
    namc_ab_t v;
    double c, s;
    if (out == NULL || !namc_finite_dq(in) || !isfinite(angle)) {
        return NAMC_INVALID_INPUT;
    }
    c = cos(angle);
    s = sin(angle);
    v.alpha = c * in.d - s * in.q;
    v.beta = s * in.d + c * in.q;
    if (!namc_finite_ab(v)) {
        return NAMC_INVALID_INPUT;
    }
    *out = v;
    return NAMC_OK;
}

namc_result_t namc_modulate(namc_ab_t voltage, double vdc, namc_duty_t *out)
{
    namc_abc_t phase;
    double magnitude, limit, common;
    namc_disable(out);
    if (out == NULL || !namc_finite_ab(voltage) || !isfinite(vdc) || vdc <= 0.0) {
        return NAMC_INVALID_INPUT;
    }
    limit = vdc / namc_sqrt3;
    magnitude = hypot(voltage.alpha, voltage.beta);
    if (!isfinite(magnitude) || limit <= 0.0) {
        return NAMC_INVALID_INPUT;
    }
    if (magnitude > limit) {
        voltage.alpha *= limit / magnitude;
        voltage.beta *= limit / magnitude;
    }
    if (namc_inverse_clarke(voltage, &phase) != NAMC_OK) {
        return NAMC_INVALID_INPUT;
    }
    /* Normalize before adding extrema to avoid overflow with a large bus. */
    phase.a /= vdc;
    phase.b /= vdc;
    phase.c /= vdc;
    common = -0.5 * (fmax(phase.a, fmax(phase.b, phase.c)) +
                     fmin(phase.a, fmin(phase.b, phase.c)));
    out->a = namc_clip(0.5 + phase.a + common, 0.0, 1.0);
    out->b = namc_clip(0.5 + phase.b + common, 0.0, 1.0);
    out->c = namc_clip(0.5 + phase.c + common, 0.0, 1.0);
    out->enabled = 1;
    return NAMC_OK;
}

void namc_current_reset(namc_current_state_t *state)
{
    if (state != NULL) {
        state->integral_d = 0.0;
        state->integral_q = 0.0;
        state->faulted = 0;
    }
}

static int namc_config_valid(const namc_current_config_t *c)
{
    return c != NULL && c->model_version == NAMC_REFERENCE_MODEL_VERSION &&
        isfinite(c->sample_time) && c->sample_time > 0.0 &&
        isfinite(c->kp_d) && c->kp_d >= 0.0 &&
        isfinite(c->kp_q) && c->kp_q >= 0.0 &&
        isfinite(c->ki_d) && c->ki_d >= 0.0 &&
        isfinite(c->ki_q) && c->ki_q >= 0.0 &&
        isfinite(c->antiwindup_gain) && c->antiwindup_gain >= 0.0 &&
        c->antiwindup_gain * c->sample_time <= 1.0 &&
        isfinite(c->ld) && c->ld > 0.0 &&
        isfinite(c->lq) && c->lq > 0.0 &&
        isfinite(c->pm_flux) && c->pm_flux >= 0.0 &&
        isfinite(c->current_limit) && c->current_limit > 0.0 &&
        isfinite(c->min_bus_voltage) && c->min_bus_voltage > 0.0 &&
        isfinite(c->max_bus_voltage) && c->max_bus_voltage >= c->min_bus_voltage &&
        isfinite(c->electrical_speed_limit) && c->electrical_speed_limit > 0.0;
}

static namc_result_t namc_current_step_internal(
    const namc_current_config_t *config, namc_current_state_t *state,
    namc_dq_t reference, namc_abc_t measured_current, double electrical_angle,
    double electrical_speed, double vdc, namc_duty_t *out,
    int use_map, const namc_flux_map_t *map, namc_flux_result_t *model_failure)
{
    namc_ab_t stationary, command;
    namc_dq_t current, raw, limited;
    double magnitude, voltage_limit, ed, eq, next_d, next_q, balance;
    namc_result_t result = NAMC_INVALID_INPUT;
    namc_disable(out);
    if (state == NULL) {
        return NAMC_INVALID_INPUT;
    }
    if (state->faulted != 0) {
        return NAMC_FAULT_LATCHED;
    }
    if (!namc_config_valid(config) || out == NULL ||
        !namc_finite_dq(reference) || !isfinite(electrical_angle) ||
        !isfinite(electrical_speed) || !isfinite(vdc) ||
        !isfinite(state->integral_d) || !isfinite(state->integral_q) ||
        namc_clarke(measured_current, &stationary) != NAMC_OK ||
        namc_park(stationary, electrical_angle, &current) != NAMC_OK) {
        goto fault;
    }
    /* Relative phase-sum tolerance; no zero-sequence sensor fault is hidden. */
    balance = measured_current.a / config->current_limit +
              measured_current.b / config->current_limit +
              measured_current.c / config->current_limit;
    if (!isfinite(balance) || fabs(balance) > 1e-6) {
        goto fault;
    }
    if (vdc < config->min_bus_voltage || vdc > config->max_bus_voltage ||
        fabs(electrical_speed) > config->electrical_speed_limit ||
        fabs(measured_current.a) > config->current_limit ||
        fabs(measured_current.b) > config->current_limit ||
        fabs(measured_current.c) > config->current_limit ||
        hypot(current.d, current.q) > config->current_limit) {
        result = NAMC_LIMIT_EXCEEDED;
        goto fault;
    }
    magnitude = hypot(reference.d, reference.q);
    if (!isfinite(magnitude)) {
        goto fault;
    }
    if (magnitude > config->current_limit) {
        reference.d *= config->current_limit / magnitude;
        reference.q *= config->current_limit / magnitude;
    }
    ed = reference.d - current.d;
    eq = reference.q - current.q;
    if (use_map) {
        namc_dq_t flux;
        namc_flux_result_t status = namc_flux_map_lookup(map, current, &flux, NULL);
        if (status != NAMC_FLUX_OK) {
            *model_failure = status;
            /* No state update before the explicit failure policy is applied.
             * Output was disabled at entry; all measurement guards ran first. */
            return NAMC_MODEL_REJECTED;
        }
        raw.d = config->kp_d * ed + state->integral_d - electrical_speed * flux.q;
        raw.q = config->kp_q * eq + state->integral_q + electrical_speed * flux.d;
    } else {
        /* Preserve baseline arithmetic and independent priors exactly. */
        raw.d = config->kp_d * ed + state->integral_d -
                electrical_speed * config->lq * current.q;
        raw.q = config->kp_q * eq + state->integral_q +
                electrical_speed * (config->ld * current.d + config->pm_flux);
    }
    magnitude = hypot(raw.d, raw.q);
    voltage_limit = vdc / namc_sqrt3;
    if (!namc_finite_dq(raw) || !isfinite(magnitude) || voltage_limit <= 0.0) {
        goto fault;
    }
    limited = raw;
    if (magnitude > voltage_limit) {
        limited.d *= voltage_limit / magnitude;
        limited.q *= voltage_limit / magnitude;
    }
    next_d = state->integral_d + config->sample_time *
        (config->ki_d * ed + config->antiwindup_gain * (limited.d - raw.d));
    next_q = state->integral_q + config->sample_time *
        (config->ki_q * eq + config->antiwindup_gain * (limited.q - raw.q));
    if (!isfinite(next_d) || !isfinite(next_q) ||
        namc_inverse_park(limited, electrical_angle, &command) != NAMC_OK) {
        goto fault;
    }
    if (namc_modulate(command, vdc, out) != NAMC_OK) {
        goto fault;
    }
    state->integral_d = namc_clip(next_d, -voltage_limit, voltage_limit);
    state->integral_q = namc_clip(next_q, -voltage_limit, voltage_limit);
    return NAMC_OK;

fault:
    state->faulted = 1;
    namc_disable(out);
    return result;
}

namc_result_t namc_current_step(
    const namc_current_config_t *config, namc_current_state_t *state,
    namc_dq_t reference, namc_abc_t measured_current, double electrical_angle,
    double electrical_speed, double vdc, namc_duty_t *out)
{
    return namc_current_step_internal(config, state, reference, measured_current,
        electrical_angle, electrical_speed, vdc, out, 0, NULL, NULL);
}

void namc_model_current_reset(namc_model_current_state_t *state)
{
    if (state != NULL) {
        namc_current_reset(&state->pi);
        state->fallback_latched = 0;
        state->model_failure = NAMC_FLUX_OK;
    }
}

namc_result_t namc_model_current_step(
    const namc_model_current_config_t *config, namc_model_current_state_t *state,
    namc_dq_t reference, namc_abc_t measured_current, double electrical_angle,
    double electrical_speed, double vdc, namc_duty_t *out)
{
    namc_result_t result;
    namc_disable(out);
    if (state == NULL) { return NAMC_INVALID_INPUT; }
    if (state->pi.faulted != 0) { return NAMC_FAULT_LATCHED; }
    if (config == NULL || config->version != NAMC_MODEL_CURRENT_VERSION ||
        (config->failure_policy != NAMC_MODEL_DISABLE &&
         config->failure_policy != NAMC_MODEL_NOMINAL_FALLBACK) ||
        (state->fallback_latched != 0 && state->fallback_latched != 1)) {
        state->pi.faulted = 1;
        return NAMC_INVALID_INPUT;
    }
    /* A policy change cannot re-enable a map or keep fallback running when
     * the caller now requires disablement. No automatic retry on a new map. */
    if (state->fallback_latched && config->failure_policy == NAMC_MODEL_DISABLE) {
        state->pi.faulted = 1;
        return NAMC_MODEL_REJECTED;
    }
    result = namc_current_step_internal(&config->nominal, &state->pi, reference,
        measured_current, electrical_angle, electrical_speed, vdc, out,
        !state->fallback_latched, config->map, &state->model_failure);
    if (result != NAMC_MODEL_REJECTED) { return result; }
    if (config->failure_policy == NAMC_MODEL_DISABLE) {
        state->pi.faulted = 1;
        return result;
    }
    state->fallback_latched = 1;
    namc_current_reset(&state->pi);
    /* The nominal path runs the same hard guards again. No fallback is
     * permitted for invalid measurements, invalid PI state or hard limits. */
    return namc_current_step_internal(&config->nominal, &state->pi, reference,
        measured_current, electrical_angle, electrical_speed, vdc, out, 0, NULL, NULL);
}
