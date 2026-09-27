"""Native resistance identification against a separate nonlinear hidden plant."""

import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

from fluxforge_namc import FluxMap, FluxMapLimits, ResistanceExperiment, run_resistance_identification


FIXTURE = Path(__file__).resolve().parents[2] / "tests/fixtures/nonlinear_flux_map.json"
LIMITS = FluxMapLimits(1e-5, 1e-5, 1e-6)


def model():
    return FluxMap.from_json(os.environ["NAMC_CORE_LIBRARY"], FIXTURE, limits=LIMITS)


def run(**kwargs):
    return run_resistance_identification(os.environ["NAMC_IDENT_EXECUTABLE"], model(),
                                         experiment=ResistanceExperiment(**kwargs))


@pytest.mark.parametrize("resistance", [0.2, 0.4, 0.8])
@pytest.mark.parametrize("seed", [1, 42, 987])
def test_estimate_hidden_resistance_without_changing_controller(resistance, seed):
    r = run(plant_resistance=resistance, seed=seed)
    assert r["accepted"] and r["state"]["phase"] == 3 and r["state"]["reason"] == 0
    estimate = r["estimate"]
    assert estimate["R_ohm"] == pytest.approx(resistance, abs=2e-6)
    assert estimate["lower_ohm"] <= resistance <= estimate["upper_ohm"]
    assert abs(estimate["validation_residual_V"]) <= r["config"]["validation_voltage_tolerance_V"]
    assert r["steps"] == r["state"]["elapsed_samples"] == 4500
    assert [w["samples"] for w in r["windows"]] == [500, 500, 500]
    assert r["plant"]["R_ohm"] == resistance and r["plant"]["map"] == model().to_data()
    assert r["controller"] == run()["controller"]  # Independent fixed prior, never retuned.
    assert r["evidence"] == "simulation-only" and not r["state"]["excitation_enabled"]
    assert r["metrics"]["peak_current_A"] <= r["limits"]["max_current_A"]
    assert r["metrics"]["peak_voltage_V"] <= r["limits"]["max_voltage_V"]
    assert 0 < r["state"]["apparent_energy_J"] < r["limits"]["max_apparent_energy_J"]


def test_replay_report_inputs_and_sample_refinement():
    r = run(seed=123, plant_resistance=0.8, current_noise=0.0005, voltage_noise=0.0005,
            current_uncertainty=0.001, voltage_uncertainty=0.001)
    restored = FluxMap(os.environ["NAMC_CORE_LIBRARY"], r["plant"]["map"],
                       FluxMapLimits(**r["plant"]["map_limits"]))
    replay = run_resistance_identification(os.environ["NAMC_IDENT_EXECUTABLE"], restored,
                                           experiment=ResistanceExperiment(**r["experiment"]))
    assert replay == r and r["accepted"]
    assert r["estimate"]["lower_ohm"] <= 0.8 <= r["estimate"]["upper_ohm"]
    refined = run(dt=0.000025, settle_samples=2000, measure_samples=1000, max_samples=10000)
    assert refined["accepted"] and refined["estimate"]["R_ohm"] == pytest.approx(0.4, abs=2e-6)


def test_constant_voltage_offset_is_reported_not_absorbed_into_resistance():
    zero, biased = run(), run(voltage_bias=0.02)
    assert biased["accepted"]
    assert biased["estimate"]["R_ohm"] == pytest.approx(zero["estimate"]["R_ohm"], abs=1e-12)
    assert biased["estimate"]["offset_V"]-zero["estimate"]["offset_V"] == pytest.approx(0.02)
    assert biased["plant"] == zero["plant"] and biased["controller"] == zero["controller"]


@pytest.mark.parametrize("inputs,reason,phase", [
    ({"voltage_bias": 0.1}, 9, 4),
    ({"voltage_signed_error": 0.01}, 11, 4),
    ({"current_noise": 0.02}, 5, 4),
    ({"voltage_uncertainty": 0.02}, 10, 4),
    ({"initial_speed": 0.2}, 3, 5),
    ({"max_samples": 2000}, 12, 4),
    ({"energy_limit": 0.01}, 3, 5),
    ({"voltage_source": 0}, 4, 4),
    ({"settle_samples": 1}, 6, 4),
])
def test_failed_identification_never_publishes_estimate_or_keeps_excitation(inputs, reason, phase):
    r = run(**inputs)
    assert not r["accepted"] and r["estimate"] is None
    assert r["state"]["phase"] == phase and r["state"]["reason"] == reason
    assert not r["state"]["excitation_enabled"] and r["steps"] <= r["config"]["max_samples"]
    if "energy_limit" in inputs:
        # The last observed interval is included even when it crosses the budget.
        assert r["state"]["apparent_energy_J"] >= inputs["energy_limit"]


@pytest.mark.parametrize("inputs", [
    {"plant_resistance": 0}, {"dt": float("nan")}, {"max_samples": 0},
    {"measure_samples": 1}, {"seed": -1}, {"current_uncertainty": -0.1},
    {"settle_samples": 1.5}, {"voltage_source": 99},
])
def test_invalid_configuration_has_no_success_report(inputs):
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(**inputs)
    assert failure.value.returncode == 2 and not failure.value.stdout


def test_cli_report_and_explicit_thresholds(tmp_path):
    output = tmp_path / "identification.json"
    command = [sys.executable, "-m", "fluxforge_namc.identify",
               "--executable", os.environ["NAMC_IDENT_EXECUTABLE"],
               "--library", os.environ["NAMC_CORE_LIBRARY"], "--map", str(FIXTURE),
               "--output", str(output)]
    missing = subprocess.run(command, capture_output=True, text=True, timeout=10)
    assert missing.returncode == 2 and not output.exists()
    complete = subprocess.run(command + ["--min-incremental-h", "1e-5", "--max-reciprocity-error-h", "1e-5",
                                        "--min-rcond", "1e-6"],
                              capture_output=True, text=True, check=True, timeout=30)
    assert json.loads(complete.stdout)["accepted"]
    assert json.loads(output.read_text(encoding="utf-8")) == run()
