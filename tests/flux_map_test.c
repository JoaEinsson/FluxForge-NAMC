#include "namc/flux_map.h"
#include "namc/magnetic_plant.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); \
    return 0; } } while (0)

/* Synthetic fixture only; polynomial coefficients are not motor measurements.
 * psi is the gradient of W = .05*d + .003*d^2/2 + .004*q^2/2
 * - 2e-6*d^4/4 - 3e-6*q^4/4 - 1.2e-6*d^2*q^2/2. */
#define GRID 33U
static double axis[GRID], psi_d[GRID * GRID], psi_q[GRID * GRID];
static const namc_flux_limits_t limits = {1e-5, 1e-5, 1e-6};

static int near(double a, double b, double tolerance)
{
    return isfinite(a) && isfinite(b) && fabs(a - b) <= tolerance;
}

static namc_dq_t oracle_flux(double d, double q)
{
    namc_dq_t f = {0.05 + 0.003 * d - 2e-6 * d * d * d - 1.2e-6 * d * q * q,
                  0.004 * q - 3e-6 * q * q * q - 1.2e-6 * d * d * q};
    return f;
}

static namc_flux_jacobian_t oracle_jacobian(double d, double q)
{
    namc_flux_jacobian_t j = {
        0.003 - 6e-6 * d * d - 1.2e-6 * q * q, -2.4e-6 * d * q,
        -2.4e-6 * d * q, 0.004 - 9e-6 * q * q - 1.2e-6 * d * d
    };
    return j;
}

static namc_flux_data_t fixture(int nonlinear)
{
    size_t i, k;
    namc_flux_data_t data = {
        NAMC_FLUX_MAP_VERSION, NAMC_FLUX_SI_UNITS, NAMC_FLUX_AMPLITUDE_INVARIANT_DQ,
        GRID, GRID, GRID * GRID, axis, axis, psi_d, psi_q,
        NAMC_FLUX_ANALYTICAL, "synthetic-polynomial-v1", 293.15, 8e-6
    };
    for (i = 0U; i < GRID; ++i) {
        axis[i] = -8.0 + 0.5 * (double)i;
    }
    for (i = 0U; i < GRID; ++i) {
        for (k = 0U; k < GRID; ++k) {
            namc_dq_t f = nonlinear ? oracle_flux(axis[i], axis[k]) :
                (namc_dq_t){0.05 + 0.003 * axis[i], 0.004 * axis[k]};
            psi_d[i * GRID + k] = f.d;
            psi_q[i * GRID + k] = f.q;
        }
    }
    return data;
}

static int exact_lookup(void)
{
    const double da[] = {-3.0, -0.5, 1.0, 7.0}, qa[] = {-4.0, 0.0, 6.0};
    double fd[12], fq[12];
    namc_flux_data_t data = {
        NAMC_FLUX_MAP_VERSION, NAMC_FLUX_SI_UNITS, NAMC_FLUX_AMPLITUDE_INVARIANT_DQ,
        4U, 3U, 12U, da, qa, fd, fq, NAMC_FLUX_ANALYTICAL, "affine-coupled", 293.15, 0.0
    };
    namc_flux_map_t map;
    namc_dq_t flux;
    namc_flux_jacobian_t j;
    size_t i, k;
    for (i = 0U; i < 4U; ++i) {
        for (k = 0U; k < 3U; ++k) {
            fd[i * 3U + k] = 0.05 + 0.003 * da[i] + 0.0004 * qa[k];
            fq[i * 3U + k] = 0.0004 * da[i] + 0.004 * qa[k];
        }
    }
    CHECK(namc_flux_map_prepare(&data, &limits, &map) == NAMC_FLUX_OK);
    for (i = 0U; i <= 20U; ++i) {
        namc_dq_t current = {-3.0 + 0.5 * (double)i, -4.0 + 0.5 * (double)i};
        CHECK(namc_flux_map_lookup(&map, current, &flux, &j) == NAMC_FLUX_OK);
        CHECK(near(flux.d, 0.05 + 0.003 * current.d + 0.0004 * current.q, 1e-15));
        CHECK(near(flux.q, 0.0004 * current.d + 0.004 * current.q, 1e-15));
        CHECK(near(j.dd, 0.003, 1e-15) && near(j.qq, 0.004, 1e-15));
        CHECK(near(j.dq, 0.0004, 1e-15) && near(j.qd, 0.0004, 1e-15));
    }
    flux = (namc_dq_t){7.0, 9.0};
    j.dd = 77.0;
    CHECK(namc_flux_map_lookup(&map, (namc_dq_t){nextafter(7.0, INFINITY), 0.0},
        &flux, &j) == NAMC_FLUX_OUT_OF_DOMAIN);
    CHECK(flux.d == 7.0 && flux.q == 9.0 && j.dd == 77.0);
    CHECK(namc_flux_map_lookup(&map, (namc_dq_t){0.0, -4.1}, &flux, NULL) == NAMC_FLUX_OUT_OF_DOMAIN);
    CHECK(namc_flux_map_lookup(&map, (namc_dq_t){NAN, 0.0}, &flux, NULL) == NAMC_FLUX_INVALID_INPUT);
    CHECK(namc_flux_map_lookup(&map, (namc_dq_t){0.0, 0.0}, &flux, NULL) == NAMC_FLUX_OK);
    return 1;
}

