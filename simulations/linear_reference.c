#include "namc/core.h"
#include "namc/linear_plant.h"
#include "namc/magnetic_plant.h"
#include "namc/reference.h"
#include "map_transport.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAMC_TRACE_CAPACITY 10000U
typedef struct namc_sim_sample {
    unsigned int step;
    namc_linear_state_t state;
    namc_duty_t duty;
    namc_ab_t voltage;
    double torque;
} namc_sim_sample_t;

/* Host-only bounded diagnostic storage, not part of the controller. */
static namc_sim_sample_t namc_trace[NAMC_TRACE_CAPACITY];

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
        "[--dt 1e-7..1e-4] [--id -10..10] [--iq -10..10] [--vdc 12..60] "
        "[--load -2..2] [--plant linear|lut] [--trace-stride 0..1000000]\n"
        "lut reads the private map transport from stdin; use the Python JSON runner.\n");
}

int main(int argc, char **argv)
{
    unsigned int steps = 2000U;
    uint32_t seed = 1U, random_state;
    double dt = 0.00005, id = 0.0, iq = 5.0, vdc = 48.0, load = 0.0;
    double initial_angle, squared_error = 0.0, tail_error = 0.0;
    double max_current = 0.0, min_duty = 1.0, max_duty = 0.0;
    unsigned int n, tail_count = 0U;
    unsigned int trace_stride = 0U, trace_count = 0U;
    double torque = 0.0, max_abs_torque = 0.0;
    int use_lut = 0;
    namc_flux_map_t map = {0};
    int argument;
    /* Independent scenario truth and nominal controller priors. Never copy
     * parameters from this plant structure into the controller configuration. */
    const namc_linear_parameters_t plant = {
        NAMC_REFERENCE_MODEL_VERSION, 0.4, 0.0018, 0.0024, 0.045, 0.01, 0.001, 4U
    };
    const namc_magnetic_parameters_t magnetic = {
        NAMC_MAGNETIC_PLANT_VERSION, &map, 0.4, 0.01, 0.001, 4U
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
        if (strcmp(key, "--plant") == 0 && argument + 1 < argc) {
            if (strcmp(argv[argument + 1], "linear") == 0) {
                use_lut = 0;
            } else if (strcmp(argv[argument + 1], "lut") == 0) {
                use_lut = 1;
            } else {
                namc_usage();
                return 2;
            }
            continue;
        }
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
        } else if (strcmp(key, "--id") == 0 && fabs(value) <= 10.0) {
            id = value;
        } else if (strcmp(key, "--trace-stride") == 0 && value >= 0.0 &&
                   value <= 1000000.0 && floor(value) == value) {
            trace_stride = (unsigned int)value;
        } else if (strcmp(key, "--vdc") == 0 && value >= 12.0 && value <= 60.0) {
            vdc = value;
        } else if (strcmp(key, "--load") == 0 && fabs(value) <= 2.0) {
            load = value;
        } else {
            namc_usage();
            return 2;
        }
    }
    if (hypot(id, iq) > controller.current_limit ||
        (trace_stride != 0U && (steps - 1U) / trace_stride + 1U > NAMC_TRACE_CAPACITY)) {
        fprintf(stderr, "Configuration rejected: reference limit or trace capacity.\n");
        return 2;
    }
    reference.d = id;
    reference.q = iq;
    if (use_lut) {
        namc_dq_t flux, zero = {0.0, 0.0};
        if (!namc_sim_read_map(&map)) {
            fprintf(stderr, "Map transport rejected.\n");
            return 2;
        }
        if (namc_flux_map_lookup(&map, zero, &flux, NULL) != NAMC_FLUX_OK ||
            namc_flux_map_lookup(&map, reference, &flux, NULL) != NAMC_FLUX_OK) {
            fprintf(stderr, "Map configuration rejected: initial/reference current is unsupported.\n");
            return 2;
        }
    }
    /* One defined LCG draw selects only initial electrical angle, not a motor
     * population. Unsigned arithmetic is modulo 2^32. */
    random_state = seed * UINT32_C(1664525) + UINT32_C(1013904223);
    initial_angle = ((double)random_state / 4294967296.0) * 6.2831853071795864769;
    state.angle = initial_angle;
    controller.sample_time = dt;
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
            namc_average_inverter(duty, vdc, &voltage) != NAMC_OK) {
            fprintf(stderr, "Simulation stopped: controller/sensor/inverter at step %u.\n", n);
            return 1;
        }
        if (use_lut) {
            namc_flux_result_t status = namc_magnetic_step(&magnetic, &state, voltage, load, dt);
            if (status != NAMC_FLUX_OK) {
                fprintf(stderr, "Simulation stopped: magnetic plant at step %u, flux status %d.\n",
                    n, (int)status);
                return 1;
            }
        } else if (namc_linear_step(&plant, &state, voltage, load, dt) != NAMC_OK) {
            fprintf(stderr, "Simulation stopped: linear plant at step %u.\n", n);
            return 1;
        }
        {
            namc_dq_t flux, post_current = {state.id, state.iq};
            int valid;
            if (use_lut) {
                valid = namc_flux_map_lookup(&map, post_current, &flux, NULL) == NAMC_FLUX_OK &&
                    namc_flux_torque(flux, post_current, magnetic.pole_pairs, &torque) == NAMC_FLUX_OK;
            } else {
                valid = namc_linear_flux_torque(&plant, post_current, &flux, &torque) == NAMC_OK;
            }
            if (!valid) {
                fprintf(stderr, "Simulation stopped: torque diagnostic at step %u.\n", n);
                return 1;
            }
        }
        max_abs_torque = fmax(max_abs_torque, fabs(torque));
        max_current = fmax(max_current, hypot(state.id, state.iq));
        min_duty = fmin(min_duty, fmin(duty.a, fmin(duty.b, duty.c)));
        max_duty = fmax(max_duty, fmax(duty.a, fmax(duty.b, duty.c)));
        error = (state.id - id) * (state.id - id) + (state.iq - iq) * (state.iq - iq);
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
        if (trace_stride != 0U && ((n + 1U) % trace_stride == 0U || n + 1U == steps)) {
            namc_sim_sample_t *sample = &namc_trace[trace_count++];
            sample->step = n + 1U;
            sample->state = state;
            sample->duty = duty;
            sample->voltage = voltage;
            sample->torque = torque;
        }
    }
    printf("{\n  \"schema\": \"%s\",\n"
        "  \"plant_backend\": \"%s\",\n"
        "  \"evidence\": \"simulation-only\",\n"
        "  \"version\": \"%s\",\n  \"revision_at_configure\": \"%s\",\n"
        "  \"compiler\": \"%s\",\n"
        "  \"build_configuration\": \"%s\",\n"
        "  \"seed\": %.0f,\n  \"steps\": %u,\n"
        "  \"dt_s\": %.17g,\n  \"iq_reference_A\": %.17g,\n"
        "  \"id_reference_A\": %.17g,\n  \"trace_stride\": %u,\n"
        "  \"vdc_V\": %.17g,\n  \"load_Nm\": %.17g,\n"
        "  \"initial\": {\"id_A\": 0, \"iq_A\": 0, \"speed_rad_s\": 0, "
        "\"electrical_angle_rad\": %.17g},\n",
        use_lut ? "namc-flux-map-reference-experimental-v1" : "namc-linear-reference-experimental-v1",
        use_lut ? "coupled-flux-lut" : "linear",
        namc_core_version_string(), NAMC_BUILD_REVISION, NAMC_BUILD_COMPILER,
        NAMC_BUILD_CONFIGURATION,
        (double)seed, steps, dt, iq, id, trace_stride, vdc, load, initial_angle);
    if (use_lut) {
        printf("  \"plant\": {\"model_version\": %u, \"R_ohm\": %.17g, "
            "\"J_kg_m2\": %.17g, \"B_Nm_s_rad\": %.17g, \"pole_pairs\": %u,\n",
            magnetic.version, magnetic.resistance, magnetic.inertia, magnetic.friction,
            magnetic.pole_pairs);
        namc_sim_print_map(&map);
        printf("},\n");
    } else {
        printf("  \"plant\": {\"model_version\": %u, \"R_ohm\": %.17g, "
            "\"Ld_H\": %.17g, \"Lq_H\": %.17g, \"pm_flux_Wb\": %.17g, "
            "\"J_kg_m2\": %.17g, \"B_Nm_s_rad\": %.17g, \"pole_pairs\": %u},\n",
            plant.model_version, plant.resistance, plant.ld, plant.lq,
            plant.pm_flux, plant.inertia, plant.friction, plant.pole_pairs);
    }
    printf("  \"controller\": {\"model_version\": %u, \"dt_s\": %.17g, "
        "\"kp_d_V_A\": %.17g, \"kp_q_V_A\": %.17g, "
        "\"ki_d_V_As\": %.17g, \"ki_q_V_As\": %.17g, \"kaw_per_s\": %.17g, "
        "\"Ld_prior_H\": %.17g, \"Lq_prior_H\": %.17g, \"pm_flux_prior_Wb\": %.17g, "
        "\"current_limit_A\": %.17g, \"min_bus_V\": %.17g, \"max_bus_V\": %.17g, "
        "\"electrical_speed_limit_rad_s\": %.17g, \"encoder_pole_pairs\": 4, "
        "\"id_reference_A\": %.17g, \"initial_integral_d_V\": 0, "
        "\"initial_integral_q_V\": 0},\n",
        controller.model_version, controller.sample_time, controller.kp_d,
        controller.kp_q, controller.ki_d, controller.ki_q, controller.antiwindup_gain,
        controller.ld, controller.lq, controller.pm_flux, controller.current_limit,
        controller.min_bus_voltage, controller.max_bus_voltage,
        controller.electrical_speed_limit, id);
    printf("  \"final\": {\"id_A\": %.17g, \"iq_A\": %.17g, "
        "\"speed_rad_s\": %.17g, \"electrical_angle_rad\": %.17g, \"torque_Nm\": %.17g},\n"
        "  \"metrics\": {\"rms_current_error_A\": %.17g, "
        "\"tail_rms_current_error_A\": %.17g, \"max_current_A\": %.17g, "
        "\"min_duty\": %.17g, \"max_duty\": %.17g, \"max_abs_torque_Nm\": %.17g},\n",
        state.id, state.iq, state.speed, state.angle, torque,
        sqrt(squared_error / (double)steps), sqrt(tail_error / (double)tail_count),
        max_current, min_duty, max_duty, max_abs_torque);
    printf("  \"trace\": [");
    for (n = 0U; n < trace_count; ++n) {
        const namc_sim_sample_t *s = &namc_trace[n];
        printf("%s{\"step\": %u, \"time_s\": %.17g, \"id_A\": %.17g, "
            "\"iq_A\": %.17g, \"speed_rad_s\": %.17g, \"electrical_angle_rad\": %.17g, "
            "\"torque_Nm\": %.17g, \"duty_a\": %.17g, \"duty_b\": %.17g, "
            "\"duty_c\": %.17g, \"voltage_alpha_V\": %.17g, \"voltage_beta_V\": %.17g}",
            n == 0U ? "" : ",\n", s->step, (double)s->step * dt, s->state.id,
            s->state.iq, s->state.speed, s->state.angle, s->torque,
            s->duty.a, s->duty.b, s->duty.c, s->voltage.alpha, s->voltage.beta);
    }
    printf("]\n}\n");
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}
