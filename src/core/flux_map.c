#include "namc/flux_map.h"

#include <math.h>
#include <stdint.h>

static int namc_flux_shape_valid(const namc_flux_data_t *d)
{
    return d != NULL && d->version == NAMC_FLUX_MAP_VERSION &&
        d->units == NAMC_FLUX_SI_UNITS && d->convention == NAMC_FLUX_AMPLITUDE_INVARIANT_DQ &&
        d->nd >= 2U && d->nd <= NAMC_FLUX_MAX_AXIS &&
        d->nq >= 2U && d->nq <= NAMC_FLUX_MAX_AXIS &&
        d->nd <= SIZE_MAX / d->nq &&
        d->count == d->nd * d->nq && d->id_axis != NULL && d->iq_axis != NULL &&
        d->psi_d != NULL && d->psi_q != NULL;
}

static int namc_flux_limits_valid(const namc_flux_limits_t *l)
{
    return l != NULL && isfinite(l->min_incremental_h) && l->min_incremental_h > 0.0 &&
        isfinite(l->max_reciprocity_error_h) && l->max_reciprocity_error_h >= 0.0 &&
        isfinite(l->min_rcond) && l->min_rcond > 0.0 && l->min_rcond <= 1.0;
}

static int namc_flux_axis_valid(const double *axis, size_t count)
{
    size_t i;
    for (i = 0U; i < count; ++i) {
        if (!isfinite(axis[i]) || (i > 0U &&
            (!isfinite(axis[i] - axis[i - 1U]) || axis[i] <= axis[i - 1U]))) {
            return 0;
        }
    }
    return 1;
}

static int namc_flux_source_valid(const char *source)
{
    size_t i;
    if (source == NULL || source[0] == '\0') {
        return 0;
    }
    for (i = 1U; i < 128U; ++i) {
        if (source[i] == '\0') {
            return 1;
        }
    }
    return 0;
}

/* Matrix scaled by its largest entry. Norms cannot overflow after scaling. */
static namc_flux_result_t namc_flux_matrix(namc_flux_jacobian_t j, double min_rcond,
    namc_flux_jacobian_t *normalized, double *scale, double *determinant)
{
    double norm, inverse_numerator, rcond;
    if (!isfinite(j.dd) || !isfinite(j.dq) || !isfinite(j.qd) || !isfinite(j.qq) ||
        !isfinite(min_rcond) || min_rcond <= 0.0 || min_rcond > 1.0) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    *scale = fmax(fmax(fabs(j.dd), fabs(j.dq)), fmax(fabs(j.qd), fabs(j.qq)));
    if (*scale == 0.0) {
        return NAMC_FLUX_ILL_CONDITIONED;
    }
    normalized->dd = j.dd / *scale;
    normalized->dq = j.dq / *scale;
    normalized->qd = j.qd / *scale;
    normalized->qq = j.qq / *scale;
    *determinant = normalized->dd * normalized->qq - normalized->dq * normalized->qd;
    norm = fmax(fabs(normalized->dd) + fabs(normalized->dq),
                fabs(normalized->qd) + fabs(normalized->qq));
    inverse_numerator = fmax(fabs(normalized->qq) + fabs(normalized->dq),
                             fabs(normalized->qd) + fabs(normalized->dd));
    rcond = fabs(*determinant) / (norm * inverse_numerator);
    if (!isfinite(rcond) || rcond < min_rcond || *determinant == 0.0) {
        return NAMC_FLUX_ILL_CONDITIONED;
    }
    return NAMC_FLUX_OK;
}

