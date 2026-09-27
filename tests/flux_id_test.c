#include "namc/flux_id.h"

#include <math.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Line %d: %s\n", __LINE__, #x); return 0; } } while (0)

static namc_fi_config_t config(void)
{
    const namc_fi_config_t c = {
        NAMC_FLUX_ID_VERSION, 0.00005, {4.0, 3.0}, {80.0, -80.0, 40.0}, 1U, 2U, 20U,
        0.4, 0.001, 0.0001, 0.0001, 0.001, 0.001, 0.02,
        0.002, 0.002, 0.01, 0.01, 0.1, 5.0, 10.0, 0.2, 0.0001, 0.05, 0.005, 0.001
    };
    return c;
}

static const namc_fi_limits_t limits = {8.0, 25.0, 200.0, 30.0};

static namc_dq_t flux(namc_dq_t i)
{
    return (namc_dq_t){0.05+0.003*i.d+0.0004*i.q, 0.0004*i.d+0.004*i.q};
}

static namc_rs_observation_t sample(const namc_fi_state_t *s)
{
    namc_rs_observation_t o = {0};
    namc_dq_t f = flux(s->config.target_current);
    o.sequence = s->elapsed_samples+1U;
    o.current = s->config.target_current; o.electrical_speed = s->config.speeds[s->stage];
    o.terminal_voltage = (namc_dq_t){0.4*o.current.d-o.electrical_speed*f.q+0.01,
        0.4*o.current.q+o.electrical_speed*f.d-0.02};
    o.voltage_source = NAMC_RS_IDEAL_TERMINAL;
    return o;
}

static int analytical(void)
{
    unsigned int trial;
    for (trial = 0U; trial < 3U; ++trial) {
        namc_fi_config_t c = config();
        namc_fi_limits_t l = limits;
        namc_fi_state_t s;
        namc_dq_t i, expected;
        double speed;
        unsigned int n = 0U;
        if (trial == 1U) { c.target_current = (namc_dq_t){0.0, 0.0}; }
        if (trial == 2U) { c.resistance = 0.4005; } /* R mismatch enters offset/bounds, not hidden truth. */
        expected = flux(c.target_current);
        CHECK(namc_fi_start(&c, &l, &s) == NAMC_OK);
        c.speeds[0] = 999.0; l.max_current = 999.0;
        while (namc_fi_request(&s, &i, &speed)) {
            namc_rs_observation_t o = sample(&s);
            CHECK(i.d == s.config.target_current.d && speed == s.config.speeds[s.stage]);
            CHECK(s.result.flux.d == 0.0 && s.result.flux.q == 0.0);
            (void)namc_fi_observe(&s, &o);
            CHECK(++n <= 9U);
        }
        CHECK(n == 9U && s.phase == NAMC_FI_COMPLETE && s.reason == NAMC_FI_NONE);
        CHECK(fabs(s.result.flux.d-expected.d) < 1e-14 && fabs(s.result.flux.q-expected.q) < 1e-14);
        CHECK(s.result.uncertainty.d > 0.0 && s.result.uncertainty.q > 0.0);
        CHECK(fabs(s.result.holdout_residual.d) < 1e-14 && fabs(s.result.holdout_residual.q) < 1e-14);
        CHECK(fabs(s.result.voltage_offset.d-(0.01+(0.4-s.config.resistance)*s.config.target_current.d)) < 1e-14);
        CHECK(fabs(s.result.voltage_offset.q-(-0.02+(0.4-s.config.resistance)*s.config.target_current.q)) < 1e-14);
        CHECK(s.result.pm_reference == (trial == 1U));
        CHECK(i.d == 0.0 && i.q == 0.0 && speed == 0.0 && s.limits.max_current == 8.0);
        CHECK(namc_fi_observe(&s, NULL) == NAMC_FI_COMPLETE);
        namc_fi_abort(&s);
        CHECK(s.phase == NAMC_FI_FAULT && s.reason == NAMC_FI_ABORTED && s.result.flux.d == 0.0);
        CHECK(!namc_fi_request(&s, &i, &speed));
        c = config();
        CHECK(namc_fi_start(&c, &limits, &s) == NAMC_OK && s.elapsed_samples == 0U);
    }
    return 1;
}

