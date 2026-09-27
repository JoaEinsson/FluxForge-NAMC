"""Observable-only flux acquisition, coverage and end-to-end phase-4 regression."""

import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

from fluxforge_namc import (FluxMap, FluxMapLimits, FluxMapError, ResistanceExperiment, FluxPointExperiment,
                            run_flux_identification, run_local_identification)
from fluxforge_namc.flux_identification import DEFAULT_POINTS


FIXTURE = Path(__file__).resolve().parents[2] / "tests/fixtures/nonlinear_flux_map.json"
LIMITS = FluxMapLimits(1e-5, 1e-5, 1e-6)


def model():
    return FluxMap.from_json(os.environ["NAMC_CORE_LIBRARY"], FIXTURE, limits=LIMITS)


def run(**options):
    return run_flux_identification(os.environ["NAMC_FLUX_IDENT_EXECUTABLE"], model(),
                                   experiment=FluxPointExperiment(0.4, 0.0003, **options))


def campaign(**options):
    return run_local_identification(os.environ["NAMC_IDENT_EXECUTABLE"],
                                     os.environ["NAMC_FLUX_IDENT_EXECUTABLE"], model(), **options)


@pytest.mark.parametrize("point", [(0, 0), (4, 4), (-4, 4), (4, -4), (-2.3, 1.7)])
@pytest.mark.parametrize("seed", [1, 42, 987])
def test_coupled_flux_points_have_truth_inside_conditional_bounds(point, seed):
    r = run(id=point[0], iq=point[1], seed=seed)
    assert r["accepted"] and r["state"]["phase"] == 3 and r["steps"] == 6000
    assert [w["samples"] for w in r["windows"]] == [500]*3
    assert not r["state"]["excitation_enabled"]
    estimate = r["estimate"]
    assert estimate["pm_reference"] == (point == (0, 0))
    assert r["evaluation"]["truth_flux_Wb"] == list(model().flux(*point))
    for f, u, truth in zip(estimate["flux_Wb"], estimate["uncertainty_Wb"], r["evaluation"]["truth_flux_Wb"]):
        assert abs(f-truth) <= u and abs(f-truth) < 2e-6
    assert max(map(abs, estimate["holdout_residual_V"])) < 0.005
    assert r["evaluation"]["peak_current_A"] < r["limits"]["max_current_A"]
    assert r["evaluation"]["peak_voltage_V"] < r["limits"]["max_voltage_V"]
    assert r["state"]["apparent_energy_J"] < r["limits"]["max_apparent_energy_J"]


def test_complete_campaign_handoff_coverage_pm_and_cross_saturation():
    r = campaign()
    assert r["complete"] and r["state"] == "complete"
    assert len(r["runs"]) == len(r["observations"]) == 11
    assert r["coverage"]["accepted_points_A"] == [list(p[:2]) for p in DEFAULT_POINTS]
    assert not r["coverage"]["complete_map"] and not r["coverage"]["interpolation_supported"]
    assert r["coverage"]["support"] == "accepted-points-only"
    assert [1, 1] not in r["coverage"]["accepted_points_A"]  # Inside bounding box is NOT acquired.
    assert r["pm_reference"]["flux_Wb"][0] == pytest.approx(0.05, abs=2e-6)
    observed = {tuple(o["target_A"]): o for o in r["observations"]}
    # Both cross effects must be resolved, not forced to zero/per-axis curves.
    for first, second, axis in [((4, 4), (4, 0), 0), ((4, 4), (0, 4), 1)]:
        a, b = observed[first], observed[second]
        difference = a["flux_Wb"][axis]-b["flux_Wb"][axis]
        assert difference < 0 and abs(difference) > a["uncertainty_Wb"][axis]+b["uncertainty_Wb"][axis]
        expected = model().flux(*first)[axis]-model().flux(*second)[axis]
        assert difference == pytest.approx(expected, abs=2e-6)
    for o in r["observations"]:
        assert "plant" not in o and "evaluation" not in o and "truth_flux_Wb" not in o
        assert o["temperature_K"] is None and o["temperature_status"] == "unmeasured"
        assert o["resistance_estimate_ohm"] == r["resistance"]["estimate"]["R_ohm"]
    assert [o["role"] for o in r["observations"]].count("held-out") == 2


@pytest.mark.parametrize("resistance", [0.2, 0.8])
def test_estimated_resistance_handoff_not_hidden_truth(resistance):
    r = campaign(resistance_experiment=ResistanceExperiment(plant_resistance=resistance),
                 points=((0, 0, "acquisition"), (4, 4, "held-out")))
    assert r["complete"]
    for entry in r["runs"]:
        run_report = entry["report"]
        assert run_report["config"]["R_ohm"] == r["resistance"]["estimate"]["R_ohm"]
        assert run_report["controller"] == run()["controller"]
        for f, truth, u in zip(run_report["estimate"]["flux_Wb"], run_report["evaluation"]["truth_flux_Wb"],
                               run_report["estimate"]["uncertainty_Wb"]):
            assert abs(f-truth) <= u


