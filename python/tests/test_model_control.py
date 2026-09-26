"""Model-aware compensation on the same hidden plant; synthetic evidence only."""

import json
import math
import os
from pathlib import Path
import subprocess
import sys

import pytest

from fluxforge_namc import FluxMap, FluxMapError, FluxMapLimits, run_flux_map_reference


FIXTURE = Path(__file__).resolve().parents[2] / "tests/fixtures/nonlinear_flux_map.json"
LIMITS = FluxMapLimits(1e-5, 1e-5, 1e-6)


def model(data=None):
    if data is None:
        return FluxMap.from_json(os.environ["NAMC_CORE_LIBRARY"], FIXTURE, limits=LIMITS)
    return FluxMap(os.environ["NAMC_CORE_LIBRARY"], data, LIMITS)


def run(plant=None, controller=None, **kwargs):
    return run_flux_map_reference(os.environ["NAMC_SIM_EXECUTABLE"],
                                  model() if plant is None else plant,
                                  controller_map=controller, **kwargs)


def narrow_model():
    data = model().to_data()
    data["source_id"] += "-narrow"
    for key in ("id_A", "iq_A"):
        data[key] = data[key][15:18]
    for key in ("psi_d_Wb", "psi_q_Wb"):
        data[key] = [row[15:18] for row in data[key][15:18]]
    return model(data)


@pytest.mark.parametrize("id,iq,load", [(-2, 5, 0.2), (-2, -5, -0.2), (2, 5, 0), (0, 0, 0)])
def test_known_model_same_plant_tracking(id, iq, load):
    baseline = run(id=id, iq=iq, load=load, seed=42)
    mapped = run(controller=model(), id=id, iq=iq, load=load, seed=42, trace_stride=20)
    assert mapped["plant"] == baseline["plant"]
    assert mapped["initial"] == baseline["initial"]
    for key, value in baseline["controller"].items():
        if key != "kind":
            assert mapped["controller"][key] == value
    assert mapped["metrics"]["tail_rms_current_error_A"] < 0.06
    assert mapped["metrics"]["max_current_A"] < 12
    assert mapped["metrics"]["fallback_steps"] == 0
    assert all(s["controller_mode"] == 1 for s in mapped["trace"])
    assert 0 <= mapped["metrics"]["min_duty"] <= mapped["metrics"]["max_duty"] <= 1


def test_model_and_policy_replay():
    plant, control = model(), narrow_model()
    report = run(plant, control, id=-2, model_failure="nominal", trace_stride=13, seed=9)
    restored_plant = FluxMap(os.environ["NAMC_CORE_LIBRARY"], report["plant"]["map"],
                            FluxMapLimits(**report["plant"]["map_limits"]))
    restored_control = FluxMap(os.environ["NAMC_CORE_LIBRARY"], report["controller_model"]["map"],
                              FluxMapLimits(**report["controller_model"]["map_limits"]))
    replay = run(restored_plant, restored_control,
                 id=report["id_reference_A"], iq=report["iq_reference_A"],
                 dt=report["dt_s"], steps=report["steps"], seed=report["seed"],
                 load=report["load_Nm"], vdc=report["vdc_V"], trace_stride=report["trace_stride"],
                 model_failure=report["controller"]["model_failure_policy"])
    assert replay == report
    assert report["plant"]["map"] == plant.to_data()
    assert report["controller_model"]["map"] == control.to_data()


@pytest.mark.parametrize("scale", [0.8, 1.2])
def test_mismatched_prior_keeps_plant_truth_and_limits_unchanged(scale):
    data = model().to_data()
    data["source_id"] += f"-affine-prior-{scale}"
    # Perturb PM flux and linear components by +/-20%; preserve the nonlinear
    # cross terms and interpolation error. The added affine field is exact
    # under bilinear interpolation, so no acceptance threshold is relaxed.
    data["psi_d_Wb"] = [[v + (scale-1)*(0.05 + 0.003*data["id_A"][i])
                         for v in row] for i, row in enumerate(data["psi_d_Wb"])]
    data["psi_q_Wb"] = [[v + (scale-1)*0.004*data["iq_A"][j]
                         for j, v in enumerate(row)] for row in data["psi_q_Wb"]]
    mismatch = run(controller=model(data), id=-2, load=0.2)
    known = run(controller=model(), id=-2, load=0.2)
    assert mismatch["plant"] == known["plant"]
    assert mismatch["controller"] == known["controller"]
    assert mismatch["controller_model"] != known["controller_model"]
    assert mismatch["metrics"] != known["metrics"]
    assert mismatch["metrics"]["tail_rms_current_error_A"] < 0.06
    assert mismatch["metrics"]["max_current_A"] < 12
    assert mismatch["metrics"]["fallback_steps"] == 0


