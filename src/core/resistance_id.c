#include "namc/resistance_id.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static int namc_rs_positive(double x) { return isfinite(x) && x > 0.0; }
static int namc_rs_nonnegative(double x) { return isfinite(x) && x >= 0.0; }

static int namc_rs_config_valid(const namc_rs_config_t *c, const namc_rs_limits_t *l)
{
    unsigned int k;
    if (c == NULL || l == NULL || c->version != NAMC_RESISTANCE_ID_VERSION ||
        !namc_rs_positive(c->sample_time) || c->sample_time > 1.0 ||
        c->settle_samples == 0U || c->settle_samples > NAMC_RESISTANCE_MAX_SAMPLES ||
        c->measure_samples < 2U || c->measure_samples > NAMC_RESISTANCE_MAX_SAMPLES ||
        c->max_samples == 0U || c->max_samples > NAMC_RESISTANCE_MAX_SAMPLES ||
        !namc_rs_positive(c->min_current_separation) ||
        !namc_rs_positive(c->max_current_span) || !namc_rs_positive(c->max_voltage_span) ||
        !namc_rs_positive(c->max_tracking_error) || !namc_rs_positive(c->max_quadrature_current) ||
        !namc_rs_positive(c->max_quadrature_voltage) || !namc_rs_positive(c->min_resistance) ||
        !namc_rs_positive(c->max_resistance) || c->min_resistance >= c->max_resistance ||
        !namc_rs_nonnegative(c->max_offset_voltage) ||
        !namc_rs_positive(c->max_resistance_uncertainty) ||
        !namc_rs_positive(c->validation_voltage_tolerance) ||
        !namc_rs_nonnegative(c->current_uncertainty) || !namc_rs_nonnegative(c->voltage_uncertainty) ||
        !namc_rs_positive(l->max_current) || !namc_rs_positive(l->max_voltage) ||
        !namc_rs_nonnegative(l->max_electrical_speed) || !namc_rs_positive(l->max_apparent_energy)) {
        return 0;
    }
    for (k = 0U; k < 3U; ++k) {
        if (!isfinite(c->levels[k]) || fabs(c->levels[k]) > l->max_current) { return 0; }
    }
    return c->levels[0] > 0.0 && c->levels[1] > c->levels[0] && c->levels[2] < 0.0 &&
        c->levels[1]-c->levels[0] > c->min_current_separation + 2.0*c->current_uncertainty;
}

static namc_rs_phase_t namc_rs_stop(namc_rs_state_t *s, namc_rs_phase_t phase, namc_rs_reason_t reason)
{
    s->phase = phase;
    s->reason = reason;
    memset(&s->result, 0, sizeof(s->result));
    return phase;
}

namc_result_t namc_rs_start(const namc_rs_config_t *c, const namc_rs_limits_t *l, namc_rs_state_t *s)
{
    if (s == NULL) { return NAMC_INVALID_INPUT; }
    /* c/l must not alias state, which is initialized by this operation. */
    if (!namc_rs_config_valid(c, l)) {
        memset(s, 0, sizeof(*s));
        s->version = NAMC_RESISTANCE_ID_VERSION;
        (void)namc_rs_stop(s, NAMC_RS_FAULT, NAMC_RS_BAD_CONFIG);
        return NAMC_INVALID_INPUT;
    }
    {
        namc_rs_config_t config = *c;
        namc_rs_limits_t limits = *l;
        memset(s, 0, sizeof(*s));
        s->config = config;
        s->limits = limits;
    }
    s->version = NAMC_RESISTANCE_ID_VERSION;
    s->phase = NAMC_RS_SETTLING;
    return NAMC_OK;
}

int namc_rs_request(const namc_rs_state_t *s, namc_dq_t *out)
{
    if (out == NULL) { return 0; }
    out->d = 0.0; out->q = 0.0;
    if (s == NULL || s->version != NAMC_RESISTANCE_ID_VERSION || s->stage >= 3U ||
        (s->phase != NAMC_RS_SETTLING && s->phase != NAMC_RS_SAMPLING) ||
        !namc_rs_config_valid(&s->config, &s->limits) ||
        s->elapsed_samples >= s->config.max_samples || !isfinite(s->apparent_energy) ||
        s->apparent_energy < 0.0 || s->apparent_energy >= s->limits.max_apparent_energy) { return 0; }
    out->d = s->config.levels[s->stage];
    return 1;
}

static namc_rs_phase_t namc_rs_finish(namc_rs_state_t *s)
{
    const namc_rs_config_t *c = &s->config;
    const namc_rs_window_t *w = s->windows;
    double di = w[1].mean_current-w[0].mean_current;
    double dv = w[1].mean_voltage-w[0].mean_voltage;
    double ui = 2.0*c->current_uncertainty, uv = 2.0*c->voltage_uncertainty;
    namc_rs_result_t r;
    if (!isfinite(di) || !isfinite(dv) || di-ui <= c->min_current_separation) {
        return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_NO_EXCITATION);
    }
    r.resistance = dv/di;
    r.lower = (dv-uv)/(di+ui);
    r.upper = (dv+uv)/(di-ui);
    r.offset_voltage = w[0].mean_voltage-r.resistance*w[0].mean_current;
    r.validation_residual = w[2].mean_voltage-(r.resistance*w[2].mean_current+r.offset_voltage);
    if (!isfinite(r.resistance) || !isfinite(r.lower) || !isfinite(r.upper) ||
        !isfinite(r.offset_voltage) || !isfinite(r.validation_residual)) {
        return namc_rs_stop(s, NAMC_RS_FAULT, NAMC_RS_BAD_OBSERVATION);
    }
    if (r.lower < c->min_resistance || r.upper > c->max_resistance) {
        return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_RANGE);
    }
    if (fabs(r.offset_voltage) > c->max_offset_voltage) {
        return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_OFFSET);
    }
    if (fmax(r.resistance-r.lower, r.upper-r.resistance) > c->max_resistance_uncertainty) {
        return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_UNCERTAINTY);
    }
    if (fabs(r.validation_residual) > c->validation_voltage_tolerance) {
        return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_VALIDATION);
    }
    s->result = r;
    s->phase = NAMC_RS_COMPLETE;
    return s->phase;
}

