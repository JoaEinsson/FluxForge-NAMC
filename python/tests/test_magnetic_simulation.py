"""Replayable native experiments. All fixtures/results are simulation-only."""

import json
import math
import os
from pathlib import Path
import subprocess
import sys

import pytest

from fluxforge_namc import FluxMap, FluxMapLimits, run_flux_map_reference, run_linear_reference


FIXTURE = Path(__file__).resolve().parents[2] / "tests/fixtures/nonlinear_flux_map.json"
LIMITS = FluxMapLimits(1e-5, 1e-5, 1e-6)


def model(data=None):
    if data is None:
        return FluxMap.from_json(os.environ["NAMC_CORE_LIBRARY"], FIXTURE, limits=LIMITS)
    return FluxMap(os.environ["NAMC_CORE_LIBRARY"], data, LIMITS)


def run(flux_map=None, **kwargs):
    return run_flux_map_reference(
        os.environ["NAMC_SIM_EXECUTABLE"], model() if flux_map is None else flux_map, **kwargs,
    )


def test_checked_in_fixture_matches_documented_analytical_nodes():
    data = model().to_data()
    assert len(data["id_A"]) == len(data["iq_A"]) == 33
    for i, d in enumerate(data["id_A"]):
        for j, q in enumerate(data["iq_A"]):
            assert data["psi_d_Wb"][i][j] == pytest.approx(
                0.05 + 0.003*d - 2e-6*d**3 - 1.2e-6*d*q**2, abs=1e-17, rel=0,
            )
            assert data["psi_q_Wb"][i][j] == pytest.approx(
                0.004*q - 3e-6*q**3 - 1.2e-6*d**2*q, abs=1e-17, rel=0,
            )


def test_nonlinear_map_replay_and_source_snapshot():
    original = model()
    result = run(original, id=-2, seed=42, load=0.2, trace_stride=23)
    assert result == run(original, id=-2, seed=42, load=0.2, trace_stride=23)
    assert result["schema"] == "namc-flux-map-reference-experimental-v1"
    assert result["plant_backend"] == "coupled-flux-lut"
    assert result["plant"]["map"] == original.to_data()
    assert result["evidence"] == "simulation-only"
    assert result["plant"]["map"]["evidence"] == "analytical"
    restored = FluxMap(
        os.environ["NAMC_CORE_LIBRARY"], result["plant"]["map"],
        FluxMapLimits(**result["plant"]["map_limits"]),
    )
    replay = run(
        restored, id=result["id_reference_A"], iq=result["iq_reference_A"],
        steps=result["steps"], seed=result["seed"], dt=result["dt_s"],
        vdc=result["vdc_V"], load=result["load_Nm"], trace_stride=result["trace_stride"],
    )
    assert replay == result
    assert result["controller"] == run_linear_reference(
        os.environ["NAMC_SIM_EXECUTABLE"], id=-2,
    )["controller"]


@pytest.mark.parametrize("id,iq,load", [(-2, 5, 0.2), (-2, -5, -0.2), (2, 5, 0), (0, 0, 0)])
def test_nonlinear_tracking_and_trace(id, iq, load):
    result = run(id=id, iq=iq, load=load, trace_stride=1)
    metrics = result["metrics"]
    assert metrics["tail_rms_current_error_A"] < 0.06
    assert metrics["max_current_A"] <= 12
    assert 0 <= metrics["min_duty"] <= metrics["max_duty"] <= 1
    trace = result["trace"]
    assert len(trace) == result["steps"]
    assert trace[-1]["step"] == result["steps"]
    assert trace[-1]["time_s"] == result["steps"] * result["dt_s"]
    for key in result["final"]:
        assert trace[-1][key] == result["final"][key]
    errors = [(s["id_A"] - id)**2 + (s["iq_A"] - iq)**2 for s in trace]
    assert math.sqrt(sum(errors) / len(errors)) == pytest.approx(metrics["rms_current_error_A"])
    assert max(abs(s["torque_Nm"]) for s in trace) == metrics["max_abs_torque_Nm"]
    assert all(math.isfinite(v) for sample in trace for v in sample.values())


def test_nonlinear_sample_refinement():
    coarse = run(id=-2, load=0.2, dt=0.00005, steps=2000)
    fine = run(id=-2, load=0.2, dt=0.000025, steps=4000)
    for key in ("id_A", "iq_A", "speed_rad_s"):
        assert abs(coarse["final"][key] - fine["final"][key]) < 0.02


def test_linear_limit_end_to_end():
    data = model().to_data()
    data["source_id"] = "project-original-linear-limit-not-measured"
    data["id_A"] = [-12, 0, 12]
    data["iq_A"] = [-12, 0, 12]
    # Independent test oracle: same parameters as the documented Phase 1 plant.
    data["psi_d_Wb"] = [[0.045 + 0.0018*d for _ in data["iq_A"]] for d in data["id_A"]]
    data["psi_q_Wb"] = [[0.0024*q for q in data["iq_A"]] for _ in data["id_A"]]
    data["declared_flux_error_Wb"] = 0
    nonlinear = run(model(data), id=-2, seed=9, load=0.2, trace_stride=37)
    linear = run_linear_reference(
        os.environ["NAMC_SIM_EXECUTABLE"], id=-2, seed=9, load=0.2, trace_stride=37,
    )
    assert nonlinear["controller"] == linear["controller"]
    assert nonlinear["final"] == pytest.approx(linear["final"], abs=1e-10, rel=0)
    assert nonlinear["metrics"] == pytest.approx(linear["metrics"], abs=1e-10, rel=0)
    for a, b in zip(nonlinear["trace"], linear["trace"], strict=True):
        assert a == pytest.approx(b, abs=1e-10, rel=0)


