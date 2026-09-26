#include "namc/current_tuning.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int prepare(namc_flux_map_t *map, double cross_dq, double cross_qd)
{
    static const double axis[2] = {-10.0, 10.0};
    static double pd[4], pq[4];
    size_t d, q;
    namc_flux_data_t data = {
        1U, 1U, 1U, 2U, 2U, 4U, axis, axis, pd, pq,
        NAMC_FLUX_ANALYTICAL, "project-original-tuning-affine", 293.15, 0.0
    };
    namc_flux_limits_t limits = {1e-6, 0.001, 1e-6};
    for (d = 0U; d < 2U; ++d) {
        for (q = 0U; q < 2U; ++q) {
            pd[2U*d + q] = 0.05 + 0.003*axis[d] + cross_dq*axis[q];
            pq[2U*d + q] = 0.004*axis[q] + cross_qd*axis[d];
        }
    }
    return namc_flux_map_prepare(&data, &limits, map) == NAMC_FLUX_OK;
}

int main(void)
{
    namc_flux_map_t map;
    namc_current_config_t base = {
        1U, 0.00005, 3.0, 4.0, 600.0, 600.0, 500.0,
        0.0015, 0.002, 0.04, 12.0, 12.0, 60.0, 2000.0
    };
    namc_tuning_request_t request = {
        1U, {-2.0, 5.0}, 0.0, 48.0, 0.4, 1000.0, 1.0, 2000.0,
        0.2, 10.0, 2000.0, 1.0, 0.5
    };
    namc_tuning_candidate_t result, saved;
    unsigned int i;
    CHECK(prepare(&map, 0.0004, 0.0003));
    CHECK(namc_current_tune(&map, &base, &request, &result) == NAMC_TUNING_OK);
    CHECK(fabs(result.kp_d - 3.0) < 1e-12 && fabs(result.kp_q - 4.0) < 1e-12);
    CHECK(fabs(result.ki_d - 400.0) < 1e-12 && result.ki_q == result.ki_d);
    CHECK(result.caps == 0U && result.bandwidth == 1000.0);
    CHECK(fabs(result.jacobian.dq - 0.0004) < 1e-14);
    CHECK(fabs(result.jacobian.qd - 0.0003) < 1e-14);
    /* Independent explicit inverse of the affine test matrix. */
    CHECK(fabs(result.inverse_norm - (0.004+0.0004)/(0.003*0.004-0.0004*0.0003)) < 1e-10);
    CHECK(fabs(result.coupling_factor - (0.004*0.003+0.0004*0.004)/
          (0.003*0.004-0.0004*0.0003)) < 1e-12);
    saved = result;
    for (i = 0U; i < 5U; ++i) {
        namc_tuning_request_t capped = request;
        double expected;
        unsigned int flag;
        if (i == 0U) { capped.max_bandwidth = 500.0; expected = 500.0; flag = NAMC_TUNING_BANDWIDTH_CAP; }
        else if (i == 1U) {
            capped.max_rate_sample = 0.02;
            expected = (0.02/base.sample_time - 0.4*saved.inverse_norm)/saved.coupling_factor;
            flag = NAMC_TUNING_SAMPLE_CAP;
        } else if (i == 2U) { capped.max_kp = 2.0; expected = 500.0; flag = NAMC_TUNING_KP_CAP; }
        else if (i == 3U) { capped.max_ki = 200.0; expected = 500.0; flag = NAMC_TUNING_KI_CAP; }
        else {
            capped.voltage_fraction = 0.05;
            expected = 0.05*saved.voltage_headroom/(0.004+0.4*base.sample_time);
            flag = NAMC_TUNING_VOLTAGE_CAP;
        }
        CHECK(namc_current_tune(&map, &base, &capped, &result) == NAMC_TUNING_OK);
        CHECK(result.caps == flag && fabs(result.bandwidth - expected) < 1e-10);
        CHECK(result.rate_sample <= capped.max_rate_sample*(1.0+1e-14));
        CHECK(fmax(result.kp_d, result.kp_q) <= capped.max_kp*(1.0+1e-14));
        CHECK(result.ki_d <= capped.max_ki*(1.0+1e-14));
        CHECK((fmax(result.kp_d, result.kp_q)+base.sample_time*result.ki_d)*capped.design_error <=
              capped.voltage_fraction*result.voltage_headroom*(1.0+1e-14));
    }
    for (i = 0U; i < 17U; ++i) {
        namc_tuning_request_t bad = request;
        namc_current_config_t bad_base = base;
        memcpy(&result, &saved, sizeof(result));
        if (i == 0U) { bad.resistance = NAN; }
        else if (i == 1U) { bad.resistance = 0.0; }
        else if (i == 2U) { bad.version = 0U; }
        else if (i == 3U) { bad.requested_bandwidth = INFINITY; }
        else if (i == 4U) { bad.max_rate_sample = 0.251; }
        else if (i == 5U) { bad.voltage_fraction = 1.1; }
        else if (i == 6U) { bad.design_error = 0.0; }
        else if (i == 7U) { bad.min_bandwidth = 1500.0; }
        else if (i == 8U) { bad.bus_voltage = 1.0; }
        else if (i == 9U) { bad.electrical_speed = 2001.0; }
        else if (i == 10U) { bad.operating_current.d = 12.0; }
        else if (i == 11U) { bad.resistance = DBL_MAX; }
        else if (i == 12U) { bad.electrical_speed = 1000.0; } /* No voltage headroom. */
        else if (i == 13U) { bad.max_rate_sample = 1e-6; }
        else if (i == 14U) { bad_base.sample_time = NAN; }
        else if (i == 15U) { bad_base.model_version = 0U; }
        else { bad.operating_current.d = -10.5; bad.operating_current.q = 0.0; }
        CHECK(namc_current_tune(&map, &bad_base, &bad, &result) != NAMC_TUNING_OK);
        CHECK(memcmp(&result, &saved, sizeof(result)) == 0);
    }
    memcpy(&result, &saved, sizeof(result));
    CHECK(namc_current_tune(NULL, &base, &request, &result) == NAMC_TUNING_MODEL_REJECTED);
    CHECK(namc_current_tune(&map, NULL, &request, &result) == NAMC_TUNING_INVALID_INPUT);
    CHECK(namc_current_tune(&map, &base, NULL, &result) == NAMC_TUNING_INVALID_INPUT);
    CHECK(namc_current_tune(&map, &base, &request, NULL) == NAMC_TUNING_INVALID_INPUT);
    CHECK(memcmp(&result, &saved, sizeof(result)) == 0);
    map.prepared = 0U;
    CHECK(namc_current_tune(&map, &base, &request, &result) == NAMC_TUNING_MODEL_REJECTED);
    CHECK(memcmp(&result, &saved, sizeof(result)) == 0);
    CHECK(prepare(&map, 0.0, 0.0));
    CHECK(namc_current_tune(&map, &base, &request, &result) == NAMC_TUNING_OK);
    CHECK(fabs(result.coupling_factor - 1.0) < 1e-14);
    CHECK(result.inverse_norm < saved.inverse_norm && result.rate_sample < saved.rate_sample);
    /* Negative cross terms also participate in the rate bound. */
    CHECK(prepare(&map, -0.0004, -0.0003));
    CHECK(namc_current_tune(&map, &base, &request, &result) == NAMC_TUNING_OK);
    CHECK(fabs(result.coupling_factor - saved.coupling_factor) < 1e-12);
    puts("Bounded current tuning: analytical gains, cross terms, caps and rejection passed.");
    return 0;
}
