#include "namc/model_current.h"
#include "namc/linear_plant.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Line %d: %s\n", __LINE__, #x); return 0; } } while (0)

static namc_model_current_config_t config(const namc_flux_map_t *map)
{
    const namc_model_current_config_t c = {
        NAMC_MODEL_CURRENT_VERSION,
        {NAMC_REFERENCE_MODEL_VERSION, 0.00005, 3.0, 4.0, 600.0, 600.0, 500.0,
         0.0015, 0.002, 0.04, 12.0, 12.0, 60.0, 2000.0},
        map, NAMC_MODEL_DISABLE
    };
    return c;
}

static int disabled(namc_duty_t d)
{
    return !d.enabled && d.a == 0.5 && d.b == 0.5 && d.c == 0.5;
}

static int compensation_and_guards(void)
{
    const double axis[] = {-4.0, 0.0, 4.0};
    double pd[9], pq[9];
    namc_flux_data_t data = {1U, 1U, 1U, 3U, 3U, 9U, axis, axis, pd, pq,
        NAMC_FLUX_ANALYTICAL, "original-affine-controller-test", 293.15, 0.0};
    const namc_flux_limits_t limits = {1e-5, 1e-10, 1e-6};
    namc_flux_map_t map;
    namc_model_current_config_t c;
    namc_model_current_state_t s;
    namc_current_state_t nominal;
    namc_duty_t duty, expected;
    namc_ab_t ab, volts;
    namc_abc_t phase;
    namc_dq_t v, current = {-2.0, 3.0};
    size_t i, q;
    unsigned int trial;
    for (i = 0U; i < 3U; ++i) {
        for (q = 0U; q < 3U; ++q) {
            pd[3U*i+q] = 0.05 + 0.003*axis[i] + 0.0004*axis[q];
            pq[3U*i+q] = 0.004*axis[q] + 0.0004*axis[i];
        }
    }
    CHECK(namc_flux_map_prepare(&data, &limits, &map) == NAMC_FLUX_OK);
    c = config(&map);
    CHECK(namc_inverse_park(current, 0.37, &ab) == NAMC_OK);
    CHECK(namc_inverse_clarke(ab, &phase) == NAMC_OK);
    namc_model_current_reset(&s);
    /* Zero tracking error isolates feedforward, including both cross terms. */
    CHECK(namc_model_current_step(&c, &s, current, phase, 0.37, 100.0, 48.0, &duty) == NAMC_OK);
    CHECK(namc_average_inverter(duty, 48.0, &volts) == NAMC_OK);
    CHECK(namc_park(volts, 0.37, &v) == NAMC_OK);
    CHECK(fabs(v.d - (-100.0*(0.004*3.0 + 0.0004*(-2.0)))) < 1e-12);
    CHECK(fabs(v.q - (100.0*(0.05 + 0.003*(-2.0) + 0.0004*3.0))) < 1e-12);

    /* Unsupported current is a model fault, never implicit extrapolation. */
    current.d = -5.0;
    CHECK(namc_inverse_park(current, 0.37, &ab) == NAMC_OK);
    CHECK(namc_inverse_clarke(ab, &phase) == NAMC_OK);
    CHECK(namc_model_current_step(&c, &s, current, phase, 0.37, 100.0, 48.0, &duty) == NAMC_MODEL_REJECTED);
    CHECK(disabled(duty) && s.pi.faulted && s.model_failure == NAMC_FLUX_OUT_OF_DOMAIN);
    CHECK(namc_model_current_step(&c, &s, current, phase, 0.37, 100.0, 48.0, &duty) == NAMC_FAULT_LATCHED);

    c.failure_policy = NAMC_MODEL_NOMINAL_FALLBACK;
    namc_model_current_reset(&s);
    s.pi.integral_d = 10.0;
    s.pi.integral_q = -10.0;
    namc_current_reset(&nominal);
    CHECK(namc_current_step(&c.nominal, &nominal, current, phase, 0.37, 100.0, 48.0, &expected) == NAMC_OK);
    CHECK(namc_model_current_step(&c, &s, current, phase, 0.37, 100.0, 48.0, &duty) == NAMC_OK);
    CHECK(s.fallback_latched && s.model_failure == NAMC_FLUX_OUT_OF_DOMAIN);
    CHECK(duty.a == expected.a && duty.b == expected.b && duty.c == expected.c);
    CHECK(s.pi.integral_d == nominal.integral_d && s.pi.integral_q == nominal.integral_q);
    /* A repaired/replaced pointer is not an authorization to retry the map. */
    c.map = NULL;
    CHECK(namc_model_current_step(&c, &s, current, phase, 0.37, 100.0, 48.0, &duty) == NAMC_OK);
    CHECK(s.fallback_latched && s.model_failure == NAMC_FLUX_OUT_OF_DOMAIN);
    c.failure_policy = NAMC_MODEL_DISABLE;
    CHECK(namc_model_current_step(&c, &s, current, phase, 0.37, 100.0, 48.0, &duty) == NAMC_MODEL_REJECTED);
    CHECK(disabled(duty));

    /* Invalid sensors/state/hard limits must never enter nominal fallback. */
    for (trial = 0U; trial < 9U; ++trial) {
        double bus = 48.0, speed = 100.0, angle = 0.37;
        namc_abc_t measured = phase;
        namc_dq_t ref = current;
        c = config(NULL);
        c.failure_policy = NAMC_MODEL_NOMINAL_FALLBACK;
        namc_model_current_reset(&s);
        if (trial == 0U) { bus = NAN; }
        if (trial == 1U) { bus = 61.0; }
        if (trial == 2U) { speed = 2001.0; }
        if (trial == 3U) { measured.a = INFINITY; }
        if (trial == 4U) { measured.a += 0.1; }
        if (trial == 5U) { angle = NAN; }
        if (trial == 6U) { ref.q = NAN; }
        if (trial == 7U) { s.pi.integral_d = NAN; }
        if (trial == 8U) { c.nominal.current_limit = 1.0; }
        CHECK(namc_model_current_step(&c, &s, ref, measured, angle, speed, bus, &duty) != NAMC_OK);
        CHECK(disabled(duty) && s.pi.faulted && !s.fallback_latched);
    }
    c = config(&map);
    namc_model_current_reset(&s);
    c.version = 99U;
    CHECK(namc_model_current_step(&c, &s, current, phase, 0.37, 100.0, 48.0, &duty) == NAMC_INVALID_INPUT);
    CHECK(disabled(duty));
    c = config(&map);
    namc_model_current_reset(&s);
    c.nominal.kp_d = DBL_MAX;
    current.d = 4.0;
    CHECK(namc_model_current_step(&c, &s, current, phase, 0.37, 100.0, 48.0, &duty) == NAMC_MODEL_REJECTED);
    /* Valid model domain plus overflowing PI command cannot reach PWM. */
    current = (namc_dq_t){0.0, 0.0};
    phase = (namc_abc_t){0.0, 0.0, 0.0};
    namc_model_current_reset(&s);
    CHECK(namc_model_current_step(&c, &s, (namc_dq_t){4.0, 0.0}, phase,
        0.0, 0.0, 48.0, &duty) == NAMC_INVALID_INPUT);
    CHECK(disabled(duty));

    /* Sustained voltage limitation and finite post-limit state exercise shared
     * antiwindup. This fixed-measurement check is not a closed-loop recovery test. */
    c = config(&map);
    namc_model_current_reset(&s);
    for (trial = 0U; trial < 500U; ++trial) {
        CHECK(namc_model_current_step(&c, &s, (namc_dq_t){8.0, 8.0}, phase,
            0.0, 0.0, 12.0, &duty) == NAMC_OK);
        CHECK(duty.enabled && duty.a >= 0.0 && duty.a <= 1.0 &&
            duty.b >= 0.0 && duty.b <= 1.0 && duty.c >= 0.0 && duty.c <= 1.0);
        CHECK(fabs(s.pi.integral_d) <= 12.0/sqrt(3.0) && fabs(s.pi.integral_q) <= 12.0/sqrt(3.0));
    }
    for (trial = 0U; trial < 2000U; ++trial) {
        CHECK(namc_model_current_step(&c, &s, current, phase, 0.0, 0.0, 48.0, &duty) == NAMC_OK);
        CHECK(isfinite(s.pi.integral_d) && isfinite(s.pi.integral_q));
    }
    return 1;
}