static int invalid_maps(void)
{
    unsigned int n;
    for (n = 0U; n < 14U; ++n) {
        namc_flux_data_t data = fixture(0);
        namc_flux_limits_t policy = limits;
        namc_flux_map_t map;
        namc_dq_t flux = {7.0, 9.0};
        CHECK(namc_flux_map_prepare(&data, &policy, &map) == NAMC_FLUX_OK);
        switch (n) {
            case 0U: data.version = 99U; break;
            case 1U: data.units = 0U; break;
            case 2U: data.convention = 0U; break;
            case 3U: data.count -= 1U; break;
            case 4U: data.nd = 1U; break;
            case 5U: data.nq = NAMC_FLUX_MAX_AXIS + 1U; break;
            case 6U: axis[4] = axis[3]; break;
            case 7U: psi_d[5] = NAN; break;
            case 8U: psi_q[100] = INFINITY; break;
            case 9U: data.source_id = ""; break;
            case 10U: data.temperature_k = -1.0; break;
            case 11U: data.declared_flux_error_wb = -1.0; break;
            case 12U: policy.min_rcond = 0.0; break;
            default: data.psi_d = NULL; break;
        }
        CHECK(namc_flux_map_prepare(&data, &policy, &map) == NAMC_FLUX_INVALID_MAP);
        CHECK(map.prepared == 0U);
        CHECK(namc_flux_map_lookup(&map, (namc_dq_t){0.0, 0.0}, &flux, NULL) == NAMC_FLUX_INVALID_MAP);
        CHECK(flux.d == 7.0 && flux.q == 9.0);
    }
    for (n = 0U; n < 4U; ++n) {
        const double a[] = {-1.0, 1.0};
        double fd[4], fq[4];
        namc_flux_data_t data = {
            NAMC_FLUX_MAP_VERSION, NAMC_FLUX_SI_UNITS, NAMC_FLUX_AMPLITUDE_INVARIANT_DQ,
            2U, 2U, 4U, a, a, fd, fq, NAMC_FLUX_ANALYTICAL, "invalid-physics", 293.15, 0.0
        };
        namc_flux_map_t map;
        const namc_flux_result_t expected[] = {
            NAMC_FLUX_ILL_CONDITIONED, NAMC_FLUX_NON_PASSIVE,
            NAMC_FLUX_NON_RECIPROCAL, NAMC_FLUX_ILL_CONDITIONED
        };
        size_t i, k;
        for (i = 0U; i < 2U; ++i) {
            for (k = 0U; k < 2U; ++k) {
                fd[2U * i + k] = (n == 1U ? -0.003 : 0.003) * a[i];
                fq[2U * i + k] = (n == 0U ? 0.0 : n == 3U ? 1e-12 : 0.004) * a[k];
                if (n == 2U) {
                    fd[2U * i + k] += 0.001 * a[k];
                }
            }
        }
        CHECK(namc_flux_map_prepare(&data, &limits, &map) == expected[n]);
    }
    return 1;
}

