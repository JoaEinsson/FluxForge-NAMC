"""Run the experimental C simulator; no motor equations are implemented here."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Any


def run_linear_reference(
    executable: str | Path,
    *,
    steps: int = 2000,
    seed: int = 1,
    dt: float = 0.00005,
    iq: float = 5.0,
    vdc: float = 48.0,
    load: float = 0.0,
) -> dict[str, Any]:
    """Return the C experiment's JSON report, or raise on a rejected/failed run.

    Requires an explicit executable path. Input domains are enforced by C.
    The timeout is a host orchestration safeguard, not a real-time guarantee.
    Report layout and this interface have no compatibility promise.
    """
    command = [str(Path(executable).resolve())]
    for name, value in (
        ("steps", steps), ("seed", seed), ("dt", dt),
        ("iq", iq), ("vdc", vdc), ("load", load),
    ):
        command.extend((f"--{name}", str(value)))
    result = subprocess.run(
        command, check=True, capture_output=True, text=True, timeout=60,
    )
    return json.loads(result.stdout)