def test_replay_noise_and_sample_refinement():
    r = run(seed=123, id=4, iq=-4, current_noise=0.0002, voltage_noise=0.0002,
            current_uncertainty=0.0003, voltage_uncertainty=0.0003)
    restored = FluxMap(os.environ["NAMC_CORE_LIBRARY"], r["plant"]["map"], FluxMapLimits(**r["plant"]["map_limits"]))
    replay = run_flux_identification(os.environ["NAMC_FLUX_IDENT_EXECUTABLE"], restored,
                                      experiment=FluxPointExperiment(**r["experiment"]))
    assert r["accepted"] and r == replay
    for f, truth, u in zip(r["estimate"]["flux_Wb"], r["evaluation"]["truth_flux_Wb"], r["estimate"]["uncertainty_Wb"]):
        assert abs(f-truth) <= u
    coarse = run(id=4, iq=-4)
    fine = run(id=4, iq=-4, dt=0.000025, settle_samples=3000, measure_samples=1000, max_samples=14000)
    assert fine["accepted"] and fine["estimate"]["flux_Wb"] == pytest.approx(coarse["estimate"]["flux_Wb"], abs=2e-6)


@pytest.mark.parametrize("pm,scale", [(0.03, 0.8), (0.07, 1.0)])
@pytest.mark.parametrize("point", [(0, 0), (4, -4)])
def test_hidden_pm_and_magnetic_response_changes_do_not_change_controller(pm, scale, point):
    data = model().to_data()
    data["source_id"] = f"synthetic-scaled-{scale}-pm-{pm}"
    data["psi_d_Wb"] = [[pm+scale*(v-0.05) for v in row] for row in data["psi_d_Wb"]]
    data["psi_q_Wb"] = [[scale*v for v in row] for row in data["psi_q_Wb"]]
    changed = FluxMap(os.environ["NAMC_CORE_LIBRARY"], data, LIMITS)
    r = run_flux_identification(os.environ["NAMC_FLUX_IDENT_EXECUTABLE"], changed,
                                 experiment=FluxPointExperiment(0.4, 0.0003, id=point[0], iq=point[1]))
    assert r["accepted"] and r["controller"] == run()["controller"]
    for f, truth, u in zip(r["estimate"]["flux_Wb"], changed.flux(*point), r["estimate"]["uncertainty_Wb"]):
        assert abs(f-truth) <= u
    if point == (0, 0):
        assert r["estimate"]["pm_reference"] and r["estimate"]["flux_Wb"][0] == pytest.approx(pm, abs=2e-6)


def test_coarse_scaled_map_is_rejected_before_acquisition():
    # Scaling this coarse fixture also scales its bilinear reciprocity error.
    # Retain the negative case; do not relax map thresholds to run it.
    data = model().to_data()
    data["source_id"] = "synthetic-scaled-1.2-rejected"
    for field in ("psi_d_Wb", "psi_q_Wb"):
        data[field] = [[1.2*v for v in row] for row in data[field]]
    with pytest.raises(FluxMapError) as error:
        FluxMap(os.environ["NAMC_CORE_LIBRARY"], data, LIMITS)
    assert error.value.status == 5


def test_constant_offset_is_separated_and_unidentifiable_gain_is_explicit():
    zero, offset = run(), run(voltage_bias=0.02)
    assert offset["accepted"]
    assert offset["estimate"]["flux_Wb"] == pytest.approx(zero["estimate"]["flux_Wb"], abs=1e-12)
    assert offset["estimate"]["offset_V"][1] == pytest.approx(0.02)
    # Deliberately FALSE calibration bounds: terminal voltage gain cannot be
    # distinguished from flux at zero current. Preserve this negative evidence.
    false_metadata = run(voltage_gain_error=0.02)
    assert false_metadata["accepted"]
    assert abs(false_metadata["estimate"]["flux_Wb"][0]-0.05) > false_metadata["estimate"]["uncertainty_Wb"][0]
    declared = run(voltage_gain_error=0.02, voltage_uncertainty=0.1)
    assert not declared["accepted"] and declared["state"]["reason"] == 10