def test_provenance_escaping_and_copy_ownership():
    data = model().to_data()
    data["source_id"] = 'synthetic "quoted" \\ path\n\u00e9\u03a8'
    data["evidence"] = "measured"  # Label is declared, never certified by parsing.
    original = model(data)
    exported = original.to_data()
    exported["psi_d_Wb"][0][0] = 123
    result = run(original, steps=2)
    assert result["plant"]["map"] == data
    assert result["evidence"] == "simulation-only"


@pytest.mark.parametrize("kwargs,diagnostic", [
    ({"id": math.nan}, "Usage:"), ({"id": 11}, "Usage:"),
    ({"id": 9}, "unsupported"), ({"id": 10, "iq": 10}, "reference limit"),
    ({"trace_stride": -1}, "Usage:"), ({"trace_stride": 1.5}, "Usage:"),
    ({"steps": 10001, "trace_stride": 1}, "trace capacity"),
])
def test_invalid_configuration_no_success_report(kwargs, diagnostic):
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(**kwargs)
    assert failure.value.returncode == 2
    assert not failure.value.stdout
    assert diagnostic in failure.value.stderr


def test_runtime_map_domain_failure_stops_without_report():
    data = model().to_data()
    # The initial/reference point is supported, but a nonzero load develops
    # back EMF and leaves this intentionally tiny valid affine domain.
    data["id_A"] = [-1e-8, 0, 1e-8]
    data["iq_A"] = [-1e-8, 0, 1e-8]
    data["psi_d_Wb"] = [[0.05 + 0.003*d for _ in data["iq_A"]] for d in data["id_A"]]
    data["psi_q_Wb"] = [[0.004*q for q in data["iq_A"]] for _ in data["id_A"]]
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(model(data), iq=0, load=1)
    assert failure.value.returncode == 1
    assert not failure.value.stdout
    assert "magnetic plant at step" in failure.value.stderr
    assert "flux status 3" in failure.value.stderr


@pytest.mark.parametrize("damage", ["magic", "size", "truncated", "extra", "nan", "negative-slope"])
def test_native_transport_does_not_trust_the_wrapper(damage):
    tokens = model()._simulation_transport().split()
    if damage == "magic": tokens[0] = "unknown"
    if damage == "size": tokens[4] = "257"
    if damage == "truncated": tokens = tokens[:-1]
    if damage == "extra": tokens.append("42")
    if damage == "nan": tokens[-1] = "nan"
    if damage == "negative-slope": tokens[-1] = "-1"
    result = subprocess.run(
        [os.environ["NAMC_SIM_EXECUTABLE"], "--plant", "lut"],
        input="\n".join(tokens), capture_output=True, text=True, timeout=10,
    )
    assert result.returncode == 2
    assert not result.stdout
    assert "Map transport rejected" in result.stderr


def test_cli_and_explicit_policy():
    command = [
        sys.executable, "-m", "fluxforge_namc", "--executable", os.environ["NAMC_SIM_EXECUTABLE"],
        "--map", str(FIXTURE), "--library", os.environ["NAMC_CORE_LIBRARY"], "--steps", "2",
    ]
    failed = subprocess.run(command, text=True, capture_output=True, timeout=10)
    assert failed.returncode == 2 and not failed.stdout
    assert "thresholds" in failed.stderr
    result = subprocess.run(command + [
        "--min-incremental-h", "1e-5", "--max-reciprocity-error-h", "1e-5", "--min-rcond", "1e-6",
    ], text=True, capture_output=True, check=True, timeout=10)
    assert json.loads(result.stdout) == run(steps=2)
    comparison = subprocess.run(command + [
        "--min-incremental-h", "1e-5", "--max-reciprocity-error-h", "1e-5", "--min-rcond", "1e-6",
        "--compare-linear",
    ], text=True, capture_output=True, check=True, timeout=10)
    report = json.loads(comparison.stdout)
    assert report["coupled"] == run(steps=2)
    assert report["coupled"]["controller"] == report["linear"]["controller"]
    for key, delta in report["metric_delta_lut_minus_linear"].items():
        assert delta == report["coupled"]["metrics"][key] - report["linear"]["metrics"][key]


@pytest.mark.parametrize("stride", [0, 3, 100])
def test_decimation_keeps_final_sample_without_changing_results(stride):
    result = run(steps=11, trace_stride=stride)
    baseline = run(steps=11)
    assert result["final"] == baseline["final"]
    assert result["metrics"] == baseline["metrics"]
    expected = [] if stride == 0 else sorted(set([*range(stride, 12, stride), 11]))
    assert [sample["step"] for sample in result["trace"]] == expected
