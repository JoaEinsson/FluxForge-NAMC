"""Offline C PI design followed by synthetic same-plant experiments."""

from dataclasses import asdict, replace
import json
import math
import os
import subprocess
import sys

import pytest

from fluxforge_namc import CurrentTuning, FluxMap, FluxMapLimits
from test_model_control import FIXTURE, model, run


REQUEST = CurrentTuning(bandwidth=1500, resistance=0.4)
GAIN_KEYS = {"kp_d_V_A", "kp_q_V_A", "ki_d_V_As", "ki_q_V_As"}


@pytest.mark.parametrize("id,iq,load", [(-2, 5, 0.2), (-2, -5, -0.2), (2, 5, 0), (0, 0, 0)])
def test_tuned_scenarios_preserve_truth_and_limits(id, iq, load):
    fixed = run(controller=model(), id=id, iq=iq, load=load, seed=42)
    tuned = run(controller=model(), tuning=REQUEST, id=id, iq=iq, load=load, seed=42)
    assert tuned["plant"] == fixed["plant"]
    assert tuned["initial"] == fixed["initial"]
    assert tuned["controller_model"] == fixed["controller_model"]
    for key, value in fixed["controller"].items():
        if key not in GAIN_KEYS:
            assert tuned["controller"][key] == value
    assert tuned["tuning"]["request"] == asdict(REQUEST)
    assert tuned["metrics"]["tail_rms_current_error_A"] < 0.06
    assert tuned["metrics"]["max_current_A"] < 12
    assert tuned["metrics"]["fallback_steps"] == 0
    assert 0 <= tuned["metrics"]["min_duty"] <= tuned["metrics"]["max_duty"] <= 1


def test_gains_match_full_local_model_and_caps():
    report = run(controller=model(), tuning=replace(REQUEST, bandwidth=1e6), id=-2)
    result, control = report["tuning"], report["controller"]
    j = result["jacobian_H"]
    expected = model().jacobian(-2, 5)
    assert list(j.values()) == pytest.approx(expected, abs=1e-14)
    # Independent explicit 2x2 inverse, only a test oracle, never Python runtime.
    det = j["dd"]*j["qq"] - j["dq"]*j["qd"]
    inverse_norm = max(abs(j["qq"])+abs(j["dq"]), abs(j["qd"])+abs(j["dd"]))/abs(det)
    factor = max(abs(j["qq"]*j["dd"])+abs(j["dq"]*j["qq"]),
                 abs(j["qd"]*j["dd"])+abs(j["dd"]*j["qq"]))/abs(det)
    assert result["inverse_norm_per_H"] == pytest.approx(inverse_norm)
    assert result["coupling_factor"] == pytest.approx(factor)
    assert j["dq"] != 0 and j["qd"] != 0 and factor > 1
    omega = result["selected_bandwidth_rad_s"]
    assert 10 <= omega <= 2000 and result["caps"] & 1
    assert control["kp_d_V_A"] == pytest.approx(omega*j["dd"])
    assert control["kp_q_V_A"] == pytest.approx(omega*j["qq"])
    assert control["ki_d_V_As"] == pytest.approx(omega*0.4)
    assert result["rate_sample"] <= REQUEST.max_rate_sample


@pytest.mark.parametrize("resistance", [0.3, 0.5])
def test_independent_resistance_mismatch(resistance):
    matched = run(controller=model(), tuning=REQUEST, id=-2, load=0.2)
    mismatch = run(controller=model(), tuning=replace(REQUEST, resistance=resistance), id=-2, load=0.2)
    assert mismatch["plant"] == matched["plant"]
    assert mismatch["controller"]["ki_d_V_As"] != matched["controller"]["ki_d_V_As"]
    assert mismatch["metrics"]["tail_rms_current_error_A"] < 0.06
    assert mismatch["metrics"]["max_current_A"] < 12


@pytest.mark.parametrize("scale", [0.8, 1.2])
def test_tuning_from_mismatched_map_does_not_read_plant(scale):
    data = model().to_data()
    data["source_id"] += f"-tuning-affine-prior-{scale}"
    data["psi_d_Wb"] = [[v+(scale-1)*(0.05+0.003*data["id_A"][i]) for v in row]
                         for i, row in enumerate(data["psi_d_Wb"])]
    data["psi_q_Wb"] = [[v+(scale-1)*0.004*data["iq_A"][j] for j, v in enumerate(row)]
                         for row in data["psi_q_Wb"]]
    known = run(controller=model(), tuning=REQUEST, id=-2, load=0.2)
    mismatch = run(controller=model(data), tuning=REQUEST, id=-2, load=0.2)
    assert mismatch["plant"] == known["plant"]
    assert mismatch["tuning"]["jacobian_H"] != known["tuning"]["jacobian_H"]
    assert mismatch["controller"]["kp_d_V_A"] != known["controller"]["kp_d_V_A"]
    assert mismatch["metrics"]["tail_rms_current_error_A"] < 0.06
    assert mismatch["metrics"]["max_current_A"] < 12