static int scaled_solve(void)
{
    const double scales[] = {1e-200, 1.0, 1e200};
    namc_dq_t rate;
    unsigned int i;
    for (i = 0U; i < 3U; ++i) {
        double s = scales[i];
        namc_flux_jacobian_t j = {2.0 * s, 0.5 * s, 0.25 * s, 3.0 * s};
        CHECK(namc_flux_solve(j, 1e-6, (namc_dq_t){2.5 * s, -8.5 * s}, &rate) == NAMC_FLUX_OK);
        CHECK(near(rate.d, 2.0, 1e-14) && near(rate.q, -3.0, 1e-14));
    }
    rate = (namc_dq_t){7.0, 9.0};
    CHECK(namc_flux_solve((namc_flux_jacobian_t){1.0, 1.0, 1.0, 1.0}, 1e-6,
        (namc_dq_t){1.0, 1.0}, &rate) == NAMC_FLUX_ILL_CONDITIONED);
    CHECK(rate.d == 7.0 && rate.q == 9.0);
    CHECK(namc_flux_solve((namc_flux_jacobian_t){NAN, 0.0, 0.0, 1.0}, 1e-6,
        (namc_dq_t){1.0, 1.0}, &rate) == NAMC_FLUX_INVALID_INPUT);
    CHECK(namc_flux_solve((namc_flux_jacobian_t){1e-200, 0.0, 0.0, 1e-200}, 1e-6,
        (namc_dq_t){DBL_MAX, 1.0}, &rate) == NAMC_FLUX_INVALID_INPUT);
    return 1;
}

static int nonlinear_lookup(void)
{
    namc_flux_data_t data = fixture(1);
    namc_flux_map_t map;
    size_t i, k;
    double max_error = 0.0, max_derivative_error = 0.0, max_torque_error = 0.0;
    CHECK(namc_flux_map_prepare(&data, &limits, &map) == NAMC_FLUX_OK);
    for (i = 0U; i < GRID - 1U; ++i) {
        for (k = 0U; k < GRID - 1U; ++k) {
            double d = axis[i] + 0.17, q = axis[k] + 0.29;
            const double epsilon = 1e-5;
            namc_dq_t f, plus, minus, expected = oracle_flux(d, q);
            namc_flux_jacobian_t j, truth = oracle_jacobian(d, q);
            double torque, expected_torque = 6.0 * (expected.d * q - expected.q * d);
            CHECK(namc_flux_map_lookup(&map, (namc_dq_t){d, q}, &f, &j) == NAMC_FLUX_OK);
            max_error = fmax(max_error, fmax(fabs(f.d - expected.d), fabs(f.q - expected.q)));
            max_derivative_error = fmax(max_derivative_error, fmax(fabs(j.dd - truth.dd),
                fmax(fabs(j.dq - truth.dq), fmax(fabs(j.qd - truth.qd), fabs(j.qq - truth.qq)))));
            CHECK(namc_flux_torque(f, (namc_dq_t){d, q}, 4U, &torque) == NAMC_FLUX_OK);
            max_torque_error = fmax(max_torque_error, fabs(torque - expected_torque));
            /* Independent finite-difference oracle in tests only. No probes
             * are used to differentiate the maps in the runtime code. */
            CHECK(namc_flux_map_lookup(&map, (namc_dq_t){d + epsilon, q}, &plus, NULL) == NAMC_FLUX_OK);
            CHECK(namc_flux_map_lookup(&map, (namc_dq_t){d - epsilon, q}, &minus, NULL) == NAMC_FLUX_OK);
            CHECK(near((plus.d - minus.d) / (2.0 * epsilon), j.dd, 1e-10));
            CHECK(near((plus.q - minus.q) / (2.0 * epsilon), j.qd, 1e-10));
            CHECK(namc_flux_map_lookup(&map, (namc_dq_t){d, q + epsilon}, &plus, NULL) == NAMC_FLUX_OK);
            CHECK(namc_flux_map_lookup(&map, (namc_dq_t){d, q - epsilon}, &minus, NULL) == NAMC_FLUX_OK);
            CHECK(near((plus.d - minus.d) / (2.0 * epsilon), j.dq, 1e-10));
            CHECK(near((plus.q - minus.q) / (2.0 * epsilon), j.qq, 1e-10));
        }
    }
    CHECK(max_error < 8e-6 && max_derivative_error < 2e-5 && max_torque_error < 0.001);
    {
        namc_dq_t f, low, high;
        namc_flux_jacobian_t j, center;
        CHECK(namc_flux_map_lookup(&map, (namc_dq_t){4.0, 5.0}, &f, &j) == NAMC_FLUX_OK);
        CHECK(namc_flux_map_lookup(&map, (namc_dq_t){0.0, 0.0}, &low, &center) == NAMC_FLUX_OK);
        CHECK(j.dd < center.dd && j.qq < center.qq && j.dq < -4e-5 && j.qd < -4e-5);
        CHECK(namc_flux_map_lookup(&map, (namc_dq_t){4.0, 0.0}, &low, NULL) == NAMC_FLUX_OK);
        CHECK(namc_flux_map_lookup(&map, (namc_dq_t){0.0, 5.0}, &high, NULL) == NAMC_FLUX_OK);
        CHECK(fabs(f.d - low.d) > 1e-4 && fabs(f.q - high.q) > 5e-5);
        /* At d=4, use the [4,4.5] cell, not the lower cell's derivative. */
        CHECK(near(j.dd, (oracle_flux(4.5, 5.0).d - oracle_flux(4.0, 5.0).d) / 0.5, 1e-14));
        CHECK(namc_flux_map_lookup(&map, (namc_dq_t){8.0, 8.0}, &f, &j) == NAMC_FLUX_OK);
        CHECK(near(f.d, oracle_flux(8.0, 8.0).d, 1e-15));
    }
    printf("Held-out LUT maxima: flux %.9g Wb, Jacobian %.9g H, torque %.9g Nm\n",
        max_error, max_derivative_error, max_torque_error);
    return 1;
}

