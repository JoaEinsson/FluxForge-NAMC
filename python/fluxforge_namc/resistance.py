"""Orchestrate native resistance identification; no estimator equations here."""

from dataclasses import asdict, dataclass, fields
import json
from pathlib import Path
import subprocess

from .flux_map import FluxMap


@dataclass(frozen=True)
class ResistanceExperiment:
    """Synthetic harness inputs, SI. Plant resistance is evaluation truth ONLY.

    Noise/bias are estimator-adapter fault injections, not a complete sensor or
    inverter model. Declared uncertainty is independent and never inferred from
    the injected noise amplitude. All input domains are checked by native C.
    """

    plant_resistance: float = 0.4
    vdc: float = 48.0
    initial_speed: float = 0.0  # mechanical rad/s
    dt: float = 0.00005
    seed: int = 42
    settle_samples: int = 1000
    measure_samples: int = 500
    max_samples: int = 5000
    energy_limit: float = 1.0
    current_noise: float = 0.0
    voltage_noise: float = 0.0
    voltage_bias: float = 0.0
    voltage_signed_error: float = 0.0
    current_uncertainty: float = 0.0001
    voltage_uncertainty: float = 0.0001
    voltage_source: int = 1  # ideal terminal adapter; 0 exercises rejection of commands

    def arguments(self):
        return [part for f in fields(self)
                for part in (f"--{f.name.replace('_', '-')}", str(getattr(self, f.name)))]


def run_resistance_identification(executable: str | Path, plant_map: FluxMap, *,
                                  experiment: ResistanceExperiment = ResistanceExperiment()):
    """Return a completed experiment, which may contain a rejected estimate.

    Inspect `accepted` and `state.reason`; a successful process does not imply
    accepted identification. Startup/plant/controller faults raise instead.
    Use executable/library from the same build. No map enters the identifier.
    """
    result = subprocess.run([str(Path(executable).resolve()), *experiment.arguments()],
                            input=plant_map._simulation_transport(), check=True,
                            text=True, encoding="utf-8", capture_output=True, timeout=60)
    report = json.loads(result.stdout)
    data = report["plant"]["map"]
    data["source_id"] = bytes.fromhex(data.pop("source_id_utf8_hex")).decode("utf-8")
    report["experiment"] = asdict(experiment)
    return report