static int invalid_model_and_handles(void)
{
    namc_flux_map_t invalid = {0};
    namc_model_current_config_t c = config(&invalid);
    namc_model_current_state_t s;
    namc_duty_t duty;
    const namc_dq_t ref = {0.0, 0.0};
    const namc_abc_t measured = {0.0, 0.0, 0.0};
    namc_model_current_reset(&s);
    CHECK(namc_model_current_step(&c, &s, ref, measured, 0.0, 0.0, 48.0, &duty) == NAMC_MODEL_REJECTED);
    CHECK(disabled(duty) && s.pi.faulted && s.model_failure != NAMC_FLUX_OK);
    namc_model_current_reset(&s);
    c.failure_policy = NAMC_MODEL_NOMINAL_FALLBACK;
    CHECK(namc_model_current_step(&c, &s, ref, measured, 0.0, 0.0, 48.0, &duty) == NAMC_OK);
    CHECK(s.fallback_latched && s.model_failure != NAMC_FLUX_OK && duty.enabled);
    namc_model_current_reset(&s);
    c.failure_policy = (namc_model_failure_policy_t)99;
    CHECK(namc_model_current_step(&c, &s, ref, measured, 0.0, 0.0, 48.0, &duty) == NAMC_INVALID_INPUT);
    CHECK(disabled(duty) && s.pi.faulted);
    namc_model_current_reset(&s);
    CHECK(namc_model_current_step(NULL, &s, ref, measured, 0.0, 0.0, 48.0, &duty) == NAMC_INVALID_INPUT);
    CHECK(disabled(duty));
    CHECK(namc_model_current_step(&c, NULL, ref, measured, 0.0, 0.0, 48.0, &duty) == NAMC_INVALID_INPUT);
    CHECK(disabled(duty));
    namc_model_current_reset(&s);
    c = config(NULL);
    c.failure_policy = NAMC_MODEL_NOMINAL_FALLBACK;
    CHECK(namc_model_current_step(&c, &s, ref, measured, 0.0, 0.0, 48.0, NULL) == NAMC_INVALID_INPUT);
    CHECK(s.pi.faulted && !s.fallback_latched);
    return 1;
}

int main(void)
{
    return compensation_and_guards() && invalid_model_and_handles() ? 0 : 1;
}
