#include "namc/flux_id.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static int namc_fi_positive(double x) { return isfinite(x) && x > 0.0; }
static int namc_fi_nonnegative(double x) { return isfinite(x) && x >= 0.0; }

static int namc_fi_valid(const namc_fi_config_t *c, const namc_fi_limits_t *l)
{
    unsigned int k;
    if (c == NULL || l == NULL || c->version != NAMC_FLUX_ID_VERSION ||
        !namc_fi_positive(c->sample_time) || c->sample_time > 1.0 ||
        c->settle_samples == 0U || c->settle_samples > NAMC_RESISTANCE_MAX_SAMPLES ||
        c->measure_samples < 2U || c->measure_samples > NAMC_RESISTANCE_MAX_SAMPLES ||
        c->max_samples == 0U || c->max_samples > NAMC_RESISTANCE_MAX_SAMPLES ||
        !namc_fi_positive(c->resistance) || !namc_fi_nonnegative(c->resistance_uncertainty) ||
        c->resistance_uncertainty >= c->resistance ||
        !namc_fi_nonnegative(c->current_uncertainty) || !namc_fi_nonnegative(c->voltage_uncertainty) ||
        !namc_fi_nonnegative(c->speed_uncertainty) || !namc_fi_nonnegative(c->model_voltage_error) ||
        !namc_fi_positive(c->gradient_bound) ||
        !namc_fi_positive(c->max_current_span) || !namc_fi_positive(c->max_voltage_span) ||
        !namc_fi_positive(c->max_speed_span) || !namc_fi_positive(c->max_tracking_error) ||
        !namc_fi_positive(c->max_speed_error) || !namc_fi_positive(c->min_abs_speed) ||
        !namc_fi_positive(c->min_speed_separation) || !namc_fi_positive(c->max_flux) ||
        !namc_fi_positive(c->max_flux_uncertainty) || !namc_fi_nonnegative(c->max_offset_voltage) ||
        !namc_fi_positive(c->validation_tolerance) || !namc_fi_positive(c->max_origin_q_flux) ||
        !namc_fi_positive(l->max_current) || !namc_fi_positive(l->max_voltage) ||
        !namc_fi_positive(l->max_electrical_speed) || !namc_fi_positive(l->max_apparent_energy) ||
        !isfinite(c->target_current.d) || !isfinite(c->target_current.q) ||
        hypot(c->target_current.d, c->target_current.q) > l->max_current) { return 0; }
    for (k = 0U; k < 3U; ++k) {
        if (!isfinite(c->speeds[k]) || fabs(c->speeds[k]) > l->max_electrical_speed ||
            fabs(c->speeds[k])-c->speed_uncertainty < c->min_abs_speed) { return 0; }
    }
    return c->speeds[0] > 0.0 && c->speeds[1] < 0.0 && c->speeds[2] > 0.0 &&
        c->speeds[0]-c->speeds[1]-2.0*c->speed_uncertainty > c->min_speed_separation &&
        fabs(c->speeds[2]-c->speeds[0])-2.0*c->speed_uncertainty > c->min_speed_separation;
}

static namc_fi_phase_t namc_fi_stop(namc_fi_state_t *s, namc_fi_phase_t p, namc_fi_reason_t r)
{
    s->phase = p; s->reason = r;
    memset(&s->result, 0, sizeof(s->result));
    return p;
}

namc_result_t namc_fi_start(const namc_fi_config_t *c, const namc_fi_limits_t *l, namc_fi_state_t *s)
{
    if (s == NULL) { return NAMC_INVALID_INPUT; }
    if (!namc_fi_valid(c, l)) {
        memset(s, 0, sizeof(*s)); s->version = NAMC_FLUX_ID_VERSION;
        (void)namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_CONFIG);
        return NAMC_INVALID_INPUT;
    }
    {
        namc_fi_config_t config = *c;
        namc_fi_limits_t limits = *l;
        memset(s, 0, sizeof(*s)); s->config = config; s->limits = limits;
    }
    s->version = NAMC_FLUX_ID_VERSION; s->phase = NAMC_FI_SETTLING;
    return NAMC_OK;
}