namc_rs_phase_t namc_rs_observe(namc_rs_state_t *s, const namc_rs_observation_t *o)
{
    namc_rs_window_t *w;
    double current, voltage, energy;
    if (s == NULL) { return NAMC_RS_FAULT; }
    if (s->version != NAMC_RESISTANCE_ID_VERSION) {
        return namc_rs_stop(s, NAMC_RS_FAULT, NAMC_RS_BAD_CONFIG);
    }
    if (s->phase == NAMC_RS_COMPLETE || s->phase == NAMC_RS_DEGRADED || s->phase == NAMC_RS_FAULT) {
        return s->phase; /* Terminal until explicit restart. */
    }
    if (!namc_rs_config_valid(&s->config, &s->limits) || s->stage >= 3U ||
        (s->phase != NAMC_RS_SETTLING && s->phase != NAMC_RS_SAMPLING)) {
        return namc_rs_stop(s, NAMC_RS_FAULT, NAMC_RS_BAD_CONFIG);
    }
    if (s->elapsed_samples >= s->config.max_samples) {
        return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_TIMEOUT);
    }
    if (o == NULL || o->sequence != s->elapsed_samples+1U ||
        !isfinite(o->current.d) || !isfinite(o->current.q) ||
        !isfinite(o->terminal_voltage.d) || !isfinite(o->terminal_voltage.q) ||
        !isfinite(o->electrical_speed) || !isfinite(s->apparent_energy) || s->apparent_energy < 0.0) {
        return namc_rs_stop(s, NAMC_RS_FAULT, NAMC_RS_BAD_OBSERVATION);
    }
    if ((o->voltage_source != NAMC_RS_IDEAL_TERMINAL && o->voltage_source != NAMC_RS_MEASURED_TERMINAL &&
         o->voltage_source != NAMC_RS_CALIBRATED_RECONSTRUCTION) ||
        (s->elapsed_samples != 0U && o->voltage_source != s->voltage_source)) {
        return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_UNTRUSTED_VOLTAGE);
    }
    current = hypot(o->current.d, o->current.q);
    voltage = hypot(o->terminal_voltage.d, o->terminal_voltage.q);
    energy = s->apparent_energy + 1.5*current*voltage*s->config.sample_time;
    if (!isfinite(current) || !isfinite(voltage) || !isfinite(energy)) {
        return namc_rs_stop(s, NAMC_RS_FAULT, NAMC_RS_LIMIT);
    }
    s->voltage_source = o->voltage_source;
    s->apparent_energy = energy;
    ++s->elapsed_samples;
    if (current > s->limits.max_current || voltage > s->limits.max_voltage ||
        fabs(o->electrical_speed) > s->limits.max_electrical_speed || energy >= s->limits.max_apparent_energy) {
        return namc_rs_stop(s, NAMC_RS_FAULT, NAMC_RS_LIMIT);
    }
    ++s->stage_samples;
    if (s->phase == NAMC_RS_SETTLING) {
        if (s->stage_samples >= s->config.settle_samples) {
            s->phase = NAMC_RS_SAMPLING;
            s->stage_samples = 0U;
        }
    } else {
        if (fabs(o->current.d-s->config.levels[s->stage]) > s->config.max_tracking_error ||
            fabs(o->current.q) > s->config.max_quadrature_current ||
            fabs(o->terminal_voltage.q) > s->config.max_quadrature_voltage) {
            return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_TRACKING);
        }
        w = &s->windows[s->stage];
        if (w->count >= s->config.measure_samples || w->count+1U != s->stage_samples ||
            !isfinite(w->mean_current) || !isfinite(w->mean_voltage)) {
            return namc_rs_stop(s, NAMC_RS_FAULT, NAMC_RS_BAD_OBSERVATION);
        }
        if (w->count == 0U) {
            w->min_current = w->max_current = o->current.d;
            w->min_voltage = w->max_voltage = o->terminal_voltage.d;
        }
        ++w->count;
        w->mean_current += (o->current.d-w->mean_current)/(double)w->count;
        w->mean_voltage += (o->terminal_voltage.d-w->mean_voltage)/(double)w->count;
        w->min_current = fmin(w->min_current, o->current.d);
        w->max_current = fmax(w->max_current, o->current.d);
        w->min_voltage = fmin(w->min_voltage, o->terminal_voltage.d);
        w->max_voltage = fmax(w->max_voltage, o->terminal_voltage.d);
        if (w->max_current-w->min_current > s->config.max_current_span ||
            w->max_voltage-w->min_voltage > s->config.max_voltage_span) {
            return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_UNSETTLED);
        }
        if (w->count == s->config.measure_samples) {
            if (s->stage == 2U) { return namc_rs_finish(s); }
            ++s->stage;
            s->stage_samples = 0U;
            s->phase = NAMC_RS_SETTLING;
        }
    }
    if (s->elapsed_samples >= s->config.max_samples) {
        return namc_rs_stop(s, NAMC_RS_DEGRADED, NAMC_RS_TIMEOUT);
    }
    return s->phase;
}

void namc_rs_abort(namc_rs_state_t *s)
{
    if (s != NULL) { (void)namc_rs_stop(s, NAMC_RS_FAULT, NAMC_RS_ABORTED); }
}
