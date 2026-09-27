#include "namc/core.h"
#include "namc/resistance_id.h"
#include "namc/magnetic_plant.h"
#include "map_transport.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int namc_number(const char *text, double *value)
{
    char *end;
    errno = 0;
    *value = strtod(text, &end);
    return errno == 0 && end != text && *end == '\0' && isfinite(*value);
}

static double namc_noise(uint32_t *state, double amplitude)
{
    *state = *state*UINT32_C(1664525)+UINT32_C(1013904223);
    return amplitude*(2.0*(double)*state/4294967296.0-1.0);
}

int main(int argc, char **argv)
{
    /* Experiment policy and independent excitation limits, not hidden truth. */
    namc_rs_config_t c = {
        NAMC_RESISTANCE_ID_VERSION, 0.00005, {1.0, 2.0, -1.0}, 1000U, 500U, 5000U,
        0.25, 0.005, 0.005, 0.05, 0.02, 0.02, 0.05, 2.0, 0.05, 0.01, 0.005,
        0.0001, 0.0001
    };
    namc_rs_limits_t limits = {3.0, 12.0, 0.5, 1.0};
    namc_rs_state_t identifier;
    namc_flux_map_t map = {0};
    namc_magnetic_parameters_t plant = {NAMC_MAGNETIC_PLANT_VERSION, &map, 0.4, 0.01, 0.001, 4U};
    /* Independent current controller; no truth resistance or magnetic map. */
    namc_current_config_t controller = {
        NAMC_REFERENCE_MODEL_VERSION, 0.00005, 3.0, 4.0, 600.0, 600.0, 500.0,
        0.0015, 0.002, 0.04, 3.0, 12.0, 60.0, 2000.0
    };
    namc_current_state_t pi;
    namc_magnetic_state_t state = {0.0, 0.0, 0.0, 0.0};
    namc_dq_t request;
    uint32_t seed = 42U, random_state;
    double initial_angle, vdc = 48.0, initial_speed = 0.0;
    double current_noise = 0.0, voltage_noise = 0.0, voltage_bias = 0.0, signed_error = 0.0;
    double peak_current = 0.0, peak_voltage = 0.0;
    unsigned int steps = 0U, k;
    int argument, voltage_source = 1;
    for (argument = 1; argument < argc; argument += 2) {
        double x;
        const char *key = argv[argument];
        if (argument+1 >= argc || !namc_number(argv[argument+1], &x)) { goto bad_argument; }
        if (strcmp(key, "--plant-resistance") == 0 && x > 0.0 && x <= 10.0) { plant.resistance = x; }
        else if (strcmp(key, "--vdc") == 0 && x >= 12.0 && x <= 60.0) { vdc = x; }
        else if (strcmp(key, "--initial-speed") == 0 && fabs(x) <= 100.0) { initial_speed = x; }
        else if (strcmp(key, "--dt") == 0 && x >= 1e-7 && x <= 1e-4) { c.sample_time = x; }
        else if (strcmp(key, "--seed") == 0 && x >= 0.0 && x <= 4294967295.0 && floor(x) == x) { seed = (uint32_t)x; }
        else if (strcmp(key, "--settle-samples") == 0 && x >= 1.0 && x <= 1000000.0 && floor(x) == x) { c.settle_samples = (unsigned int)x; }
        else if (strcmp(key, "--measure-samples") == 0 && x >= 2.0 && x <= 1000000.0 && floor(x) == x) { c.measure_samples = (unsigned int)x; }
        else if (strcmp(key, "--max-samples") == 0 && x >= 1.0 && x <= 1000000.0 && floor(x) == x) { c.max_samples = (unsigned int)x; }
        else if (strcmp(key, "--energy-limit") == 0 && x > 0.0 && x <= 10.0) { limits.max_apparent_energy = x; }
        else if (strcmp(key, "--current-noise") == 0 && x >= 0.0 && x <= 0.1) { current_noise = x; }
        else if (strcmp(key, "--voltage-noise") == 0 && x >= 0.0 && x <= 0.1) { voltage_noise = x; }
        else if (strcmp(key, "--voltage-bias") == 0 && fabs(x) <= 1.0) { voltage_bias = x; }
        else if (strcmp(key, "--voltage-signed-error") == 0 && fabs(x) <= 1.0) { signed_error = x; }
        else if (strcmp(key, "--current-uncertainty") == 0 && x >= 0.0 && x <= 1.0) { c.current_uncertainty = x; }
        else if (strcmp(key, "--voltage-uncertainty") == 0 && x >= 0.0 && x <= 1.0) { c.voltage_uncertainty = x; }
        else if (strcmp(key, "--voltage-source") == 0 && (x == 0.0 || x == 1.0)) { voltage_source = (int)x; }
        else { goto bad_argument; }
    }
    if (namc_rs_start(&c, &limits, &identifier) != NAMC_OK) {
        fprintf(stderr, "Identifier configuration rejected.\n"); return 2;
    }
    if (!namc_sim_read_map(&map, 0U) || !namc_sim_map_end()) {
        fprintf(stderr, "Hidden plant map rejected.\n"); return 2;
    }
    random_state = seed;
    initial_angle = (namc_noise(&random_state, 1.0)+1.0)*3.14159265358979323846;
    state.angle = initial_angle;
    state.speed = initial_speed;
    controller.sample_time = c.sample_time;
    namc_current_reset(&pi);
    while (namc_rs_request(&identifier, &request)) {
        namc_ab_t ab, voltage;
        namc_abc_t phases;
        namc_duty_t duty;
        namc_rs_observation_t observation;
        namc_dq_t measured = {state.id, state.iq};
        /* Ideal current/encoder adapters plus explicit estimator-only sensor
         * perturbations below. No parameter/map pointer crosses the boundary. */
        if (namc_inverse_park(measured, state.angle, &ab) != NAMC_OK ||
            namc_inverse_clarke(ab, &phases) != NAMC_OK ||
            namc_current_step(&controller, &pi, request, phases, state.angle,
                4.0*state.speed, vdc, &duty) != NAMC_OK ||
            namc_average_inverter(duty, vdc, &voltage) != NAMC_OK) {
            fprintf(stderr, "Controller/inverter rejected at step %u.\n", steps); return 1;
        }
        if (namc_magnetic_step(&plant, &state, voltage, 0.0, c.sample_time) != NAMC_FLUX_OK) {
            fprintf(stderr, "Hidden plant rejected at step %u.\n", steps); return 1;
        }
        ++steps;
        /* Voltage from the actual average-inverter output over this interval,
         * not PI's pre-limit request; aligned to the measured rotor frame. */
        if (namc_park(voltage, state.angle, &observation.terminal_voltage) != NAMC_OK) {
            fprintf(stderr, "Voltage adapter rejected.\n"); return 1;
        }
        observation.current.d = state.id+namc_noise(&random_state, current_noise);
        observation.current.q = state.iq;
        observation.terminal_voltage.d += voltage_bias +
            (state.id >= 0.0 ? signed_error : -signed_error) + namc_noise(&random_state, voltage_noise);
        observation.sequence = steps;
        observation.electrical_speed = 4.0*state.speed;
        observation.voltage_source = (namc_rs_voltage_source_t)voltage_source;
        peak_current = fmax(peak_current, hypot(state.id, state.iq));
        peak_voltage = fmax(peak_voltage, hypot(voltage.alpha, voltage.beta));
        (void)namc_rs_observe(&identifier, &observation);
    }
    /* An inactive request means actuator disable, not a zero-current PI command.
     * No post-disable discharge/physical shutdown transient is simulated here. */
    namc_current_reset(&pi);
    if (identifier.phase != NAMC_RS_COMPLETE && identifier.phase != NAMC_RS_DEGRADED &&
        identifier.phase != NAMC_RS_FAULT) { namc_rs_abort(&identifier); }
    printf("{\"schema\":\"namc-resistance-identification-experimental-v1\","
        "\"evidence\":\"simulation-only\",\"version\":\"%s\","
        "\"revision_at_configure\":\"%s\",\"compiler\":\"%s\",\"build_configuration\":\"%s\","
        "\"seed\":%u,\"steps\":%u,\"initial_angle_rad\":%.17g,\"initial_speed_rad_s\":%.17g,\"vdc_V\":%.17g,"
        "\"observations\":{\"voltage_source\":%d,\"current_noise_A\":%.17g,\"voltage_noise_V\":%.17g,"
        "\"voltage_bias_V\":%.17g,\"voltage_signed_error_V\":%.17g},",
        namc_core_version_string(), NAMC_BUILD_REVISION, NAMC_BUILD_COMPILER, NAMC_BUILD_CONFIGURATION,
        (unsigned int)seed, steps, initial_angle, initial_speed, vdc,
        voltage_source, current_noise, voltage_noise, voltage_bias, signed_error);
    printf("\"plant\":{\"R_ohm\":%.17g,\"J_kg_m2\":%.17g,\"B_Nm_s_rad\":%.17g,\"pole_pairs\":%u,",
        plant.resistance, plant.inertia, plant.friction, plant.pole_pairs);
    namc_sim_print_map(&map);
    printf("},\"controller\":{\"kp_d\":%.17g,\"kp_q\":%.17g,\"ki_d\":%.17g,\"ki_q\":%.17g,"
        "\"kaw\":%.17g,\"ld_prior\":%.17g,\"lq_prior\":%.17g,\"pm_flux_prior\":%.17g,"
        "\"current_limit\":%.17g,\"min_bus\":%.17g,\"max_bus\":%.17g,\"speed_limit\":%.17g},",
        controller.kp_d, controller.kp_q, controller.ki_d, controller.ki_q, controller.antiwindup_gain,
        controller.ld, controller.lq, controller.pm_flux, controller.current_limit,
        controller.min_bus_voltage, controller.max_bus_voltage, controller.electrical_speed_limit);
    printf("\"config\":{\"version\":%u,\"dt_s\":%.17g,\"levels_A\":[%.17g,%.17g,%.17g],"
        "\"settle_samples\":%u,\"measure_samples\":%u,\"max_samples\":%u,"
        "\"min_current_separation_A\":%.17g,\"max_current_span_A\":%.17g,\"max_voltage_span_V\":%.17g,"
        "\"max_tracking_error_A\":%.17g,\"max_quadrature_current_A\":%.17g,\"max_quadrature_voltage_V\":%.17g,"
        "\"min_resistance_ohm\":%.17g,\"max_resistance_ohm\":%.17g,\"max_offset_voltage_V\":%.17g,"
        "\"max_resistance_uncertainty_ohm\":%.17g,\"validation_voltage_tolerance_V\":%.17g,"
        "\"current_uncertainty_A\":%.17g,\"voltage_uncertainty_V\":%.17g},",
        c.version, c.sample_time, c.levels[0], c.levels[1], c.levels[2], c.settle_samples, c.measure_samples,
        c.max_samples, c.min_current_separation, c.max_current_span, c.max_voltage_span,
        c.max_tracking_error, c.max_quadrature_current, c.max_quadrature_voltage,
        c.min_resistance, c.max_resistance, c.max_offset_voltage, c.max_resistance_uncertainty,
        c.validation_voltage_tolerance, c.current_uncertainty, c.voltage_uncertainty);
    printf("\"limits\":{\"max_current_A\":%.17g,\"max_voltage_V\":%.17g,\"max_electrical_speed_rad_s\":%.17g,"
        "\"max_apparent_energy_J\":%.17g},\"state\":{\"phase\":%d,\"reason\":%d,\"stage\":%u,"
        "\"elapsed_samples\":%u,\"apparent_energy_J\":%.17g,\"excitation_enabled\":false},"
        "\"metrics\":{\"peak_current_A\":%.17g,\"peak_voltage_V\":%.17g},\"windows\":[",
        limits.max_current, limits.max_voltage, limits.max_electrical_speed, limits.max_apparent_energy,
        (int)identifier.phase, (int)identifier.reason, identifier.stage, identifier.elapsed_samples,
        identifier.apparent_energy, peak_current, peak_voltage);
    for (k = 0U; k < 3U; ++k) {
        const namc_rs_window_t *w = &identifier.windows[k];
        printf("%s{\"samples\":%u,\"id_mean_A\":%.17g,\"vd_mean_V\":%.17g,"
            "\"current_span_A\":%.17g,\"voltage_span_V\":%.17g,"
            "\"min_id_A\":%.17g,\"max_id_A\":%.17g,\"min_vd_V\":%.17g,\"max_vd_V\":%.17g}", k ? "," : "",
            w->count, w->mean_current, w->mean_voltage, w->max_current-w->min_current, w->max_voltage-w->min_voltage,
            w->min_current, w->max_current, w->min_voltage, w->max_voltage);
    }
    printf("],\"accepted\":%s,\"estimate\":", identifier.phase == NAMC_RS_COMPLETE ? "true" : "false");
    if (identifier.phase == NAMC_RS_COMPLETE) {
        printf("{\"R_ohm\":%.17g,\"lower_ohm\":%.17g,\"upper_ohm\":%.17g,"
            "\"offset_V\":%.17g,\"validation_residual_V\":%.17g,\"truth_error_ohm\":%.17g}",
            identifier.result.resistance, identifier.result.lower, identifier.result.upper,
            identifier.result.offset_voltage, identifier.result.validation_residual,
            identifier.result.resistance-plant.resistance);
    } else { printf("null"); }
    printf("}\n");
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;

bad_argument:
    fprintf(stderr, "Invalid identification experiment option. Use the documented Python runner.\n");
    return 2;
}
