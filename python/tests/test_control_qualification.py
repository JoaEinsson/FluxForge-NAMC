"""Native fallback transitions, independent metric oracles and finite campaign."""

import math
import json
import os
import subprocess
import sys

import pytest

from fluxforge_namc import CurrentTuning, FluxMap, FluxMapLimits
from fluxforge_namc.qualification import assess, cases, run_campaign
from test_model_control import FIXTURE, model, run


REQUEST = CurrentTuning(1500, 0.4, electrical_speed=80)


@pytest.mark.parametrize("fault", [0, 200, 1000, 1999])
def test_tuned_fallback_is_latched_with_separate_reserve(fault):
    report = run(controller=model(), tuning=REQUEST, model_failure="nominal",
                 model_fault_step=fault, initial_speed=20, id=-2, load=0.2, trace_stride=1)
    reserve = {"kp_d_V_A": 3, "kp_q_V_A": 4, "ki_d_V_As": 600, "ki_q_V_As": 600}
    assert report["fallback_gains"] == reserve
    assert report["controller"]["kp_d_V_A"] != reserve["kp_d_V_A"]
    assert report["model_diagnostics"]["first_fallback_step_zero_based"] == fault
    assert report["model_diagnostics"]["flux_status"] != 0
    assert report["metrics"]["fallback_steps"] == report["steps"]-fault
    assert all(s["controller_mode"] == (1 if s["step"] <= fault else 2) for s in report["trace"])
    assert 0 <= report["metrics"]["min_duty"] <= report["metrics"]["max_duty"] <= 1
    assert report["metrics"]["max_current_A"] <= 12
    if fault == 0:
        baseline = run(initial_speed=20, id=-2, load=0.2, trace_stride=1)
        assert report["final"] == baseline["final"]
        for actual, expected in zip(report["trace"], baseline["trace"]):
            assert {k: v for k, v in actual.items() if k != "controller_mode"} == {
                k: v for k, v in expected.items() if k != "controller_mode"}
    else:
        before, after = report["trace"][fault-1:fault+1]
        expected_jump = math.hypot(after["voltage_alpha_V"]-before["voltage_alpha_V"],
                                   after["voltage_beta_V"]-before["voltage_beta_V"])
        assert report["metrics"]["fallback_transition_voltage_jump_V"] == pytest.approx(expected_jump)


def test_model_loss_default_policy_disables_without_success_report():
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(controller=model(), tuning=REQUEST, model_fault_step=200)
    assert failure.value.returncode == 1 and not failure.value.stdout
    assert "at step 200, status 4" in failure.value.stderr


def test_new_scenario_fields_replay_from_report():
    report = run(controller=model(), tuning=REQUEST, model_failure="nominal",
                 model_fault_step=333, initial_speed=-20, id=-2, load=0.2, trace_stride=23, seed=7)
    restore = lambda value: FluxMap(os.environ["NAMC_CORE_LIBRARY"], value["map"],
                                    FluxMapLimits(**value["map_limits"]))
    replay = run(restore(report["plant"]), restore(report["controller_model"]),
                 tuning=CurrentTuning(**report["tuning"]["request"]),
                 model_failure=report["controller"]["model_failure_policy"],
                 model_fault_step=report["model_fault_step"], initial_speed=report["initial"]["speed_rad_s"],
                 id=report["id_reference_A"], iq=report["iq_reference_A"],
                 load=report["load_Nm"], vdc=report["vdc_V"], dt=report["dt_s"],
                 steps=report["steps"], seed=report["seed"], trace_stride=report["trace_stride"])
    assert replay == report


def test_cli_tuned_fallback_comparison_and_timing():
    command = [sys.executable, "-m", "fluxforge_namc", "--executable", os.environ["NAMC_SIM_EXECUTABLE"],
               "--library", os.environ["NAMC_CORE_LIBRARY"], "--map", str(FIXTURE),
               "--controller-map", str(FIXTURE), "--min-incremental-h", "1e-5",
               "--max-reciprocity-error-h", "1e-5", "--min-rcond", "1e-6",
               "--control-min-incremental-h", "1e-5", "--control-max-reciprocity-error-h", "1e-5",
               "--control-min-rcond", "1e-6", "--compare-controllers", "--initial-speed", "20",
               "--tune-bandwidth", "1500", "--tune-resistance", "0.4", "--tune-speed", "80",
               "--model-failure", "nominal", "--model-fault-step", "200", "--timing"]
    result = subprocess.run(command, capture_output=True, text=True, check=True, timeout=30)
    report = json.loads(result.stdout)
    assert report["nominal"]["model_fault_step"] == -1
    for mode in ("nominal", "model_aware", "tuned_model_aware"):
        arm = report[mode]
        assert arm["initial"] == report["nominal"]["initial"]
        assert arm["plant"] == report["nominal"]["plant"] and "timing" in arm
        assert arm["metrics"]["fallback_steps"] == (0 if mode == "nominal" else 1800)


