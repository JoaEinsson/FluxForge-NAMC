"""Orchestration and observation datasets for native, coupled flux acquisition."""

from dataclasses import asdict, dataclass, fields, replace
import json
import math
from pathlib import Path
import subprocess

from .flux_map import FluxMap
from .resistance import ResistanceExperiment, run_resistance_identification


@dataclass(frozen=True)
class FluxPointExperiment:
    """SI experiment inputs; estimates must be independent of hidden plant truth."""

    resistance_estimate: float
    resistance_uncertainty: float
    plant_resistance: float = 0.4
    id: float = 0.0
    iq: float = 0.0
    speed: float = 80.0  # electrical rad/s; native schedule is +speed,-speed,+speed/2
    actual_speed_scale: float = 1.0  # external shaft fault injection, not estimator input
    vdc: float = 48.0
    dt: float = 0.00005
    seed: int = 42
    settle_samples: int = 1500
    measure_samples: int = 500
    max_samples: int = 7000
    energy_limit: float = 30.0
    current_noise: float = 0.0
    voltage_noise: float = 0.0
    voltage_bias: float = 0.0
    voltage_speed_sign_error: float = 0.0
    voltage_gain_error: float = 0.0
    current_uncertainty: float = 0.0001
    voltage_uncertainty: float = 0.0001
    speed_uncertainty: float = 0.001
    voltage_source: int = 1

    def arguments(self):
        return [part for f in fields(self)
                for part in (f"--{f.name.replace('_', '-')}", str(getattr(self, f.name)))]


def run_flux_identification(executable: str | Path, plant_map: FluxMap, *, experiment: FluxPointExperiment):
    """Native simulation of one candidate point. A recorded rejection is not success."""
    result = subprocess.run([str(Path(executable).resolve()), *experiment.arguments()],
                            input=plant_map._simulation_transport(), check=True, text=True,
                            encoding="utf-8", capture_output=True, timeout=60)
    report = json.loads(result.stdout)
    data = report["plant"]["map"]
    data["source_id"] = bytes.fromhex(data.pop("source_id_utf8_hex")).decode("utf-8")
    report["experiment"] = asdict(experiment)
    return report


# Requested acquisition coverage, not LUT resolution or an interpolation domain.
DEFAULT_POINTS = tuple((d, q, "acquisition") for d in (-4.0, 0.0, 4.0) for q in (-4.0, 0.0, 4.0)) + (
    (-2.3, 1.7, "held-out"), (2.1, -1.3, "held-out"),
)


def run_local_identification(resistance_executable, flux_executable, plant_map, *,
                             resistance_experiment=ResistanceExperiment(), points=DEFAULT_POINTS,
                             flux_options=None):
    """Bounded campaign: accepted native R -> independent native flux-point runs.

    This does not fit, interpolate or activate a magnetic map. Unsupported
    points remain unsupported. Failed attempts are retained, never imputed.
    Each native run enforces its own immutable limits; at most 64 are allowed.
    A FAULT or process failure stops subsequent points; DEGRADED allows the next
    independent experiment but prevents a complete-campaign claim.
    """
    points = tuple(points)
    if not 1 <= len(points) <= 64:
        raise ValueError("Request 1..64 acquisition points")
    if any(not isinstance(p, (list, tuple)) or len(p) != 3 or p[2] not in ("acquisition", "held-out") for p in points):
        raise ValueError("Each point needs id, iq and acquisition/held-out role")
    # Validate the entire plan before running any experiment.
    if any(isinstance(v, bool) or not isinstance(v, (float, int)) or not math.isfinite(v)
           for p in points for v in p[:2]):
        raise ValueError("Point currents must be finite numbers")
    if len({(p[0], p[1]) for p in points}) != len(points):
        raise ValueError("Duplicate current point")
    options = dict(flux_options or {})
    allowed = {f.name for f in fields(FluxPointExperiment)} - {
        "resistance_estimate", "resistance_uncertainty", "plant_resistance", "id", "iq", "seed",
    }
    if not options.keys() <= allowed:
        raise ValueError("Flux options cannot override accepted R, hidden plant, seed or requested points")
    resistance = run_resistance_identification(resistance_executable, plant_map, experiment=resistance_experiment)
    report = {
        "schema": "namc-local-identification-experimental-v1", "evidence": "simulation-only",
        "resistance": resistance, "requests": [list(p) for p in points], "flux_options": options,
        "runs": [], "observations": [], "pm_reference": None,
        "coverage": {"support": "accepted-points-only", "interpolation_supported": False,
                     "complete_map": False, "accepted_points_A": [], "unsupported_requests_A": []},
        "state": "degraded", "reason": "resistance-rejected", "complete": False,
    }
    if not resistance["accepted"]:
        if resistance["state"]["phase"] == 5:
            report["state"] = "fault"
        report["coverage"]["unsupported_requests_A"] = [list(p[:2]) for p in points]
        return report
    estimate = resistance["estimate"]
    uncertainty = max(estimate["R_ohm"]-estimate["lower_ohm"], estimate["upper_ohm"]-estimate["R_ohm"])
    base = FluxPointExperiment(estimate["R_ohm"], uncertainty,
                               plant_resistance=resistance_experiment.plant_resistance,
                               seed=resistance_experiment.seed, **options)
    stopped = False
    for d, q, role in points:
        if stopped:
            report["runs"].append({"target_A": [d, q], "role": role, "skipped": "previous-fault"})
            report["coverage"]["unsupported_requests_A"].append([d, q])
            continue
        try:
            run = run_flux_identification(flux_executable, plant_map, experiment=replace(base, id=d, iq=q))
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired, OSError) as error:
            report["runs"].append({"target_A": [d, q], "role": role,
                                   "process_error": getattr(error, "returncode", None),
                                   "diagnostic": error.stderr if isinstance(error, subprocess.CalledProcessError) else str(error)})
            report["coverage"]["unsupported_requests_A"].append([d, q])
            stopped = True
            continue
        report["runs"].append({"target_A": [d, q], "role": role, "report": run})
        if not run["accepted"]:
            report["coverage"]["unsupported_requests_A"].append([d, q])
            stopped = run["state"]["phase"] == 5
            continue
        # Fitting observations deliberately omit all hidden truth/evaluation.
        observation = {
            "target_A": [d, q], "role": role, **run["estimate"], "windows": run["windows"],
            "evidence": "simulation-only", "units": "SI", "convention": "amplitude-invariant-dq",
            "source_run": len(report["runs"])-1, "voltage_source": run["observations"]["source"],
            "temperature_K": None, "temperature_status": "unmeasured",
            "resistance_estimate_ohm": estimate["R_ohm"], "resistance_uncertainty_ohm": uncertainty,
            "conditional_policy": run["config"],
        }
        report["observations"].append(observation)
        report["coverage"]["accepted_points_A"].append([d, q])
        if observation["pm_reference"]:
            report["pm_reference"] = {"source_run": observation["source_run"],
                                      "flux_Wb": observation["flux_Wb"], "uncertainty_Wb": observation["uncertainty_Wb"]}
    report["complete"] = not report["coverage"]["unsupported_requests_A"] and report["pm_reference"] is not None
    report["state"] = "fault" if stopped else ("complete" if report["complete"] else "degraded")
    report["reason"] = ("point-fault" if stopped else "accepted-requested-coverage" if report["complete"]
                        else "incomplete-coverage-or-missing-pm-reference")
    return report