def test_scaling_does_not_relax_map_acceptance_policy():
    data = model().to_data()
    for key in ("psi_d_Wb", "psi_q_Wb"):
        data[key] = [[v * 1.2 for v in row] for row in data[key]]
    # This also scales the interpolation-induced reciprocity error and is
    # rejected by the unchanged 10 uH tolerance. No simulation is launched.
    with pytest.raises(FluxMapError) as rejected:
        model(data)
    assert rejected.value.status == 5


def test_nominal_flux_map_matches_original_compensation():
    data = model().to_data()
    data["id_A"] = data["iq_A"] = [-8, 0, 8]
    data["psi_d_Wb"] = [[0.04 + 0.0015*d for _ in data["iq_A"]] for d in data["id_A"]]
    data["psi_q_Wb"] = [[0.002*q for q in data["iq_A"]] for _ in data["id_A"]]
    data["source_id"] = "project-original-independent-nominal-controller-map"
    data["declared_flux_error_Wb"] = 0
    baseline = run(id=-2, load=0.2)
    mapped = run(controller=model(data), id=-2, load=0.2)
    assert mapped["final"] == pytest.approx(baseline["final"], abs=1e-10, rel=0)
    assert mapped["metrics"] == pytest.approx(baseline["metrics"], abs=1e-10, rel=0)


def test_domain_failure_disables_by_default():
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(controller=narrow_model(), id=-2)
    assert failure.value.returncode == 1 and not failure.value.stdout
    assert "controller/inverter at step" in failure.value.stderr
    assert "status 4, model status 3" in failure.value.stderr


def test_explicit_fallback_is_latched_observable_and_bounded():
    report = run(controller=narrow_model(), id=-2, model_failure="nominal", trace_stride=1)
    first = report["model_diagnostics"]["first_fallback_step_zero_based"]
    assert 0 < first < report["steps"]
    assert report["model_diagnostics"]["flux_status"] == 3
    assert report["metrics"]["fallback_steps"] == report["steps"] - first
    assert all(s["controller_mode"] == 1 for s in report["trace"][:first])
    assert all(s["controller_mode"] == 2 for s in report["trace"][first:])
    assert report["metrics"]["tail_rms_current_error_A"] < 0.06
    assert report["metrics"]["max_current_A"] < 12
    assert 0 <= report["metrics"]["min_duty"] <= report["metrics"]["max_duty"] <= 1


def test_map_control_refinement():
    coarse = run(controller=model(), id=-2, load=0.2)
    fine = run(controller=model(), id=-2, load=0.2, dt=0.000025, steps=4000)
    for key in ("id_A", "iq_A", "speed_rad_s"):
        assert abs(coarse["final"][key] - fine["final"][key]) < 0.02


def test_voltage_limitation_and_closed_loop_recovery():
    report = run(controller=model(), id=-2, load=0.2, vdc=12, trace_stride=1)
    assert report["metrics"]["voltage_limit_steps"] > 0
    boundary = 12 / math.sqrt(3) * (1-1e-12)
    counted = sum(math.hypot(s["voltage_alpha_V"], s["voltage_beta_V"]) >= boundary
                  for s in report["trace"])
    assert counted == report["metrics"]["voltage_limit_steps"]
    assert report["metrics"]["tail_rms_current_error_A"] < 0.06
    assert report["metrics"]["max_current_A"] < 12
    assert 0 <= report["metrics"]["min_duty"] <= report["metrics"]["max_duty"] <= 1


def test_cli_same_plant_controller_comparison():
    command = [sys.executable, "-m", "fluxforge_namc",
               "--executable", os.environ["NAMC_SIM_EXECUTABLE"],
               "--library", os.environ["NAMC_CORE_LIBRARY"], "--map", str(FIXTURE),
               "--controller-map", str(FIXTURE), "--min-incremental-h", "1e-5",
               "--max-reciprocity-error-h", "1e-5", "--min-rcond", "1e-6",
               "--control-min-incremental-h", "1e-5", "--control-max-reciprocity-error-h", "1e-5",
               "--control-min-rcond", "1e-6", "--compare-controllers", "--id", "-2"]
    result = subprocess.run(command, text=True, capture_output=True, check=True, timeout=30)
    report = json.loads(result.stdout)
    assert report["nominal"]["plant"] == report["model_aware"]["plant"]
    assert report["model_aware"] == run(controller=model(), id=-2)
    for key, value in report["metric_delta_model_minus_nominal"].items():
        assert value == report["model_aware"]["metrics"][key] - report["nominal"]["metrics"][key]
    failed = subprocess.run(command + ["--compare-linear"], text=True, capture_output=True, timeout=10)
    assert failed.returncode == 2 and not failed.stdout


def test_missing_controller_transport_is_not_replaced_by_plant_truth():
    result = subprocess.run([os.environ["NAMC_SIM_EXECUTABLE"], "--plant", "lut",
                             "--controller", "flux-map"], input=model()._simulation_transport(),
                            text=True, capture_output=True, timeout=10)
    assert result.returncode == 2 and not result.stdout
    assert "Controller map transport rejected" in result.stderr
