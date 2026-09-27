#include "namc/resistance_id.h"

#include <math.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Line %d: %s\n", __LINE__, #x); return 0; } } while (0)

static namc_rs_config_t config(void)
{
    const namc_rs_config_t c = {
        NAMC_RESISTANCE_ID_VERSION, 0.00005, {1.0, 2.0, -1.0}, 1U, 2U, 20U,
        0.25, 0.005, 0.005, 0.05, 0.02, 0.02, 0.05, 2.0, 0.05, 0.01, 0.005,
        0.0001, 0.0001
    };
    return c;
}

static const namc_rs_limits_t limits = {3.0, 12.0, 0.5, 1.0};

static namc_rs_observation_t sample(const namc_rs_state_t *s)
{
    namc_rs_observation_t o = {0};
    o.sequence = s->elapsed_samples+1U;
    o.current.d = s->config.levels[s->stage];
    o.terminal_voltage.d = 0.4*o.current.d+0.02;
    o.voltage_source = NAMC_RS_IDEAL_TERMINAL;
    return o;
}

static int analytical_and_terminal(void)
{
    namc_rs_state_t s;
    namc_rs_config_t c = config();
    namc_dq_t request;
    unsigned int n = 0U;
    CHECK(namc_rs_start(&c, &limits, &s) == NAMC_OK);
    c.levels[0] = 999.0; /* Start owns a copy; caller changes cannot expand excitation. */
    while (namc_rs_request(&s, &request)) {
        namc_rs_observation_t o = sample(&s);
        CHECK(request.q == 0.0 && request.d == s.config.levels[s.stage]);
        CHECK(s.result.resistance == 0.0);
        (void)namc_rs_observe(&s, &o);
        CHECK(++n <= 9U);
    }
    CHECK(n == 9U && s.phase == NAMC_RS_COMPLETE && s.reason == NAMC_RS_NONE);
    CHECK(fabs(s.result.resistance-0.4) < 1e-14);
    CHECK(fabs(s.result.offset_voltage-0.02) < 1e-14);
    CHECK(fabs(s.result.validation_residual) < 1e-14);
    CHECK(fabs(s.result.lower-(0.4-0.0002)/(1.0+0.0002)) < 1e-14);
    CHECK(fabs(s.result.upper-(0.4+0.0002)/(1.0-0.0002)) < 1e-14);
    CHECK(request.d == 0.0 && request.q == 0.0);
    CHECK(namc_rs_observe(&s, NULL) == NAMC_RS_COMPLETE);
    CHECK(s.result.resistance != 0.0);
    namc_rs_abort(&s);
    CHECK(s.phase == NAMC_RS_FAULT && s.reason == NAMC_RS_ABORTED && s.result.resistance == 0.0);
    CHECK(!namc_rs_request(&s, &request));
    c = config();
    CHECK(namc_rs_start(&c, &limits, &s) == NAMC_OK);
    CHECK(s.phase == NAMC_RS_SETTLING && s.elapsed_samples == 0U && s.apparent_energy == 0.0);
    return 1;
}

static int guards(void)
{
    unsigned int trial;
    for (trial = 0U; trial < 13U; ++trial) {
        namc_rs_config_t c = config();
        namc_rs_state_t s;
        namc_rs_observation_t o;
        namc_dq_t request;
        namc_rs_limits_t l = limits;
        namc_rs_reason_t reason = NAMC_RS_BAD_OBSERVATION;
        if (trial == 10U) { l.max_apparent_energy = 1e-8; }
        CHECK(namc_rs_start(&c, &l, &s) == NAMC_OK);
        o = sample(&s);
        if (trial == 0U) { o.current.d = NAN; }
        if (trial == 1U) { o.terminal_voltage.q = INFINITY; }
        if (trial == 2U) { o.electrical_speed = NAN; }
        if (trial == 3U) { o.sequence = 2U; }
        if (trial == 4U) { o.current.d = 3.01; reason = NAMC_RS_LIMIT; }
        if (trial == 5U) { o.terminal_voltage.d = 12.01; reason = NAMC_RS_LIMIT; }
        if (trial == 6U) { o.electrical_speed = 0.51; reason = NAMC_RS_LIMIT; }
        if (trial == 7U) { o.voltage_source = NAMC_RS_COMMAND_ONLY; reason = NAMC_RS_UNTRUSTED_VOLTAGE; }
        if (trial == 8U) { o.voltage_source = (namc_rs_voltage_source_t)99; reason = NAMC_RS_UNTRUSTED_VOLTAGE; }
        if (trial == 9U) { s.apparent_energy = NAN; }
        if (trial == 10U) { reason = NAMC_RS_LIMIT; }
        if (trial == 11U) { s.version = 0U; reason = NAMC_RS_BAD_CONFIG; }
        if (trial == 12U) {
            (void)namc_rs_observe(&s, &o);
            o = sample(&s); o.voltage_source = NAMC_RS_MEASURED_TERMINAL;
            reason = NAMC_RS_UNTRUSTED_VOLTAGE;
        }
        (void)namc_rs_observe(&s, &o);
        CHECK(s.reason == reason && (s.phase == NAMC_RS_FAULT || s.phase == NAMC_RS_DEGRADED));
        CHECK(!namc_rs_request(&s, &request) && request.d == 0.0 && request.q == 0.0);
        CHECK(s.result.resistance == 0.0);
        (void)namc_rs_observe(&s, &o);
        CHECK(s.reason == reason);
    }
    return 1;
}

