"""Generate simulation-only R, PM-reference and coupled-flux observation data."""

import argparse
import json
from pathlib import Path
import subprocess

from .flux_map import FluxMap, FluxMapLimits
from .resistance import ResistanceExperiment
from .flux_identification import run_local_identification


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resistance-executable", required=True)
    parser.add_argument("--flux-executable", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--map", required=True)
    parser.add_argument("--min-incremental-h", type=float, required=True)
    parser.add_argument("--max-reciprocity-error-h", type=float, required=True)
    parser.add_argument("--min-rcond", type=float, required=True)
    parser.add_argument("--plant-resistance", type=float, default=0.4)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        model = FluxMap.from_json(args.library, args.map, limits=FluxMapLimits(
            args.min_incremental_h, args.max_reciprocity_error_h, args.min_rcond))
        report = run_local_identification(args.resistance_executable, args.flux_executable, model,
                                         resistance_experiment=ResistanceExperiment(
                                             plant_resistance=args.plant_resistance, seed=args.seed))
        text = json.dumps(report, indent=2, allow_nan=False)+"\n"
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
        print(json.dumps({k: report[k] for k in ("state", "reason", "complete", "coverage", "pm_reference")}))
    except subprocess.CalledProcessError as error:
        parser.exit(error.returncode, error.stderr)
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        parser.exit(2, f"Acquisition rejected: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
