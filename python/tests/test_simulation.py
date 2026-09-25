"""End-to-end regression tests against the native simulator executable."""

import math
import os
import subprocess

import pytest

from fluxforge_namc import run_linear_reference


def run(**kwargs):
    return run_linear_reference(os.environ["NAMC_SIM_EXECUTABLE"], **kwargs)


def test_repeatability_and_recorded_configuration():
    result = run(seed=42)
    assert result == run(seed=42)
    assert result["initial"] != run(seed=43)["initial"]
    assert result["seed"] == 42
    assert result["steps"] == 2000
    assert result["evidence"] == "simulation-only"
    assert result["revision_at_configure"]
    assert result["compiler"]
    assert "build_configuration" in result
    assert result["plant"]["Ld_H"] != result["controller"]["Ld_prior_H"]
    assert result["plant"]["pm_flux_Wb"] != result["controller"]["pm_flux_prior_Wb"]
    # Reconstruct all variable inputs from the report. Fixed parameters are
    # identified by the source revision; dirty snapshots require saved source.
    replay = run(
        steps=result["steps"], seed=result["seed"], dt=result["dt_s"],
        iq=result["iq_reference_A"], vdc=result["vdc_V"], load=result["load_Nm"],
    )
    assert replay == result


@pytest.mark.parametrize("iq,load", [(5.0, 0.0), (5.0, 0.2), (-5.0, -0.2), (0.0, 0.0)])
def test_current_tracking(iq, load):
    result = run(iq=iq, load=load)
    metrics = result["metrics"]
    assert all(math.isfinite(value) for value in metrics.values())
    assert metrics["tail_rms_current_error_A"] < 0.06
    assert metrics["max_current_A"] <= 12.0
    assert 0.0 <= metrics["min_duty"] <= metrics["max_duty"] <= 1.0
    assert abs(result["final"]["id_A"]) < 0.06
    assert abs(result["final"]["iq_A"] - iq) < 0.06


def test_sample_time_refinement():
    coarse = run(dt=0.00005, steps=2000)
    fine = run(dt=0.000025, steps=4000)
    assert abs(coarse["final"]["iq_A"] - fine["final"]["iq_A"]) < 0.02
    assert abs(coarse["final"]["speed_rad_s"] - fine["final"]["speed_rad_s"]) < 0.02


@pytest.mark.parametrize("kwargs", [
    {"iq": math.nan}, {"vdc": math.inf}, {"vdc": 0}, {"dt": -1},
    {"dt": 1}, {"steps": 0}, {"steps": 2.5}, {"seed": -1},
    {"seed": 4294967296}, {"iq": 11}, {"load": 3},
])
def test_invalid_experiments_fail_without_success_report(kwargs):
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run(**kwargs)
    assert failure.value.returncode == 2
    assert not failure.value.stdout
    assert "Usage:" in failure.value.stderr
