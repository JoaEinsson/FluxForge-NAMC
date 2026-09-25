#include "namc/core.h"
#include "namc/linear_plant.h"
#include "namc/reference.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int namc_parse_number(const char *text, double *value)
{
    char *end;
    errno = 0;
    *value = strtod(text, &end);
    return errno == 0 && end != text && *end == '\0' && isfinite(*value);
}

static void namc_usage(void)
{
    fprintf(stderr, "Usage: namc_sim [--steps 2..1000000] [--seed 0..4294967295] "
        "[--dt 1e-7..1e-4] [--iq -10..10] [--vdc 12..60] [--load -2..2]\n");
}

int main(int argc, char **argv)
{
    unsigned int steps = 2000U;
    uint32_t seed = 1U, random_state;
    double dt = 0.00005, iq = 5.0, vdc = 48.0, load = 0.0;
    double initial_angle, squared_error = 0.0, tail_error = 0.0;
    double max_current = 0.0, min_duty = 1.0, max_duty = 0.0;
    unsigned int n, tail_count = 0U;
    int argument;
    /* Independent scenario truth and nominal controller priors. Never copy
     * parameters from this plant structure into the controller configuration. */
    const namc_linear_parameters_t plant = {
        NAMC_REFERENCE_MODEL_VERSION, 0.4, 0.0018, 0.0024, 0.045, 0.01, 0.001, 4U
    };
    namc_current_config_t controller = {
        NAMC_REFERENCE_MODEL_VERSION, 0.00005, 3.0, 4.0, 600.0, 600.0, 500.0,
        0.0015, 0.002, 0.04, 12.0, 12.0, 60.0, 2000.0
    };
    namc_linear_state_t state = {0.0, 0.0, 0.0, 0.0};
    namc_current_state_t control_state;
    namc_dq_t reference = {0.0, 5.0};

    for (argument = 1; argument < argc; argument += 2) {
        double value;
        const char *key = argv[argument];
        if (argument + 1 >= argc || !namc_parse_number(argv[argument + 1], &value)) {
            namc_usage();
            return 2;
        }
        if (strcmp(key, "--steps") == 0 && value >= 2.0 && value <= 1000000.0 &&
            floor(value) == value) {
            steps = (unsigned int)value;
        } else if (strcmp(key, "--seed") == 0 && value >= 0.0 &&
                   value <= 4294967295.0 && floor(value) == value) {
            seed = (uint32_t)value;
        } else if (strcmp(key, "--dt") == 0 && value >= 1e-7 && value <= 1e-4) {
            dt = value;
        } else if (strcmp(key, "--iq") == 0 && fabs(value) <= 10.0) {
            iq = value;
        } else if (strcmp(key, "--vdc") == 0 && value >= 12.0 && value <= 60.0) {
            vdc = value;
        } else if (strcmp(key, "--load") == 0 && fabs(value) <= 2.0) {
            load = value;
        } else {
            namc_usage();
            return 2;
        }
    }
    /* One defined LCG draw selects only initial electrical angle, not a motor
     * population. Unsigned arithmetic is modulo 2^32. */
    random_state = seed * UINT32_C(1664525) + UINT32_C(1013904223);
    initial_angle = ((double)random_state / 4294967296.0) * 6.2831853071795864769;
    state.angle = initial_angle;
    controller.sample_time = dt;
    reference.q = iq;
    namc_current_reset(&control_state);
    for (n = 0U; n < steps; ++n) {
        namc_dq_t current = {state.id, state.iq};
        namc_ab_t stationary_current, voltage;
        namc_abc_t measured_current;
        namc_duty_t duty;
        double error;
        /* Ideal sensor adapter. Pole-pair count 4 is an independent encoder
         * configuration, not read from hidden magnetic/mechanical truth. */
        if (namc_inverse_park(current, state.angle, &stationary_current) != NAMC_OK ||
            namc_inverse_clarke(stationary_current, &measured_current) != NAMC_OK ||
            namc_current_step(&controller, &control_state, reference, measured_current,
                state.angle, 4.0 * state.speed, vdc, &duty) != NAMC_OK ||
            namc_average_inverter(duty, vdc, &voltage) != NAMC_OK ||
            namc_linear_step(&plant, &state, voltage, load, dt) != NAMC_OK) {
            fprintf(stderr, "Simulation stopped: invalid state or limit at step %u.\n", n);
            return 1;
        }
        max_current = fmax(max_current, hypot(state.id, state.iq));
        min_duty = fmin(min_duty, fmin(duty.a, fmin(duty.b, duty.c)));
        max_duty = fmax(max_duty, fmax(duty.a, fmax(duty.b, duty.c)));
        error = state.id * state.id + (state.iq - iq) * (state.iq - iq);
        squared_error += error;
        if (n >= steps / 2U) {
            tail_error += error;
            ++tail_count;
        }
        /* The harness also checks post-integration truth. A fault on the final
         * step must not be reported as a successful experiment. */
        if (!isfinite(squared_error) || max_current > controller.current_limit ||
            fabs(4.0 * state.speed) > controller.electrical_speed_limit) {
            fprintf(stderr, "Simulation stopped: post-step limit at step %u.\n", n);
            return 1;
        }
    }
    printf("{\n  \"schema\": \"namc-linear-reference-experimental-v1\",\n"
        "  \"evidence\": \"simulation-only\",\n"
        "  \"version\": \"%s\",\n  \"revision_at_configure\": \"%s\",\n"
        "  \"compiler\": \"%s\",\n"
        "  \"build_configuration\": \"%s\",\n"
        "  \"seed\": %.0f,\n  \"steps\": %u,\n"
        "  \"dt_s\": %.17g,\n  \"iq_reference_A\": %.17g,\n"
        "  \"vdc_V\": %.17g,\n  \"load_Nm\": %.17g,\n"
        "  \"initial\": {\"id_A\": 0, \"iq_A\": 0, \"speed_rad_s\": 0, "
        "\"electrical_angle_rad\": %.17g},\n",
        namc_core_version_string(), NAMC_BUILD_REVISION, NAMC_BUILD_COMPILER,
        NAMC_BUILD_CONFIGURATION,
        (double)seed, steps, dt, iq, vdc, load, initial_angle);
    printf("  \"plant\": {\"model_version\": %u, \"R_ohm\": %.17g, "
        "\"Ld_H\": %.17g, \"Lq_H\": %.17g, \"pm_flux_Wb\": %.17g, "
        "\"J_kg_m2\": %.17g, \"B_Nm_s_rad\": %.17g, \"pole_pairs\": %u},\n",
        plant.model_version, plant.resistance, plant.ld, plant.lq,
        plant.pm_flux, plant.inertia, plant.friction, plant.pole_pairs);
    printf("  \"controller\": {\"model_version\": %u, \"dt_s\": %.17g, "
        "\"kp_d_V_A\": %.17g, \"kp_q_V_A\": %.17g, "
        "\"ki_d_V_As\": %.17g, \"ki_q_V_As\": %.17g, \"kaw_per_s\": %.17g, "
        "\"Ld_prior_H\": %.17g, \"Lq_prior_H\": %.17g, \"pm_flux_prior_Wb\": %.17g, "
        "\"current_limit_A\": %.17g, \"min_bus_V\": %.17g, \"max_bus_V\": %.17g, "
        "\"electrical_speed_limit_rad_s\": %.17g, \"encoder_pole_pairs\": 4, "
        "\"id_reference_A\": 0, \"initial_integral_d_V\": 0, "
        "\"initial_integral_q_V\": 0},\n",
        controller.model_version, controller.sample_time, controller.kp_d,
        controller.kp_q, controller.ki_d, controller.ki_q, controller.antiwindup_gain,
        controller.ld, controller.lq, controller.pm_flux, controller.current_limit,
        controller.min_bus_voltage, controller.max_bus_voltage,
        controller.electrical_speed_limit);
    printf("  \"final\": {\"id_A\": %.17g, \"iq_A\": %.17g, "
        "\"speed_rad_s\": %.17g, \"electrical_angle_rad\": %.17g},\n"
        "  \"metrics\": {\"rms_current_error_A\": %.17g, "
        "\"tail_rms_current_error_A\": %.17g, \"max_current_A\": %.17g, "
        "\"min_duty\": %.17g, \"max_duty\": %.17g}\n}\n",
        state.id, state.iq, state.speed, state.angle,
        sqrt(squared_error / (double)steps), sqrt(tail_error / (double)tail_count),
        max_current, min_duty, max_duty);
    return 0;
}