static namc_flux_result_t namc_flux_physics(namc_flux_jacobian_t j,
    const namc_flux_limits_t *limits)
{
    namc_flux_jacobian_t n;
    double scale, det, floor_value, a, b, c;
    namc_flux_result_t result = namc_flux_matrix(j, limits->min_rcond, &n, &scale, &det);
    if (result != NAMC_FLUX_OK) {
        return result;
    }
    if (fabs(j.dq - j.qd) > limits->max_reciprocity_error_h) {
        return NAMC_FLUX_NON_RECIPROCAL;
    }
    floor_value = limits->min_incremental_h / scale;
    a = n.dd - floor_value;
    b = 0.5 * n.dq + 0.5 * n.qd;
    c = n.qq - floor_value;
    if (!isfinite(floor_value) || a <= 0.0 || c <= 0.0 || a * c - b * b <= 0.0) {
        return NAMC_FLUX_NON_PASSIVE;
    }
    return NAMC_FLUX_OK;
}

static int namc_flux_interpolate(const double *values, size_t offset, size_t stride,
    double hd, double hq, double u, double v, double *flux, double *dd, double *dq)
{
    double f00 = values[offset], f01 = values[offset + 1U];
    double f10 = values[offset + stride], f11 = values[offset + stride + 1U];
    if (!isfinite(f00) || !isfinite(f01) || !isfinite(f10) || !isfinite(f11)) {
        return 0;
    }
    *flux = (1.0 - u) * ((1.0 - v) * f00 + v * f01) +
            u * ((1.0 - v) * f10 + v * f11);
    *dd = (1.0 - v) * ((f10 - f00) / hd) + v * ((f11 - f01) / hd);
    *dq = (1.0 - u) * ((f01 - f00) / hq) + u * ((f11 - f10) / hq);
    return isfinite(*flux) && isfinite(*dd) && isfinite(*dq);
}

static namc_flux_result_t namc_flux_cell(const namc_flux_data_t *d,
    const namc_flux_limits_t *limits, size_t i, size_t k, double u, double v,
    namc_dq_t *flux, namc_flux_jacobian_t *jacobian)
{
    double hd = d->id_axis[i + 1U] - d->id_axis[i];
    double hq = d->iq_axis[k + 1U] - d->iq_axis[k];
    size_t offset = i * d->nq + k;
    if (!isfinite(hd) || hd <= 0.0 || !isfinite(hq) || hq <= 0.0 ||
        !isfinite(u) || u < 0.0 || u > 1.0 || !isfinite(v) || v < 0.0 || v > 1.0 ||
        !namc_flux_interpolate(d->psi_d, offset, d->nq, hd, hq, u, v,
                              &flux->d, &jacobian->dd, &jacobian->dq) ||
        !namc_flux_interpolate(d->psi_q, offset, d->nq, hd, hq, u, v,
                              &flux->q, &jacobian->qd, &jacobian->qq)) {
        return NAMC_FLUX_INVALID_MAP;
    }
    return namc_flux_physics(*jacobian, limits);
}

namc_flux_result_t namc_flux_map_prepare(const namc_flux_data_t *data,
    const namc_flux_limits_t *limits, namc_flux_map_t *out)
{
    size_t i, k;
    unsigned int corner;
    if (out == NULL) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    *out = (namc_flux_map_t){0};
    if (!namc_flux_shape_valid(data) || !namc_flux_limits_valid(limits) ||
        data->evidence < NAMC_FLUX_ANALYTICAL || data->evidence > NAMC_FLUX_MEASURED ||
        !namc_flux_source_valid(data->source_id) ||
        !isfinite(data->temperature_k) || data->temperature_k <= 0.0 ||
        !isfinite(data->declared_flux_error_wb) || data->declared_flux_error_wb < 0.0 ||
        !namc_flux_axis_valid(data->id_axis, data->nd) ||
        !namc_flux_axis_valid(data->iq_axis, data->nq)) {
        return NAMC_FLUX_INVALID_MAP;
    }
    for (i = 0U; i + 1U < data->nd; ++i) {
        for (k = 0U; k + 1U < data->nq; ++k) {
            for (corner = 0U; corner < 4U; ++corner) {
                namc_dq_t flux;
                namc_flux_jacobian_t jacobian;
                namc_flux_result_t result = namc_flux_cell(data, limits, i, k,
                    (double)(corner / 2U), (double)(corner % 2U), &flux, &jacobian);
                if (result != NAMC_FLUX_OK) {
                    return result;
                }
            }
        }
    }
    out->data = *data;
    out->limits = *limits;
    out->prepared = NAMC_FLUX_MAP_VERSION;
    return NAMC_FLUX_OK;
}