def test_low_bus_caps_tuning_without_relaxing_runtime_limits():
    report = run(controller=model(), tuning=REQUEST, id=-2, load=0.2, vdc=12, trace_stride=1)
    tuned, control = report["tuning"], report["controller"]
    assert tuned["caps"] & 16
    assert tuned["selected_bandwidth_rad_s"] < REQUEST.bandwidth
    budget = REQUEST.voltage_fraction*tuned["voltage_headroom_V"]
    assert (max(control["kp_d_V_A"], control["kp_q_V_A"])+report["dt_s"]*control["ki_d_V_As"])*REQUEST.design_error <= budget*(1+1e-12)
    assert report["metrics"]["tail_rms_current_error_A"] < 0.06
    assert report["metrics"]["max_current_A"] < 12
    assert all(math.hypot(s["voltage_alpha_V"], s["voltage_beta_V"]) <= 12/math.sqrt(3)*(1+1e-12)
               for s in report["trace"])


def test_full_tuned_replay_and_sample_refinement():
    report = run(controller=model(), tuning=REQUEST, id=-2, load=0.2, trace_stride=17, seed=42)
    restore = lambda value: FluxMap(os.environ["NAMC_CORE_LIBRARY"], value["map"],
                                    FluxMapLimits(**value["map_limits"]))
    replay = run(restore(report["plant"]), restore(report["controller_model"]),
                 tuning=CurrentTuning(**report["tuning"]["request"]),
                 id=report["id_reference_A"], iq=report["iq_reference_A"],
                 dt=report["dt_s"], steps=report["steps"], seed=report["seed"],
                 load=report["load_Nm"], vdc=report["vdc_V"], trace_stride=report["trace_stride"])
    assert replay == report
    refined = run(controller=model(), tuning=REQUEST, id=-2, load=0.2, seed=42, dt=0.000025, steps=4000)
    for key in ("id_A", "iq_A", "speed_rad_s"):
        assert abs(report["final"][key]-refined["final"][key]) < 0.02


@pytest.mark.parametrize("changes", [
    {"resistance": 0}, {"bandwidth": float("nan")}, {"design_error": 10},
    {"electrical_speed": 1000}, {"min_bandwidth": 1900}, {"max_rate_sample": 1e-6},
    {"voltage_fraction": 2}, {"max_kp": -1},
])
def test_bad_or_infeasible_tuning_has_no_success_report(changes):
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(controller=model(), tuning=replace(REQUEST, **changes), id=-2)
    assert failure.value.returncode == 2 and not failure.value.stdout


@pytest.mark.parametrize("controller,failure_policy", [(None, "disable"), ("model", "nominal")])
def test_no_implicit_controller_model_or_unvalidated_fallback(controller, failure_policy):
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(controller=model() if controller else None, tuning=REQUEST, model_failure=failure_policy)
    assert failure.value.returncode == 2 and not failure.value.stdout


def test_cli_three_way_comparison_and_required_inputs():
    command = [sys.executable, "-m", "fluxforge_namc", "--executable", os.environ["NAMC_SIM_EXECUTABLE"],
               "--library", os.environ["NAMC_CORE_LIBRARY"], "--map", str(FIXTURE),
               "--controller-map", str(FIXTURE), "--min-incremental-h", "1e-5",
               "--max-reciprocity-error-h", "1e-5", "--min-rcond", "1e-6",
               "--control-min-incremental-h", "1e-5", "--control-max-reciprocity-error-h", "1e-5",
               "--control-min-rcond", "1e-6", "--compare-controllers", "--id", "-2",
               "--tune-bandwidth", "1500"]
    missing = subprocess.run(command, text=True, capture_output=True, timeout=10)
    assert missing.returncode == 2 and not missing.stdout
    result = subprocess.run(command + ["--tune-resistance", "0.4"], text=True,
                            capture_output=True, check=True, timeout=30)
    report = json.loads(result.stdout)
    assert report["schema"] == "namc-tuning-comparison-experimental-v1"
    assert report["nominal"]["plant"] == report["model_aware"]["plant"] == report["tuned_model_aware"]["plant"]
    assert report["tuned_model_aware"] == run(controller=model(), tuning=REQUEST, id=-2)
    for key, delta in report["metric_delta_tuned_minus_fixed"].items():
        assert delta == report["tuned_model_aware"]["metrics"][key]-report["model_aware"]["metrics"][key]