static int energy_consistency(void)
{
    namc_flux_data_t data = fixture(1);
    namc_flux_map_t map;
    namc_dq_t bottom, top, left, right;
    double circulation;
    CHECK(namc_flux_map_prepare(&data, &limits, &map) == NAMC_FLUX_OK);
    /* Sub-cell loop: midpoint integration is exact along bilinear cell edges.
     * Nonzero curl is bounded by the configured reciprocity tolerance. */
    CHECK(namc_flux_map_lookup(&map, (namc_dq_t){4.25, 5.1}, &bottom, NULL) == NAMC_FLUX_OK);
    CHECK(namc_flux_map_lookup(&map, (namc_dq_t){4.25, 5.3}, &top, NULL) == NAMC_FLUX_OK);
    CHECK(namc_flux_map_lookup(&map, (namc_dq_t){4.1, 5.2}, &left, NULL) == NAMC_FLUX_OK);
    CHECK(namc_flux_map_lookup(&map, (namc_dq_t){4.4, 5.2}, &right, NULL) == NAMC_FLUX_OK);
    circulation = 0.3 * (bottom.d - top.d) + 0.2 * (right.q - left.q);
    CHECK(fabs(circulation) <= limits.max_reciprocity_error_h * 0.3 * 0.2 + 1e-15);
    /* Strict reciprocity rejects this interpolation approximation; it is not
     * silently symmetrized or described as an exactly conservative fit. */
    {
        namc_flux_limits_t strict = limits;
        strict.max_reciprocity_error_h = 1e-12;
        CHECK(namc_flux_map_prepare(&data, &strict, &map) == NAMC_FLUX_NON_RECIPROCAL);
    }
    printf("Sub-cell flux circulation: %.9g Wb A\n", circulation);
    return 1;
}

static int plant_linear_limit_and_rejection(void)
{
    namc_flux_data_t data = fixture(0);
    namc_flux_map_t map;
    const namc_linear_parameters_t linear = {
        NAMC_REFERENCE_MODEL_VERSION, 0.4, 0.003, 0.004, 0.05, 0.01, 0.001, 4U
    };
    namc_magnetic_parameters_t nonlinear = {NAMC_MAGNETIC_PLANT_VERSION, &map, 0.4, 0.01, 0.001, 4U};
    namc_linear_state_t a = {-1.0, 1.0, 2.0, 0.5}, b = a;
    unsigned int n;
    CHECK(namc_flux_map_prepare(&data, &limits, &map) == NAMC_FLUX_OK);
    for (n = 0U; n < 2000U; ++n) {
        CHECK(namc_linear_step(&linear, &a, (namc_ab_t){2.0, 1.0}, 0.1, 0.000025) == NAMC_OK);
        CHECK(namc_magnetic_step(&nonlinear, &b, (namc_ab_t){2.0, 1.0}, 0.1, 0.000025) == NAMC_FLUX_OK);
        CHECK(near(a.id, b.id, 1e-10) && near(a.iq, b.iq, 1e-10));
        CHECK(near(a.speed, b.speed, 1e-10) && near(a.angle, b.angle, 1e-10));
    }
    b = (namc_magnetic_state_t){7.9, 0.0, 0.0, 0.0};
    CHECK(namc_magnetic_step(&nonlinear, &b, (namc_ab_t){100.0, 0.0}, 0.0, 0.001) == NAMC_FLUX_OUT_OF_DOMAIN);
    CHECK(b.id == 7.9 && b.iq == 0.0 && b.speed == 0.0 && b.angle == 0.0);
    CHECK(namc_magnetic_step(&nonlinear, &b, (namc_ab_t){NAN, 0.0}, 0.0, 0.001) == NAMC_FLUX_INVALID_INPUT);
    CHECK(b.id == 7.9 && b.angle == 0.0);
    nonlinear.inertia = 0.0;
    CHECK(namc_magnetic_step(&nonlinear, &b, (namc_ab_t){0.0, 0.0}, 0.0, 0.001) == NAMC_FLUX_INVALID_INPUT);
    return 1;
}

