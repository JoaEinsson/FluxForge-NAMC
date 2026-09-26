#include "namc/current_tuning.h"

#include <math.h>
#include <stddef.h>

static int namc_tuning_positive(double value)
{
    return isfinite(value) && value > 0.0;
}

static int namc_tuning_valid(const namc_current_config_t *b, const namc_tuning_request_t *r)
{
    /* Gains/nominal magnetic priors are deliberately not inputs to this design. */
    return b != NULL && r != NULL && b->model_version == NAMC_REFERENCE_MODEL_VERSION &&
        r->version == NAMC_CURRENT_TUNING_VERSION &&
        namc_tuning_positive(b->sample_time) && namc_tuning_positive(b->current_limit) &&
        namc_tuning_positive(b->min_bus_voltage) && isfinite(b->max_bus_voltage) &&
        b->max_bus_voltage >= b->min_bus_voltage &&
        namc_tuning_positive(b->electrical_speed_limit) &&
        isfinite(r->operating_current.d) && isfinite(r->operating_current.q) &&
        isfinite(r->electrical_speed) && isfinite(r->bus_voltage) &&
        namc_tuning_positive(r->resistance) && namc_tuning_positive(r->requested_bandwidth) &&
        namc_tuning_positive(r->min_bandwidth) && namc_tuning_positive(r->max_bandwidth) &&
        r->min_bandwidth <= r->max_bandwidth &&
        namc_tuning_positive(r->max_rate_sample) && r->max_rate_sample <= 0.25 &&
        namc_tuning_positive(r->max_kp) && namc_tuning_positive(r->max_ki) &&
        namc_tuning_positive(r->design_error) &&
        namc_tuning_positive(r->voltage_fraction) && r->voltage_fraction <= 1.0;
}

static int namc_tuning_cap(double cap, unsigned int flag, double requested,
    namc_tuning_candidate_t *candidate)
{
    if (!namc_tuning_positive(cap)) { return 0; }
    if (cap < requested) { candidate->caps |= flag; }
    candidate->bandwidth = fmin(candidate->bandwidth, cap);
    return 1;
}

namc_tuning_result_t namc_current_tune(const namc_flux_map_t *map,
    const namc_current_config_t *base, const namc_tuning_request_t *r,
    namc_tuning_candidate_t *out)
{
    namc_tuning_candidate_t c = {0};
    namc_dq_t flux, col_d, col_q, unit_d = {1.0, 0.0}, unit_q = {0.0, 1.0};
    double peak_l, resistive_rate, sample_cap, voltage_cap, vd, vq, radius;
    if (out == NULL || !namc_tuning_valid(base, r)) { return NAMC_TUNING_INVALID_INPUT; }
    radius = hypot(r->operating_current.d, r->operating_current.q) + r->design_error;
    if (!isfinite(radius) || radius > base->current_limit ||
        r->bus_voltage < base->min_bus_voltage || r->bus_voltage > base->max_bus_voltage ||
        fabs(r->electrical_speed) > base->electrical_speed_limit) {
        return NAMC_TUNING_INFEASIBLE;
    }
    if (namc_flux_map_lookup(map, r->operating_current, &flux, &c.jacobian) != NAMC_FLUX_OK ||
        namc_flux_solve(c.jacobian, map->limits.min_rcond, unit_d, &col_d) != NAMC_FLUX_OK ||
        namc_flux_solve(c.jacobian, map->limits.min_rcond, unit_q, &col_q) != NAMC_FLUX_OK) {
        return NAMC_TUNING_MODEL_REJECTED;
    }
    /* Both cross terms participate in these infinity-norm bounds. J is never
     * replaced by diag(J) in the plant or lookup; diagonal PI is a controller
     * architecture, not a decoupled magnetic model or stability proof. */
    c.inverse_norm = fmax(fabs(col_d.d) + fabs(col_q.d), fabs(col_d.q) + fabs(col_q.q));
    c.coupling_factor = fmax(fabs(col_d.d * c.jacobian.dd) + fabs(col_q.d * c.jacobian.qq),
                            fabs(col_d.q * c.jacobian.dd) + fabs(col_q.q * c.jacobian.qq));
    peak_l = fmax(c.jacobian.dd, c.jacobian.qq);
    resistive_rate = r->resistance * c.inverse_norm;
    vd = r->resistance * r->operating_current.d - r->electrical_speed * flux.q;
    vq = r->resistance * r->operating_current.q + r->electrical_speed * flux.d;
    c.voltage_headroom = r->bus_voltage / sqrt(3.0) - hypot(vd, vq);
    if (!namc_tuning_positive(c.inverse_norm) || !namc_tuning_positive(c.coupling_factor) ||
        !namc_tuning_positive(peak_l) || !namc_tuning_positive(resistive_rate) ||
        !namc_tuning_positive(c.voltage_headroom)) { return NAMC_TUNING_INFEASIBLE; }

    sample_cap = (r->max_rate_sample / base->sample_time - resistive_rate) / c.coupling_factor;
    /* Reserve headroom for the specified error's proportional action plus
     * one integration increment; not a bound on later integral accumulation. */
    voltage_cap = (r->voltage_fraction * c.voltage_headroom / r->design_error) /
                  (peak_l + r->resistance * base->sample_time);
    c.bandwidth = r->requested_bandwidth;
    if (!namc_tuning_cap(r->max_bandwidth, NAMC_TUNING_BANDWIDTH_CAP, r->requested_bandwidth, &c) ||
        !namc_tuning_cap(sample_cap, NAMC_TUNING_SAMPLE_CAP, r->requested_bandwidth, &c) ||
        !namc_tuning_cap(r->max_kp / peak_l, NAMC_TUNING_KP_CAP, r->requested_bandwidth, &c) ||
        !namc_tuning_cap(r->max_ki / r->resistance, NAMC_TUNING_KI_CAP, r->requested_bandwidth, &c) ||
        !namc_tuning_cap(voltage_cap, NAMC_TUNING_VOLTAGE_CAP, r->requested_bandwidth, &c) ||
        c.bandwidth < r->min_bandwidth) { return NAMC_TUNING_INFEASIBLE; }
    c.kp_d = c.bandwidth * c.jacobian.dd;
    c.kp_q = c.bandwidth * c.jacobian.qq;
    c.ki_d = c.bandwidth * r->resistance;
    c.ki_q = c.ki_d;
    c.rate_sample = base->sample_time * (resistive_rate + c.bandwidth * c.coupling_factor);
    if (!namc_tuning_positive(c.kp_d) || !namc_tuning_positive(c.kp_q) ||
        !namc_tuning_positive(c.ki_d) || !namc_tuning_positive(c.rate_sample)) {
        return NAMC_TUNING_INFEASIBLE;
    }
    *out = c;
    return NAMC_TUNING_OK;
}
