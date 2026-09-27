#include "namc/flux_id.h"
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
    errno = 0; *value = strtod(text, &end);
    return errno == 0 && end != text && *end == '\0' && isfinite(*value);
}

static double namc_noise(uint32_t *state, double amplitude)
{
    *state = *state*UINT32_C(1664525)+UINT32_C(1013904223);
    return amplitude*(2.0*(double)*state/4294967296.0-1.0);
}

static void namc_print_pair(namc_dq_t value)
{
    printf("[%.17g,%.17g]", value.d, value.q);
}

int main(int argc, char **argv)
{
    namc_fi_config_t c = {
        NAMC_FLUX_ID_VERSION, 0.00005, {0.0, 0.0}, {80.0, -80.0, 40.0}, 1500U, 500U, 7000U,
        0.0, 0.0, /* R estimate is mandatory; never initialized from plant R. */
        0.0001, 0.0001, 0.001, 0.001, 0.02,
        0.002, 0.002, 0.01, 0.01, 0.1, 5.0, 10.0,
        0.2, 0.0001, 0.05, 0.005, 0.001
    };
    namc_fi_limits_t limits = {8.0, 25.0, 200.0, 30.0};
    namc_fi_state_t identifier;
    namc_flux_map_t map = {0};
    namc_magnetic_parameters_t plant = {NAMC_MAGNETIC_PLANT_VERSION, &map, 0.4, 0.01, 0.001, 4U};
    namc_current_config_t controller = {
        NAMC_REFERENCE_MODEL_VERSION, 0.00005, 3.0, 4.0, 600.0, 600.0, 500.0,
        0.0015, 0.002, 0.04, 8.0, 12.0, 60.0, 200.0
    };
    namc_current_state_t pi;
    namc_magnetic_state_t state = {0.0, 0.0, 0.0, 0.0};
    namc_dq_t request, truth = {0.0, 0.0};
    double speed, vdc = 48.0, voltage_bias = 0.0, signed_error = 0.0, voltage_gain = 0.0;
    double current_noise = 0.0, voltage_noise = 0.0, actual_speed_scale = 1.0;
    double peak_current = 0.0, peak_voltage = 0.0, shaft_work = 0.0, initial_angle;
    unsigned int steps = 0U, previous_stage = 99U, k, j;
    uint32_t seed = 42U, random_state;
    int argument, voltage_source = 1, have_uncertainty = 0;
    for (argument = 1; argument < argc; argument += 2) {
        double x;
        const char *key = argv[argument];
        if (argument+1 >= argc || !namc_number(argv[argument+1], &x)) { goto bad_argument; }
        if (strcmp(key, "--resistance-estimate") == 0 && x > 0.0 && x <= 10.0) { c.resistance = x; }
        else if (strcmp(key, "--resistance-uncertainty") == 0 && x >= 0.0 && x <= 1.0) { c.resistance_uncertainty = x; have_uncertainty = 1; }
        else if (strcmp(key, "--plant-resistance") == 0 && x > 0.0 && x <= 10.0) { plant.resistance = x; }
        else if (strcmp(key, "--id") == 0 && fabs(x) <= 8.0) { c.target_current.d = x; }
        else if (strcmp(key, "--iq") == 0 && fabs(x) <= 8.0) { c.target_current.q = x; }
        else if (strcmp(key, "--speed") == 0 && x > 0.0 && x <= 200.0) { c.speeds[0] = x; c.speeds[1] = -x; c.speeds[2] = x/2.0; }
        else if (strcmp(key, "--actual-speed-scale") == 0 && x >= 0.0 && x <= 3.0) { actual_speed_scale = x; }
        else if (strcmp(key, "--vdc") == 0 && x >= 12.0 && x <= 60.0) { vdc = x; }
        else if (strcmp(key, "--dt") == 0 && x >= 1e-7 && x <= 1e-4) { c.sample_time = x; }
        else if (strcmp(key, "--seed") == 0 && x >= 0.0 && x <= 4294967295.0 && floor(x) == x) { seed = (uint32_t)x; }
        else if (strcmp(key, "--settle-samples") == 0 && x >= 1.0 && x <= 1000000.0 && floor(x) == x) { c.settle_samples = (unsigned int)x; }
        else if (strcmp(key, "--measure-samples") == 0 && x >= 2.0 && x <= 1000000.0 && floor(x) == x) { c.measure_samples = (unsigned int)x; }
        else if (strcmp(key, "--max-samples") == 0 && x >= 1.0 && x <= 1000000.0 && floor(x) == x) { c.max_samples = (unsigned int)x; }
        else if (strcmp(key, "--energy-limit") == 0 && x > 0.0 && x <= 100.0) { limits.max_apparent_energy = x; }
        else if (strcmp(key, "--current-noise") == 0 && x >= 0.0 && x <= 0.1) { current_noise = x; }
        else if (strcmp(key, "--voltage-noise") == 0 && x >= 0.0 && x <= 0.1) { voltage_noise = x; }
        else if (strcmp(key, "--voltage-bias") == 0 && fabs(x) <= 1.0) { voltage_bias = x; }
        else if (strcmp(key, "--voltage-speed-sign-error") == 0 && fabs(x) <= 1.0) { signed_error = x; }
        else if (strcmp(key, "--voltage-gain-error") == 0 && fabs(x) <= 0.1) { voltage_gain = x; }
        else if (strcmp(key, "--current-uncertainty") == 0 && x >= 0.0 && x <= 1.0) { c.current_uncertainty = x; }
        else if (strcmp(key, "--voltage-uncertainty") == 0 && x >= 0.0 && x <= 1.0) { c.voltage_uncertainty = x; }
        else if (strcmp(key, "--speed-uncertainty") == 0 && x >= 0.0 && x <= 10.0) { c.speed_uncertainty = x; }
        else if (strcmp(key, "--voltage-source") == 0 && (x == 0.0 || x == 1.0)) { voltage_source = (int)x; }
        else { goto bad_argument; }
    }
    if (!have_uncertainty || namc_fi_start(&c, &limits, &identifier) != NAMC_OK) {
        fprintf(stderr, "Flux acquisition requires a valid independent R estimate/uncertainty and policy.\n"); return 2;
    }
    if (!namc_sim_read_map(&map, 0U) || !namc_sim_map_end()) {
        fprintf(stderr, "Hidden plant map rejected.\n"); return 2;
    }
    random_state = seed;
    initial_angle = (namc_noise(&random_state, 1.0)+1.0)*3.14159265358979323846;
    controller.sample_time = c.sample_time;
    while (namc_fi_request(&identifier, &request, &speed)) {
        namc_ab_t ab, voltage;
        namc_abc_t phases;
        namc_duty_t duty;
        namc_rs_observation_t observation;
        namc_magnetic_state_t before;
        namc_dq_t midpoint_voltage, flux;
        double half_angle, sinc, torque;
        if (identifier.stage != previous_stage) {
            /* Three independent externally driven runs; no instant physical
             * reversal or shaft startup claimed. Reset plant currents and PI. */
            state = (namc_magnetic_state_t){0.0, 0.0, speed*actual_speed_scale/4.0, initial_angle};
            namc_current_reset(&pi); previous_stage = identifier.stage;
        }
        before = state;
        if (namc_inverse_park((namc_dq_t){state.id, state.iq}, state.angle, &ab) != NAMC_OK ||
            namc_inverse_clarke(ab, &phases) != NAMC_OK ||
            namc_current_step(&controller, &pi, request, phases, state.angle, 4.0*state.speed, vdc, &duty) != NAMC_OK ||
            namc_average_inverter(duty, vdc, &voltage) != NAMC_OK ||
            namc_magnetic_driven_step(&plant, &state, voltage, c.sample_time) != NAMC_FLUX_OK) {
            fprintf(stderr, "Controller/inverter/driven plant rejected at sample %u.\n", steps); return 1;
        }
        /* Interval-average terminal dq voltage: exact integration of the held
         * alpha/beta voltage in a uniformly rotating frame. Current uses the
         * measured endpoint trapezoid; residual alignment is budgeted. */
        half_angle = 4.0*state.speed*c.sample_time/2.0;
        sinc = half_angle == 0.0 ? 1.0 : sin(half_angle)/half_angle;
        if (namc_park(voltage, before.angle+half_angle, &midpoint_voltage) != NAMC_OK) { return 1; }
        observation.current.d = (before.id+state.id)/2.0+namc_noise(&random_state, current_noise);
        observation.current.q = (before.iq+state.iq)/2.0+namc_noise(&random_state, current_noise);
        observation.terminal_voltage.d = midpoint_voltage.d*sinc*(1.0+voltage_gain)+namc_noise(&random_state, voltage_noise);
        observation.terminal_voltage.q = midpoint_voltage.q*sinc*(1.0+voltage_gain)+voltage_bias+
            (speed >= 0.0 ? signed_error : -signed_error)+namc_noise(&random_state, voltage_noise);
        observation.electrical_speed = 4.0*state.speed;
        observation.sequence = ++steps;
        observation.voltage_source = (namc_rs_voltage_source_t)voltage_source;
        peak_current = fmax(peak_current, hypot(state.id, state.iq));
        peak_voltage = fmax(peak_voltage, hypot(voltage.alpha, voltage.beta));
        /* Harness-only shaft-work diagnostic, never an identifier input. */
        if (namc_flux_map_lookup(&map, (namc_dq_t){state.id, state.iq}, &flux, NULL) != NAMC_FLUX_OK ||
            namc_flux_torque(flux, (namc_dq_t){state.id, state.iq}, 4U, &torque) != NAMC_FLUX_OK) { return 1; }
        shaft_work += (torque-plant.friction*state.speed)*state.speed*c.sample_time;
        (void)namc_fi_observe(&identifier, &observation);
    }
    namc_current_reset(&pi);
    if (identifier.phase != NAMC_FI_COMPLETE && identifier.phase != NAMC_FI_DEGRADED &&
        identifier.phase != NAMC_FI_FAULT) { namc_fi_abort(&identifier); }
    /* Truth lookup only after termination, for evaluation, not estimation. */
    if (identifier.phase == NAMC_FI_COMPLETE && namc_flux_map_lookup(&map, c.target_current, &truth, NULL) != NAMC_FLUX_OK) { return 1; }
    printf("{\"schema\":\"namc-flux-point-experimental-v1\",\"evidence\":\"simulation-only\","
        "\"revision_at_configure\":\"%s\",\"compiler\":\"%s\",\"build_configuration\":\"%s\","
        "\"seed\":%u,\"initial_angle_rad\":%.17g,\"vdc_V\":%.17g,\"steps\":%u,"
        "\"shaft_model\":\"ideal-prescribed-speed-independent-runs\",\"units\":\"SI\",\"convention\":\"amplitude-invariant-dq\",",
        NAMC_BUILD_REVISION, NAMC_BUILD_COMPILER, NAMC_BUILD_CONFIGURATION, (unsigned int)seed, initial_angle, vdc, steps);
    printf("\"plant\":{\"R_ohm\":%.17g,\"J_kg_m2\":%.17g,\"B_Nm_s_rad\":%.17g,\"pole_pairs\":%u,",
        plant.resistance, plant.inertia, plant.friction, plant.pole_pairs);
    namc_sim_print_map(&map);
    printf("},\"controller\":{\"kp_d\":%.17g,\"kp_q\":%.17g,\"ki_d\":%.17g,\"ki_q\":%.17g,"
        "\"kaw\":%.17g,\"ld_prior\":%.17g,\"lq_prior\":%.17g,\"pm_flux_prior\":%.17g,"
        "\"current_limit\":%.17g,\"min_bus\":%.17g,\"max_bus\":%.17g,\"speed_limit\":%.17g},",
        controller.kp_d, controller.kp_q, controller.ki_d, controller.ki_q, controller.antiwindup_gain,
        controller.ld, controller.lq, controller.pm_flux, controller.current_limit, controller.min_bus_voltage,
        controller.max_bus_voltage, controller.electrical_speed_limit);
    printf("\"config\":{\"version\":%u,\"dt_s\":%.17g,\"target_A\":", c.version, c.sample_time);
    namc_print_pair(c.target_current);
    printf(",\"speeds_rad_s\":[%.17g,%.17g,%.17g],\"settle_samples\":%u,\"measure_samples\":%u,\"max_samples\":%u,"
        "\"R_ohm\":%.17g,\"R_uncertainty_ohm\":%.17g,\"current_uncertainty_A\":%.17g,"
        "\"voltage_uncertainty_V\":%.17g,\"speed_uncertainty_rad_s\":%.17g,\"model_voltage_error_V\":%.17g,"
        "\"gradient_bound_H\":%.17g,\"max_current_span_A\":%.17g,\"max_voltage_span_V\":%.17g,"
        "\"max_speed_span_rad_s\":%.17g,\"max_tracking_error_A\":%.17g,\"max_speed_error_rad_s\":%.17g,"
        "\"min_abs_speed_rad_s\":%.17g,\"min_speed_separation_rad_s\":%.17g,\"max_flux_Wb\":%.17g,"
        "\"max_flux_uncertainty_Wb\":%.17g,\"max_offset_voltage_V\":%.17g,\"validation_tolerance_V\":%.17g,\"max_origin_q_flux_Wb\":%.17g},",
        c.speeds[0], c.speeds[1], c.speeds[2], c.settle_samples, c.measure_samples, c.max_samples,
        c.resistance, c.resistance_uncertainty, c.current_uncertainty, c.voltage_uncertainty, c.speed_uncertainty,
        c.model_voltage_error, c.gradient_bound, c.max_current_span, c.max_voltage_span, c.max_speed_span,
        c.max_tracking_error, c.max_speed_error, c.min_abs_speed, c.min_speed_separation, c.max_flux,
        c.max_flux_uncertainty, c.max_offset_voltage, c.validation_tolerance, c.max_origin_q_flux);
    printf("\"limits\":{\"max_current_A\":%.17g,\"max_voltage_V\":%.17g,\"max_speed_rad_s\":%.17g,\"max_apparent_energy_J\":%.17g},"
        "\"observations\":{\"source\":%d,\"current_noise_A\":%.17g,\"voltage_noise_V\":%.17g,\"voltage_bias_V\":%.17g,"
        "\"voltage_speed_sign_error_V\":%.17g,\"voltage_gain_error\":%.17g,\"actual_speed_scale\":%.17g},"
        "\"state\":{\"phase\":%d,\"reason\":%d,\"stage\":%u,\"elapsed_samples\":%u,\"apparent_energy_J\":%.17g,\"excitation_enabled\":false},"
        "\"evaluation\":{\"peak_current_A\":%.17g,\"peak_voltage_V\":%.17g,\"shaft_work_J\":%.17g,\"truth_flux_Wb\":",
        limits.max_current, limits.max_voltage, limits.max_electrical_speed, limits.max_apparent_energy,
        voltage_source, current_noise, voltage_noise, voltage_bias, signed_error, voltage_gain, actual_speed_scale,
        (int)identifier.phase, (int)identifier.reason, identifier.stage, identifier.elapsed_samples, identifier.apparent_energy,
        peak_current, peak_voltage, shaft_work);
    if (identifier.phase == NAMC_FI_COMPLETE) { namc_print_pair(truth); } else { printf("null"); }
    printf("},\"windows\":[");
    for (k = 0U; k < 3U; ++k) {
        const namc_fi_window_t *w = &identifier.windows[k];
        printf("%s{\"samples\":%u,\"mean\":[", k ? "," : "", w->count);
        for (j = 0U; j < 5U; ++j) { printf("%s%.17g", j ? "," : "", w->mean[j]); }
        printf("],\"minimum\":[");
        for (j = 0U; j < 5U; ++j) { printf("%s%.17g", j ? "," : "", w->minimum[j]); }
        printf("],\"maximum\":[");
        for (j = 0U; j < 5U; ++j) { printf("%s%.17g", j ? "," : "", w->maximum[j]); }
        printf("]}");
    }
    printf("],\"accepted\":%s,\"estimate\":", identifier.phase == NAMC_FI_COMPLETE ? "true" : "false");
    if (identifier.phase == NAMC_FI_COMPLETE) {
        printf("{\"flux_Wb\":"); namc_print_pair(identifier.result.flux);
        printf(",\"uncertainty_Wb\":"); namc_print_pair(identifier.result.uncertainty);
        printf(",\"offset_V\":"); namc_print_pair(identifier.result.voltage_offset);
        printf(",\"holdout_residual_V\":"); namc_print_pair(identifier.result.holdout_residual);
        printf(",\"pm_reference\":%s}", identifier.result.pm_reference ? "true" : "false");
    } else { printf("null"); }
    printf("}\n");
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
bad_argument:
    fprintf(stderr, "Invalid flux acquisition option. Use the documented Python runner.\n"); return 2;
}
