"""Run the experimental C simulator; no motor equations are implemented here."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Any

from .flux_map import FluxMap


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
) -> dict[str, Any]:
    """Return the C experiment's JSON report, or raise on a rejected/failed run.

    Requires an explicit executable path. Input domains are enforced by C.
    The timeout is a host orchestration safeguard, not a real-time guarantee.
    Report layout and this interface have no compatibility promise.
    """
    command = [str(Path(executable).resolve()), "--plant", plant]
    for name, value in (
        ("steps", steps), ("seed", seed), ("dt", dt),
        ("id", id), ("iq", iq), ("vdc", vdc), ("load", load),
        ("trace-stride", trace_stride),
    ):
        command.extend((f"--{name}", str(value)))
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
    without a success report. Both reference runners use the same fixed nominal
    PI controller and independent limits. No stability/API promise is implied.
    """
    return _run_reference(
        executable, steps=steps, seed=seed, dt=dt, id=id, iq=iq,
        vdc=vdc, load=load, trace_stride=trace_stride,
    )


def run_flux_map_reference(
    executable: str | Path, flux_map: FluxMap, *, steps: int = 2000, seed: int = 1,
    dt: float = 0.00005, id: float = 0.0, iq: float = 5.0,
    vdc: float = 48.0, load: float = 0.0, trace_stride: int = 0,
) -> dict[str, Any]:
    """Run a coupled-map hidden plant, using the same C loop as the baseline.

    The report embeds the full accepted map and thresholds for replay. The
    simulation evidence stays simulation-only regardless of map provenance.
    No map parameters enter the controller. Use library/executable from the
    same build; the executable independently validates its received copy.
    """
    result = _run_reference(
        executable, steps=steps, seed=seed, dt=dt, id=id, iq=iq, vdc=vdc,
        load=load, trace_stride=trace_stride, plant="lut",
        map_input=flux_map._simulation_transport(),
    )
    data = result["plant"]["map"]
    data["source_id"] = bytes.fromhex(data.pop("source_id_utf8_hex")).decode("utf-8")
    return result
