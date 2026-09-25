"""Host CLI for the native reference experiments; no physics in Python."""

import argparse
import json
import subprocess

from . import FluxMap, FluxMapLimits, run_flux_map_reference, run_linear_reference


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, help="Explicit path to namc_sim")
    parser.add_argument("--map", help="Experimental flux-map JSON; omit for the linear baseline")
    parser.add_argument("--compare-linear", action="store_true",
                        help="Run the linear plant too, with identical scenario/controller settings")
    parser.add_argument("--library", help="Native core library from the same build (required with --map)")
    parser.add_argument("--min-incremental-h", type=float)
    parser.add_argument("--max-reciprocity-error-h", type=float)
    parser.add_argument("--min-rcond", type=float)
    for name, value, kind in (
        ("steps", 2000, int), ("seed", 1, int), ("dt", 0.00005, float),
        ("id", 0.0, float), ("iq", 5.0, float), ("vdc", 48.0, float),
        ("load", 0.0, float), ("trace-stride", 0, int),
    ):
        parser.add_argument(f"--{name}", type=kind, default=value)
    args = parser.parse_args()
    policy = (args.min_incremental_h, args.max_reciprocity_error_h, args.min_rcond)
    if args.map and (not args.library or any(v is None for v in policy)):
        parser.error("--map requires --library and all three explicit validation thresholds")
    if not args.map and (args.library or any(v is not None for v in policy)):
        parser.error("Map library/thresholds require --map")
    if args.compare_linear and not args.map:
        parser.error("--compare-linear requires --map")
    config = {key: getattr(args, key) for key in (
        "steps", "seed", "dt", "id", "iq", "vdc", "load", "trace_stride",
    )}
    try:
        if args.map:
            model = FluxMap.from_json(args.library, args.map, limits=FluxMapLimits(*policy))
            result = run_flux_map_reference(args.executable, model, **config)
            if args.compare_linear:
                linear = run_linear_reference(args.executable, **config)
                result = {
                    "schema": "namc-plant-comparison-experimental-v1",
                    "evidence": "simulation-only",
                    "comparison": "same nominal PI and scenario; different hidden magnetic plants",
                    "metric_delta_lut_minus_linear": {
                        key: value - linear["metrics"][key] for key, value in result["metrics"].items()
                    },
                    "linear": linear, "coupled": result,
                }
        else:
            result = run_linear_reference(args.executable, **config)
    except subprocess.CalledProcessError as error:
        parser.exit(error.returncode, error.stderr)
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        parser.exit(2, f"Experiment rejected: {error}\n")
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