static int run_nonlinear_loop(double dt, unsigned int steps, namc_magnetic_state_t *final_state)
{
    namc_flux_data_t data = fixture(1);
    namc_flux_map_t map;
    namc_magnetic_parameters_t plant = {NAMC_MAGNETIC_PLANT_VERSION, &map, 0.4, 0.01, 0.001, 4U};
    namc_magnetic_state_t state = {0.0, 0.0, 0.0, 0.37};
    /* Nominal priors configured independently of hidden fixture/map values. */
    const namc_current_config_t controller = {
        NAMC_REFERENCE_MODEL_VERSION, dt, 3.0, 4.0, 600.0, 600.0, 500.0,
        0.0015, 0.002, 0.04, 7.0, 12.0, 60.0, 2000.0
    };
    const namc_dq_t reference = {-2.0, 5.0};
    namc_current_state_t control;
    double sum = 0.0;
    unsigned int n;
    CHECK(namc_flux_map_prepare(&data, &limits, &map) == NAMC_FLUX_OK);
    namc_current_reset(&control);
    for (n = 0U; n < steps; ++n) {
        namc_ab_t measured, voltage;
        namc_abc_t phase;
        namc_duty_t duty;
        CHECK(namc_inverse_park((namc_dq_t){state.id, state.iq}, state.angle, &measured) == NAMC_OK);
        CHECK(namc_inverse_clarke(measured, &phase) == NAMC_OK);
        CHECK(namc_current_step(&controller, &control, reference, phase, state.angle,
            4.0 * state.speed, 48.0, &duty) == NAMC_OK);
        CHECK(duty.enabled == 1 && duty.a >= 0.0 && duty.a <= 1.0);
        CHECK(duty.b >= 0.0 && duty.b <= 1.0 && duty.c >= 0.0 && duty.c <= 1.0);
        CHECK(namc_average_inverter(duty, 48.0, &voltage) == NAMC_OK);
        CHECK(namc_magnetic_step(&plant, &state, voltage, 0.2, controller.sample_time) == NAMC_FLUX_OK);
        CHECK(hypot(state.id, state.iq) <= controller.current_limit);
        if (n >= steps / 2U) {
            sum += (state.id - reference.d) * (state.id - reference.d) +
                   (state.iq - reference.q) * (state.iq - reference.q);
        }
    }
    CHECK(sqrt(sum / (double)(steps - steps / 2U)) < 0.06);
    printf("Nonlinear loop dt %.9g s: final id %.9g A, iq %.9g A, tail RMS %.9g A\n",
        dt, state.id, state.iq, sqrt(sum / (double)(steps - steps / 2U)));
    *final_state = state;
    return 1;
}

static int nonlinear_closed_loop(void)
{
    namc_magnetic_state_t coarse, fine;
    CHECK(run_nonlinear_loop(0.00005, 2000U, &coarse));
    CHECK(run_nonlinear_loop(0.000025, 4000U, &fine));
    CHECK(near(coarse.id, fine.id, 0.02) && near(coarse.iq, fine.iq, 0.02));
    CHECK(near(coarse.speed, fine.speed, 0.02));
    printf("Nonlinear refinement deltas: id %.9g A, iq %.9g A, speed %.9g rad/s\n",
        fabs(coarse.id - fine.id), fabs(coarse.iq - fine.iq), fabs(coarse.speed - fine.speed));
    return 1;
}

int main(void)
{
    if (!exact_lookup() || !invalid_maps() || !scaled_solve() || !nonlinear_lookup() ||
        !energy_consistency() || !plant_linear_limit_and_rejection() || !nonlinear_closed_loop()) {
        return 1;
    }
    puts("Coupled flux LUT and nonlinear plant tests passed (synthetic evidence only).");
    return 0;
}