@pytest.mark.parametrize("arguments", [
    {"initial_speed": float("nan")}, {"initial_speed": 501},
    {"model_fault_step": 2000}, {"model_fault_step": -2}, {"model_fault_step": 1.5},
])
def test_bad_experiment_parameters_rejected(arguments):
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(controller=model(), **arguments)
    assert failure.value.returncode == 2 and not failure.value.stdout


def test_fault_injection_requires_explicit_controller_map():
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(model_fault_step=0)
    assert failure.value.returncode == 2 and not failure.value.stdout


@pytest.mark.parametrize("id,iq,steps", [(-2, 5, 2000), (2, -5, 2000), (0, 0, 2000), (-2, 5, 2)])
def test_transient_metrics_match_independent_dense_trace(id, iq, steps):
    report = run(controller=model(), id=id, iq=iq, steps=steps, trace_stride=1)
    trace, m = report["trace"], report["metrics"]
    errors = [math.hypot(id, iq)] + [math.hypot(s["id_A"]-id, s["iq_A"]-iq) for s in trace]
    threshold = max(0.02, 0.02*math.hypot(id, iq))
    outside = [i for i, e in enumerate(errors) if e > threshold]
    expected_time = 0 if not outside else (-1 if outside[-1] == steps else (outside[-1]+1)*report["dt_s"])
    assert m["peak_current_error_A"] == pytest.approx(max(errors))
    assert m["settling_band_A"] == pytest.approx(threshold)
    assert m["settling_time_s"] == pytest.approx(expected_time)
    for axis, target in (("d", id), ("q", iq)):
        values = [s[f"i{axis}_A"] for s in trace]
        expected = max(abs(v) if target == 0 else max(0, (v-target)*(1 if target > 0 else -1)) for v in values)
        assert m[f"overshoot_{axis}_A"] == pytest.approx(expected)
    decimated = run(controller=model(), id=id, iq=iq, steps=steps, trace_stride=17)
    assert decimated["metrics"] == m


def test_timing_is_opt_in_and_cannot_change_physics_or_replay():
    options = dict(controller=model(), tuning=REQUEST, model_failure="nominal",
                   model_fault_step=200, initial_speed=20, id=-2, load=0.2, trace_stride=17)
    deterministic = run(**options)
    assert deterministic == run(**options) and "timing" not in deterministic
    timed = run(**options, timing=True)
    timing = timed.pop("timing")
    assert timed == deterministic
    assert timing["scope"] == "host-wall-time-not-WCET" and timing["seconds_per_tick"] > 0
    for key, count in {"nominal": 0, "mapped": 200, "fallback_transition": 1, "fallback_latched": 1799}.items():
        stats = timing["paths"][key]
        assert stats["samples"] == count
        if count:
            assert 0 <= stats["min_s"] <= stats["mean_s"] <= stats["max_s"]
        else:
            assert stats == {"samples": 0}
    baseline = run(timing=True)
    assert baseline["timing"]["paths"]["nominal"]["samples"] == 2000


def test_campaign_retains_rejections_and_does_not_claim_universal_qualification():
    report = run_campaign(os.environ["NAMC_SIM_EXECUTABLE"], os.environ["NAMC_CORE_LIBRARY"], FIXTURE)
    assert len(list(cases())) == 56 and report["summary"]["runs"] == 180
    assert report["evidence"] == "simulation-only"
    completed = [r for r in report["runs"] if r["status"] == "completed"]
    rejected = [r for r in report["runs"] if r["status"] != "completed"]
    assert completed and rejected
    assert report["summary"]["completed"] == len(completed)
    assert report["summary"]["meets_criteria"] == sum(all(r["criteria"].values()) for r in completed)
    assert report["summary"]["meets_criteria"] < len(completed)
    assert all(r["criteria"] == assess(r["report"]) for r in completed)
    assert all(r["criteria"]["current"] and r["criteria"]["duty"] for r in completed)
    assert all(r["returncode"] in (1, 2) and r["stderr"] for r in rejected)
    assert any(r["case"]["name"] == "infeasible-headroom" and r["mode"] == "tuned" and
               "Tuning rejected before controller activation" in r["stderr"] for r in rejected)
    fallback = [r for r in completed if r["model_fault_step"] >= 0]
    assert fallback and any(r["meets_criteria"] for r in fallback)
    assert all(r["report"]["metrics"]["fallback_steps"] == 2000-r["model_fault_step"] for r in fallback)
    assert report["plant"]["map"] == model().to_data()