static int guards(void)
{
    unsigned int trial;
    for (trial = 0U; trial < 13U; ++trial) {
        namc_fi_config_t c = config(); namc_fi_state_t s;
        namc_fi_limits_t l = limits; namc_rs_observation_t o;
        namc_dq_t i; double speed;
        namc_fi_reason_t expected = NAMC_FI_OBSERVATION;
        if (trial == 9U) { l.max_apparent_energy = 1e-8; }
        CHECK(namc_fi_start(&c, &l, &s) == NAMC_OK); o = sample(&s);
        switch (trial) {
        case 0U: o.current.d = NAN; break;
        case 1U: o.terminal_voltage.q = INFINITY; break;
        case 2U: o.electrical_speed = NAN; break;
        case 3U: ++o.sequence; break;
        case 4U: o.current.q = 9.0; expected = NAMC_FI_LIMIT; break;
        case 5U: o.terminal_voltage.d = 26.0; expected = NAMC_FI_LIMIT; break;
        case 6U: o.electrical_speed = 201.0; expected = NAMC_FI_LIMIT; break;
        case 7U: o.voltage_source = NAMC_RS_COMMAND_ONLY; expected = NAMC_FI_SOURCE; break;
        case 8U: o.voltage_source = (namc_rs_voltage_source_t)99; expected = NAMC_FI_SOURCE; break;
        case 9U: expected = NAMC_FI_LIMIT; break;
        case 10U: s.apparent_energy = NAN; break;
        case 11U: s.version = 99U; expected = NAMC_FI_CONFIG; break;
        default:
            (void)namc_fi_observe(&s, &o); o = sample(&s);
            o.voltage_source = NAMC_RS_MEASURED_TERMINAL; expected = NAMC_FI_SOURCE; break;
        }
        (void)namc_fi_observe(&s, &o);
        CHECK(s.reason == expected && (s.phase == NAMC_FI_FAULT || s.phase == NAMC_FI_DEGRADED));
        CHECK(!namc_fi_request(&s, &i, &speed) && i.d == 0.0 && speed == 0.0);
        CHECK(s.result.flux.d == 0.0 && s.result.flux.q == 0.0);
        if (trial == 9U) { CHECK(s.apparent_energy > l.max_apparent_energy); }
        (void)namc_fi_observe(&s, NULL); CHECK(s.reason == expected);
    }
    return 1;
}

static int quality(void)
{
    const namc_fi_reason_t reasons[] = {NAMC_FI_UNOBSERVABLE, NAMC_FI_TRACKING, NAMC_FI_UNSETTLED,
        NAMC_FI_OFFSET, NAMC_FI_UNCERTAINTY, NAMC_FI_VALIDATION, NAMC_FI_TIMEOUT, NAMC_FI_RANGE,
        NAMC_FI_REFERENCE, NAMC_FI_TRACKING, NAMC_FI_UNSETTLED, NAMC_FI_UNSETTLED};
    unsigned int trial;
    for (trial = 0U; trial < sizeof(reasons)/sizeof(reasons[0]); ++trial) {
        namc_fi_config_t c = config(); namc_fi_state_t s; namc_dq_t i;
        unsigned int n = 0U; double speed;
        if (trial == 4U) { c.voltage_uncertainty = 0.1; }
        if (trial == 6U) { c.max_samples = 4U; }
        if (trial == 7U) { c.max_flux = 0.02; }
        if (trial == 8U) { c.target_current = (namc_dq_t){0.0, 0.0}; }
        CHECK(namc_fi_start(&c, &limits, &s) == NAMC_OK);
        while (namc_fi_request(&s, &i, &speed)) {
            namc_rs_observation_t o = sample(&s);
            if (trial == 0U) { o.electrical_speed = 0.0; }
            if (trial == 1U) { o.current.q += 0.02; }
            if (trial == 2U) { o.current.d += (n % 2U) ? 0.0015 : -0.0015; }
            if (trial == 3U) { o.terminal_voltage.d += 0.1; }
            if (trial == 5U && s.stage == 2U) { o.terminal_voltage.q += 0.02; }
            if (trial == 8U) { o.terminal_voltage.d -= o.electrical_speed*0.01; }
            if (trial == 9U) { o.electrical_speed += 0.2; }
            if (trial == 10U) { o.terminal_voltage.d += (n % 2U) ? 0.0015 : -0.0015; }
            if (trial == 11U) { o.electrical_speed += (n % 2U) ? 0.007 : -0.007; }
            (void)namc_fi_observe(&s, &o); CHECK(++n <= 9U);
        }
        CHECK(s.phase == NAMC_FI_DEGRADED && s.reason == reasons[trial] && s.result.flux.d == 0.0);
    }
    return 1;
}