int namc_fi_request(const namc_fi_state_t *s, namc_dq_t *i, double *speed)
{
    if (i != NULL) { *i = (namc_dq_t){0.0, 0.0}; }
    if (speed != NULL) { *speed = 0.0; }
    if (i == NULL || speed == NULL || s == NULL || s->version != NAMC_FLUX_ID_VERSION ||
        s->stage >= 3U || (s->phase != NAMC_FI_SETTLING && s->phase != NAMC_FI_SAMPLING) ||
        !namc_fi_valid(&s->config, &s->limits) || s->elapsed_samples >= s->config.max_samples ||
        !isfinite(s->apparent_energy) || s->apparent_energy < 0.0 ||
        s->apparent_energy >= s->limits.max_apparent_energy) { return 0; }
    *i = s->config.target_current; *speed = s->config.speeds[s->stage];
    return 1;
}

/* Bound the axis residual of v-R*i at the requested point, including current
 * coverage error. Does not assert that caller-declared calibration, gradient
 * or unmodeled-dynamics bounds are physically true. */
static double namc_fi_error(const namc_fi_config_t *c, const namc_fi_window_t *w, unsigned int axis)
{
    double distance = hypot(
        fmax(fabs(w->minimum[0]-c->target_current.d), fabs(w->maximum[0]-c->target_current.d)),
        fmax(fabs(w->minimum[1]-c->target_current.q), fabs(w->maximum[1]-c->target_current.q))) +
        1.4142135623730950488*c->current_uncertainty;
    return c->voltage_uncertainty + c->resistance*c->current_uncertainty +
        c->resistance_uncertainty*(fabs(w->mean[axis])+c->current_uncertainty) +
        c->model_voltage_error + (fmax(fabs(w->minimum[4]), fabs(w->maximum[4]))+
        c->speed_uncertainty)*c->gradient_bound*distance;
}

static namc_fi_phase_t namc_fi_finish(namc_fi_state_t *s)
{
    const namc_fi_config_t *c = &s->config;
    const namc_fi_window_t *w = s->windows;
    namc_fi_result_t result = {0};
    double delta = w[0].mean[4]-w[1].mean[4], udelta = 2.0*c->speed_uncertainty;
    unsigned int axis;
    if (!isfinite(delta) || delta-udelta <= c->min_speed_separation ||
        fabs(w[2].mean[4]-w[0].mean[4])-udelta <= c->min_speed_separation) {
        return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_UNOBSERVABLE);
    }
    for (axis = 0U; axis < 2U; ++axis) {
        double y[3], f, uncertainty, offset, residual;
        unsigned int k;
        for (k = 0U; k < 3U; ++k) { y[k] = w[k].mean[axis+2U]-c->resistance*w[k].mean[axis]; }
        f = (y[0]-y[1])/delta; /* axis 0 = -psi_q; axis 1 = psi_d */
        uncertainty = (namc_fi_error(c, &w[0], axis)+namc_fi_error(c, &w[1], axis)+
            fabs(f)*udelta)/(delta-udelta);
        offset = y[0]-w[0].mean[4]*f;
        residual = y[2]-(w[2].mean[4]*f+offset);
        if (!isfinite(f) || !isfinite(uncertainty) || !isfinite(offset) || !isfinite(residual)) {
            return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_OBSERVATION);
        }
        if (fabs(f)+uncertainty > c->max_flux) { return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_RANGE); }
        if (fabs(offset) > c->max_offset_voltage) { return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_OFFSET); }
        if (uncertainty > c->max_flux_uncertainty) { return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_UNCERTAINTY); }
        if (fabs(residual) > c->validation_tolerance) { return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_VALIDATION); }
        if (axis == 0U) {
            result.flux.q = -f; result.uncertainty.q = uncertainty;
            result.voltage_offset.d = offset; result.holdout_residual.d = residual;
        } else {
            result.flux.d = f; result.uncertainty.d = uncertainty;
            result.voltage_offset.q = offset; result.holdout_residual.q = residual;
        }
    }
    if (c->target_current.d == 0.0 && c->target_current.q == 0.0) {
        if (result.flux.d-result.uncertainty.d <= 0.0 ||
            fabs(result.flux.q)+result.uncertainty.q > c->max_origin_q_flux) {
            return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_REFERENCE);
        }
        result.pm_reference = 1;
    }
    s->result = result; s->phase = NAMC_FI_COMPLETE;
    return s->phase;
}

