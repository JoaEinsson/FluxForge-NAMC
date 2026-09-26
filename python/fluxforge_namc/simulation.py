"""Run the experimental C simulator; no motor equations are implemented here."""

from __future__ import annotations

import json
import subprocess
from dataclasses import dataclass, fields
from pathlib import Path
from typing import Any

from .flux_map import FluxMap


@dataclass(frozen=True)
class CurrentTuning:
    """Offline C gain-design request, not identified motor parameters.

    Bandwidths and electrical_speed are rad/s; resistance ohm; design_error A;
    max_kp V/A and max_ki V/(A s). C validates every value. The runner designs
    at its reference currents and bus voltage, before starting a reset loop.
    Limits do not establish achieved bandwidth or closed-loop stability.
    """

    bandwidth: float
    resistance: float
    min_bandwidth: float = 10.0
    max_bandwidth: float = 2000.0
    max_rate_sample: float = 0.2
    max_kp: float = 10.0
    max_ki: float = 2000.0
    design_error: float = 1.0
    voltage_fraction: float = 0.5
    electrical_speed: float = 0.0

    def _arguments(self):
        names = {"design_error": "error", "electrical_speed": "speed"}
        arguments = []
        for field in fields(self):
            name = names.get(field.name, field.name).replace("_", "-")
            arguments.extend((f"--tune-{name}", str(getattr(self, field.name))))
        return arguments


def _run_reference(
    executable: str | Path,
    *,
    steps: int = 2000,
    seed: int = 1,
    dt: float = 0.00005,
    id: float = 0.0,
    iq: float = 5.0,
    vdc: float = 48.0,
    load: float = 0.0,
    trace_stride: int = 0,
    plant: str = "linear",
    map_input: str | None = None,
    controller: str = "nominal",
    model_failure: str = "disable",
    tuning: CurrentTuning | None = None,
) -> dict[str, Any]:
    """Return the C experiment's JSON report, or raise on a rejected/failed run.

    Requires an explicit executable path. Input domains are enforced by C.
    The timeout is a host orchestration safeguard, not a real-time guarantee.
    Report layout and this interface have no compatibility promise.
    """
    command = [str(Path(executable).resolve()), "--plant", plant,
               "--controller", controller, "--model-failure", model_failure]
    for name, value in (
        ("steps", steps), ("seed", seed), ("dt", dt),
        ("id", id), ("iq", iq), ("vdc", vdc), ("load", load),
        ("trace-stride", trace_stride),
    ):
        command.extend((f"--{name}", str(value)))
    if tuning is not None:
        command.extend(tuning._arguments())
    result = subprocess.run(
        command, check=True, capture_output=True, text=True, encoding="utf-8", timeout=60,
        input=map_input,
    )
    return json.loads(result.stdout)


def run_linear_reference(
    executable: str | Path, *, steps: int = 2000, seed: int = 1,
    dt: float = 0.00005, id: float = 0.0, iq: float = 5.0,
    vdc: float = 48.0, load: float = 0.0, trace_stride: int = 0,
) -> dict[str, Any]:
    """Run the native linear baseline, optionally retaining decimated samples.

    Inputs use SI units. C enforces bounds; failed runs raise CalledProcessError
    without a success report. The default is fixed nominal PI with independent
    limits. No stability/API promise is implied.
    """
    return _run_reference(
        executable, steps=steps, seed=seed, dt=dt, id=id, iq=iq,
        vdc=vdc, load=load, trace_stride=trace_stride,
    )


def run_flux_map_reference(
    executable: str | Path, flux_map: FluxMap, *, steps: int = 2000, seed: int = 1,
    dt: float = 0.00005, id: float = 0.0, iq: float = 5.0,
    vdc: float = 48.0, load: float = 0.0, trace_stride: int = 0,
    controller_map: FluxMap | None = None, model_failure: str = "disable",
    tuning: CurrentTuning | None = None,
) -> dict[str, Any]:
    """Run a coupled-map hidden plant, using the same C loop as the baseline.

    The report embeds the full accepted map and thresholds for replay. The
    simulation evidence stays simulation-only regardless of map provenance.
    Plant map parameters never enter the controller. Supplying controller_map
    explicitly selects flux-based PI using a separate native map allocation.
    The default remains nominal PI. Use library/executable from the same build;
    the executable independently validates both received copies.
    """
    result = _run_reference(
        executable, steps=steps, seed=seed, dt=dt, id=id, iq=iq, vdc=vdc,
        load=load, trace_stride=trace_stride, plant="lut",
        map_input=flux_map._simulation_transport() + (
            controller_map._simulation_transport() if controller_map is not None else ""
        ),
        controller="flux-map" if controller_map is not None else "nominal",
        model_failure=model_failure,
        tuning=tuning,
    )
    data = result["plant"]["map"]
    data["source_id"] = bytes.fromhex(data.pop("source_id_utf8_hex")).decode("utf-8")
    if "controller_model" in result:
        data = result["controller_model"]["map"]
        data["source_id"] = bytes.fromhex(data.pop("source_id_utf8_hex")).decode("utf-8")
    return result