static int invalid_config(void)
{
    unsigned int trial;
    for (trial = 0U; trial < 18U; ++trial) {
        namc_fi_config_t c = config(); namc_fi_limits_t l = limits; namc_fi_state_t s;
        switch (trial) {
        case 0U: c.version = 0U; break;
        case 1U: c.sample_time = NAN; break;
        case 2U: c.target_current.d = 9.0; break;
        case 3U: c.speeds[1] = 80.0; break;
        case 4U: c.speeds[2] = 80.0; break;
        case 5U: c.resistance = 0.0; break;
        case 6U: c.resistance_uncertainty = 0.5; break;
        case 7U: c.current_uncertainty = -0.1; break;
        case 8U: c.gradient_bound = 0.0; break;
        case 9U: c.model_voltage_error = -0.1; break;
        case 10U: c.measure_samples = 1U; break;
        case 11U: c.max_samples = NAMC_RESISTANCE_MAX_SAMPLES+1U; break;
        case 12U: c.max_origin_q_flux = 0.0; break;
        case 13U: l.max_electrical_speed = 40.0; break;
        case 14U: l.max_apparent_energy = 0.0; break;
        case 15U: c.speed_uncertainty = 100.0; break;
        case 16U: c.max_voltage_span = INFINITY; break;
        default: c.max_flux_uncertainty = 0.0; break;
        }
        CHECK(namc_fi_start(&c, &l, &s) == NAMC_INVALID_INPUT && s.reason == NAMC_FI_CONFIG);
    }
    {
        namc_fi_config_t c = config(); namc_fi_state_t s; namc_dq_t i; double speed;
        CHECK(namc_fi_start(NULL, &limits, &s) == NAMC_INVALID_INPUT);
        CHECK(namc_fi_start(&c, NULL, &s) == NAMC_INVALID_INPUT);
        CHECK(namc_fi_start(&c, &limits, NULL) == NAMC_INVALID_INPUT);
        CHECK(!namc_fi_request(NULL, &i, &speed) && !namc_fi_request(&s, NULL, &speed));
        CHECK(!namc_fi_request(&s, &i, NULL));
        CHECK(namc_fi_observe(NULL, NULL) == NAMC_FI_FAULT);
    }
    return 1;
}

static int conditional_bounds(void)
{
    namc_fi_config_t c = config(); namc_fi_state_t s;
    namc_dq_t i, expected = flux(c.target_current); double speed;
    CHECK(namc_fi_start(&c, &limits, &s) == NAMC_OK);
    while (namc_fi_request(&s, &i, &speed)) {
        namc_rs_observation_t o = sample(&s);
        double sign = s.stage == 0U ? 1.0 : -1.0;
        namc_dq_t actual = {i.d+sign*0.001, i.q-sign*0.001}, f = flux(actual);
        o.current = (namc_dq_t){actual.d+sign*c.current_uncertainty, actual.q-sign*c.current_uncertainty};
        o.electrical_speed = speed+sign*c.speed_uncertainty;
        o.terminal_voltage = (namc_dq_t){0.4009*actual.d-speed*f.q+sign*c.voltage_uncertainty,
            0.4009*actual.q+speed*f.d-sign*c.voltage_uncertainty};
        (void)namc_fi_observe(&s, &o);
    }
    CHECK(s.phase == NAMC_FI_COMPLETE);
    CHECK(fabs(s.result.flux.d-expected.d) <= s.result.uncertainty.d);
    CHECK(fabs(s.result.flux.q-expected.q) <= s.result.uncertainty.q);
    return 1;
}

int main(void) { return analytical() && guards() && quality() && invalid_config() && conditional_bounds() ? 0 : 1; }