namc_fi_phase_t namc_fi_observe(namc_fi_state_t *s, const namc_rs_observation_t *o)
{
    double values[5], current, voltage, energy;
    namc_fi_window_t *w;
    unsigned int k;
    if (s == NULL) { return NAMC_FI_FAULT; }
    if (s->version != NAMC_FLUX_ID_VERSION) { return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_CONFIG); }
    if (s->phase == NAMC_FI_COMPLETE || s->phase == NAMC_FI_DEGRADED || s->phase == NAMC_FI_FAULT) { return s->phase; }
    if (!namc_fi_valid(&s->config, &s->limits) || s->stage >= 3U ||
        (s->phase != NAMC_FI_SETTLING && s->phase != NAMC_FI_SAMPLING)) {
        return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_CONFIG);
    }
    if (s->elapsed_samples >= s->config.max_samples) { return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_TIMEOUT); }
    if (o == NULL || o->sequence != s->elapsed_samples+1U ||
        !isfinite(s->apparent_energy) || s->apparent_energy < 0.0) {
        return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_OBSERVATION);
    }
    values[0] = o->current.d; values[1] = o->current.q;
    values[2] = o->terminal_voltage.d; values[3] = o->terminal_voltage.q; values[4] = o->electrical_speed;
    for (k = 0U; k < 5U; ++k) {
        if (!isfinite(values[k])) { return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_OBSERVATION); }
    }
    if ((o->voltage_source != NAMC_RS_IDEAL_TERMINAL && o->voltage_source != NAMC_RS_MEASURED_TERMINAL &&
         o->voltage_source != NAMC_RS_CALIBRATED_RECONSTRUCTION) ||
        (s->elapsed_samples != 0U && o->voltage_source != s->voltage_source)) {
        return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_SOURCE);
    }
    current = hypot(values[0], values[1]); voltage = hypot(values[2], values[3]);
    energy = s->apparent_energy+1.5*current*voltage*s->config.sample_time;
    if (!isfinite(current) || !isfinite(voltage) || !isfinite(energy)) {
        return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_LIMIT);
    }
    s->apparent_energy = energy; s->voltage_source = o->voltage_source; ++s->elapsed_samples;
    if (current > s->limits.max_current || voltage > s->limits.max_voltage ||
        fabs(values[4]) > s->limits.max_electrical_speed || energy >= s->limits.max_apparent_energy) {
        return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_LIMIT);
    }
    ++s->stage_samples;
    if (s->phase == NAMC_FI_SETTLING) {
        if (s->stage_samples >= s->config.settle_samples) { s->phase = NAMC_FI_SAMPLING; s->stage_samples = 0U; }
    } else {
        const namc_fi_config_t *c = &s->config;
        int unsettled = 0;
        if (fabs(values[4])-c->speed_uncertainty < c->min_abs_speed) {
            return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_UNOBSERVABLE);
        }
        if (hypot(values[0]-c->target_current.d, values[1]-c->target_current.q) > c->max_tracking_error ||
            fabs(values[4]-c->speeds[s->stage]) > c->max_speed_error) {
            return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_TRACKING);
        }
        w = &s->windows[s->stage];
        if (w->count >= c->measure_samples || w->count+1U != s->stage_samples) {
            return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_OBSERVATION);
        }
        for (k = 0U; k < 5U; ++k) {
            double span_limit = k < 2U ? c->max_current_span : (k < 4U ? c->max_voltage_span : c->max_speed_span);
            if (!isfinite(w->mean[k]) || !isfinite(w->minimum[k]) || !isfinite(w->maximum[k])) {
                return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_OBSERVATION);
            }
            if (w->count == 0U) { w->minimum[k] = w->maximum[k] = values[k]; }
            w->mean[k] += (values[k]-w->mean[k])/(double)(w->count+1U);
            w->minimum[k] = fmin(w->minimum[k], values[k]); w->maximum[k] = fmax(w->maximum[k], values[k]);
            if (!isfinite(w->mean[k])) { return namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_OBSERVATION); }
            if (w->maximum[k]-w->minimum[k] > span_limit) {
                unsettled = 1;
            }
        }
        ++w->count;
        if (unsettled) { return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_UNSETTLED); }
        if (w->count == c->measure_samples) {
            if (s->stage == 2U) { return namc_fi_finish(s); }
            ++s->stage; s->stage_samples = 0U; s->phase = NAMC_FI_SETTLING;
        }
    }
    if (s->elapsed_samples >= s->config.max_samples) { return namc_fi_stop(s, NAMC_FI_DEGRADED, NAMC_FI_TIMEOUT); }
    return s->phase;
}

void namc_fi_abort(namc_fi_state_t *s)
{
    if (s != NULL) { (void)namc_fi_stop(s, NAMC_FI_FAULT, NAMC_FI_ABORTED); }
}