/* At most ceil(log2(NAMC_FLUX_MAX_AXIS - 1)) comparisons. */
static size_t namc_flux_interval(const double *axis, size_t count, double value)
{
    size_t low = 0U, high = count - 1U;
    while (high - low > 1U) {
        size_t mid = low + (high - low) / 2U;
        if (value < axis[mid]) {
            high = mid;
        } else {
            low = mid;
        }
    }
    return low;
}

namc_flux_result_t namc_flux_map_lookup(const namc_flux_map_t *map,
    namc_dq_t current, namc_dq_t *flux, namc_flux_jacobian_t *jacobian)
{
    const namc_flux_data_t *d;
    size_t i, k;
    double u, v;
    namc_dq_t candidate;
    namc_flux_jacobian_t j;
    namc_flux_result_t result;
    if (map == NULL || flux == NULL || !isfinite(current.d) || !isfinite(current.q)) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    if (map->prepared != NAMC_FLUX_MAP_VERSION || !namc_flux_shape_valid(&map->data) ||
        !namc_flux_limits_valid(&map->limits)) {
        return NAMC_FLUX_INVALID_MAP;
    }
    d = &map->data;
    if (current.d < d->id_axis[0] || current.d > d->id_axis[d->nd - 1U] ||
        current.q < d->iq_axis[0] || current.q > d->iq_axis[d->nq - 1U]) {
        return NAMC_FLUX_OUT_OF_DOMAIN;
    }
    i = namc_flux_interval(d->id_axis, d->nd, current.d);
    k = namc_flux_interval(d->iq_axis, d->nq, current.q);
    u = (current.d - d->id_axis[i]) / (d->id_axis[i + 1U] - d->id_axis[i]);
    v = (current.q - d->iq_axis[k]) / (d->iq_axis[k + 1U] - d->iq_axis[k]);
    result = namc_flux_cell(d, &map->limits, i, k, u, v, &candidate, &j);
    if (result != NAMC_FLUX_OK) {
        return result;
    }
    *flux = candidate;
    if (jacobian != NULL) {
        *jacobian = j;
    }
    return NAMC_FLUX_OK;
}

namc_flux_result_t namc_flux_solve(namc_flux_jacobian_t jacobian,
    double min_rcond, namc_dq_t rhs, namc_dq_t *rate)
{
    namc_flux_jacobian_t n;
    double scale, det, rd, rq;
    namc_dq_t result;
    namc_flux_result_t status;
    if (rate == NULL || !isfinite(rhs.d) || !isfinite(rhs.q)) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    status = namc_flux_matrix(jacobian, min_rcond, &n, &scale, &det);
    if (status != NAMC_FLUX_OK) {
        return status;
    }
    rd = rhs.d / scale;
    rq = rhs.q / scale;
    result.d = (n.qq * rd - n.dq * rq) / det;
    result.q = (n.dd * rq - n.qd * rd) / det;
    if (!isfinite(result.d) || !isfinite(result.q)) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    *rate = result;
    return NAMC_FLUX_OK;
}

namc_flux_result_t namc_flux_torque(namc_dq_t flux, namc_dq_t current,
    unsigned int pole_pairs, double *torque)
{
    double result;
    if (torque == NULL || pole_pairs == 0U || !isfinite(flux.d) || !isfinite(flux.q) ||
        !isfinite(current.d) || !isfinite(current.q)) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    result = 1.5 * (double)pole_pairs * (flux.d * current.q - flux.q * current.d);
    if (!isfinite(result)) {
        return NAMC_FLUX_INVALID_INPUT;
    }
    *torque = result;
    return NAMC_FLUX_OK;
}