static int quality_rejections(void)
{
    unsigned int trial;
    for (trial = 0U; trial < 9U; ++trial) {
        namc_rs_config_t c = config();
        namc_rs_state_t s;
        namc_dq_t request;
        namc_rs_reason_t expected = NAMC_RS_NONE;
        unsigned int n = 0U;
        if (trial == 2U) { c.max_tracking_error = 5.0; }
        if (trial == 5U) { c.voltage_uncertainty = 0.02; }
        if (trial == 7U) { c.max_samples = 4U; }
        CHECK(namc_rs_start(&c, &limits, &s) == NAMC_OK);
        while (namc_rs_request(&s, &request)) {
            namc_rs_observation_t o = sample(&s);
            if (trial == 0U) { o.current.q = 0.03; expected = NAMC_RS_TRACKING; }
            if (trial == 1U) { o.current.d += (n % 2U) ? 0.004 : -0.004; expected = NAMC_RS_UNSETTLED; }
            if (trial == 2U) { o.current.d = 1.0; o.terminal_voltage.d = 0.4; expected = NAMC_RS_NO_EXCITATION; }
            if (trial == 3U) { o.terminal_voltage.d = 2.5*o.current.d; expected = NAMC_RS_RANGE; }
            if (trial == 4U) { o.terminal_voltage.d += 0.1; expected = NAMC_RS_OFFSET; }
            if (trial == 5U) { expected = NAMC_RS_UNCERTAINTY; }
            if (trial == 6U) { if (s.stage == 2U) { o.terminal_voltage.d += 0.03; } expected = NAMC_RS_VALIDATION; }
            if (trial == 7U) { expected = NAMC_RS_TIMEOUT; }
            if (trial == 8U) { o.terminal_voltage.q = 0.03; expected = NAMC_RS_TRACKING; }
            (void)namc_rs_observe(&s, &o);
            CHECK(++n <= 9U);
        }
        CHECK(s.phase == NAMC_RS_DEGRADED && s.reason == expected && s.result.resistance == 0.0);
    }
    return 1;
}

static int invalid_configuration(void)
{
    unsigned int trial;
    for (trial = 0U; trial < 14U; ++trial) {
        namc_rs_config_t c = config();
        namc_rs_limits_t l = limits;
        namc_rs_state_t s;
        if (trial == 0U) { c.version = 99U; }
        if (trial == 1U) { c.sample_time = 0.0; }
        if (trial == 2U) { c.levels[1] = c.levels[0]; }
        if (trial == 3U) { c.levels[2] = 1.0; }
        if (trial == 4U) { c.levels[0] = NAN; }
        if (trial == 5U) { c.measure_samples = 1U; }
        if (trial == 6U) { c.max_samples = NAMC_RESISTANCE_MAX_SAMPLES+1U; }
        if (trial == 7U) { c.min_resistance = 3.0; }
        if (trial == 8U) { c.current_uncertainty = -1.0; }
        if (trial == 9U) { c.voltage_uncertainty = INFINITY; }
        if (trial == 10U) { l.max_current = 1.0; }
        if (trial == 11U) { l.max_voltage = NAN; }
        if (trial == 12U) { l.max_apparent_energy = 0.0; }
        if (trial == 13U) { c.max_voltage_span = 0.0; }
        CHECK(namc_rs_start(&c, &l, &s) == NAMC_INVALID_INPUT);
        CHECK(s.phase == NAMC_RS_FAULT && s.reason == NAMC_RS_BAD_CONFIG);
    }
    {
        namc_rs_state_t s;
        namc_rs_config_t c = config();
        namc_dq_t request;
        CHECK(namc_rs_start(NULL, &limits, &s) == NAMC_INVALID_INPUT);
        CHECK(namc_rs_start(&c, NULL, &s) == NAMC_INVALID_INPUT);
        CHECK(namc_rs_start(&c, &limits, NULL) == NAMC_INVALID_INPUT);
        CHECK(!namc_rs_request(NULL, &request) && !namc_rs_request(&s, NULL));
        CHECK(namc_rs_observe(NULL, NULL) == NAMC_RS_FAULT);
    }
    return 1;
}

int main(void)
{
    return analytical_and_terminal() && guards() && quality_rejections() && invalid_configuration() ? 0 : 1;
}