@pytest.mark.parametrize("inputs,reason,phase", [
    ({"voltage_bias": 0.1}, 9, 4), ({"voltage_speed_sign_error": 0.02}, 11, 4),
    ({"actual_speed_scale": 0}, 7, 4), ({"actual_speed_scale": 1.1}, 6, 4),
    ({"max_samples": 2500}, 12, 4), ({"id": 4, "iq": 4, "energy_limit": 0.01}, 3, 5),
    ({"voltage_source": 0}, 4, 4), ({"id": 4, "iq": 4, "settle_samples": 1}, 6, 4),
    ({"voltage_uncertainty": 0.02}, 10, 4), ({"current_noise": 0.002}, 5, 4),
])
def test_nonconvergence_and_bad_measurements_reject_without_candidate(inputs, reason, phase):
    r = run(**inputs)
    assert not r["accepted"] and r["estimate"] is None
    assert r["state"]["reason"] == reason and r["state"]["phase"] == phase
    assert not r["state"]["excitation_enabled"]
    assert r["steps"] <= r["config"]["max_samples"]


def test_campaign_degraded_fault_missing_anchor_and_no_resistance():
    rejected = campaign(resistance_experiment=ResistanceExperiment(voltage_source=0))
    assert not rejected["complete"] and not rejected["runs"] and not rejected["observations"]
    assert rejected["reason"] == "resistance-rejected"
    missing = campaign(points=((4, 4, "acquisition"),))
    assert not missing["complete"] and missing["pm_reference"] is None and len(missing["observations"]) == 1
    failed = campaign(flux_options={"energy_limit": 0.00001})
    assert failed["state"] == "fault" and not failed["complete"] and not failed["observations"]
    assert len(failed["coverage"]["unsupported_requests_A"]) == 11
    assert all(r["skipped"] == "previous-fault" for r in failed["runs"][1:])
    degraded = campaign(points=((0, 0, "acquisition"), (4, 4, "acquisition")),
                         flux_options={"voltage_source": 0})
    assert len(degraded["runs"]) == 2 and not degraded["observations"] and degraded["state"] == "degraded"
    # Native process failure is retained and stops the rest of the plan.
    stopped = campaign(points=((7, 7, "acquisition"), (0, 0, "acquisition")))
    assert stopped["state"] == "fault" and "process_error" in stopped["runs"][0]
    assert stopped["runs"][1]["skipped"] == "previous-fault"


@pytest.mark.parametrize("inputs", [{"speed": 0}, {"speed": 5}, {"id": float("nan")},
                                         {"dt": -1}, {"measure_samples": 1}, {"voltage_source": 99}])
def test_invalid_native_config_has_no_report(inputs):
    with pytest.raises(subprocess.CalledProcessError) as error:
        run(**inputs)
    assert error.value.returncode == 2 and not error.value.stdout


@pytest.mark.parametrize("options", [{"points": ()}, {"points": ((0, 0, "bad"),)},
    {"points": ((0, 0, "acquisition"),)*65}, {"points": ((0, 0, "acquisition"),)*2},
    {"points": ((float("nan"), 0, "acquisition"),)}, {"flux_options": {"resistance_estimate": 0.4}}])
def test_invalid_plan_is_rejected_before_acquisition(options):
    with pytest.raises(ValueError):
        campaign(**options)


def test_campaign_replay_and_cli(tmp_path):
    first = campaign(points=((0, 0, "acquisition"), (4, -4, "held-out")))
    source = first["resistance"]["plant"]
    restored = FluxMap(os.environ["NAMC_CORE_LIBRARY"], source["map"], FluxMapLimits(**source["map_limits"]))
    replay = run_local_identification(os.environ["NAMC_IDENT_EXECUTABLE"], os.environ["NAMC_FLUX_IDENT_EXECUTABLE"],
        restored, resistance_experiment=ResistanceExperiment(**first["resistance"]["experiment"]),
        points=first["requests"], flux_options=first["flux_options"])
    assert replay == first
    output = tmp_path / "local-identification.json"
    command = [sys.executable, "-m", "fluxforge_namc.local_identification",
        "--resistance-executable", os.environ["NAMC_IDENT_EXECUTABLE"],
        "--flux-executable", os.environ["NAMC_FLUX_IDENT_EXECUTABLE"],
        "--library", os.environ["NAMC_CORE_LIBRARY"], "--map", str(FIXTURE), "--output", str(output)]
    invalid = subprocess.run(command, capture_output=True, text=True, timeout=10)
    assert invalid.returncode == 2 and not output.exists()
    result = subprocess.run(command+["--min-incremental-h", "1e-5", "--max-reciprocity-error-h", "1e-5", "--min-rcond", "1e-6"],
                            capture_output=True, text=True, check=True, timeout=60)
    assert json.loads(result.stdout)["complete"]
    assert json.loads(output.read_text(encoding="utf-8")) == campaign()
