"""Run the simulation-only native stator-resistance identification experiment."""

import argparse
from dataclasses import fields
import json
from pathlib import Path
import subprocess

from .flux_map import FluxMap, FluxMapLimits
from .resistance import ResistanceExperiment, run_resistance_identification


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, help="Explicit path to namc_ident_sim")
    parser.add_argument("--library", required=True)
    parser.add_argument("--map", required=True, help="Hidden synthetic plant map, not identifier input")
    parser.add_argument("--min-incremental-h", type=float, required=True)
    parser.add_argument("--max-reciprocity-error-h", type=float, required=True)
    parser.add_argument("--min-rcond", type=float, required=True)
    parser.add_argument("--output", type=Path, help="Optional JSON report file")
    for field in fields(ResistanceExperiment):
        parser.add_argument(f"--{field.name.replace('_', '-')}", type=field.type, default=field.default)
    args = parser.parse_args()
    try:
        model = FluxMap.from_json(args.library, args.map, limits=FluxMapLimits(
            args.min_incremental_h, args.max_reciprocity_error_h, args.min_rcond))
        experiment = ResistanceExperiment(**{f.name: getattr(args, f.name) for f in fields(ResistanceExperiment)})
        report = run_resistance_identification(args.executable, model, experiment=experiment)
        text = json.dumps(report, indent=2, allow_nan=False)+"\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(text, encoding="utf-8")
            print(json.dumps({k: report[k] for k in ("accepted", "state", "estimate")}))
        else:
            print(text, end="")
    except subprocess.CalledProcessError as error:
        parser.exit(error.returncode, error.stderr)
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        parser.exit(2, f"Experiment rejected: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
