"""Host CLI for the native reference experiments; no physics in Python."""

import argparse
import json
import subprocess

from . import CurrentTuning, FluxMap, FluxMapLimits, run_flux_map_reference, run_linear_reference


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
    parser.add_argument("--controller-map", help="Separate accepted controller map JSON")
    parser.add_argument("--control-min-incremental-h", type=float)
    parser.add_argument("--control-max-reciprocity-error-h", type=float)
    parser.add_argument("--control-min-rcond", type=float)
    parser.add_argument("--model-failure", choices=("disable", "nominal"), default="disable")
    parser.add_argument("--compare-controllers", action="store_true",
                        help="Compare nominal and flux-based PI on the same hidden plant")
    for name in ("bandwidth", "resistance", "min-bandwidth", "max-bandwidth", "max-rate-sample",
                 "max-kp", "max-ki", "error", "voltage-fraction", "speed"):
        parser.add_argument(f"--tune-{name}", type=float,
                            help="Offline PI design input (SI units; bandwidth/speed in rad/s)")
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
    control_policy = (args.control_min_incremental_h, args.control_max_reciprocity_error_h,
                      args.control_min_rcond)
    if args.controller_map:
        if not args.map or any(v is None for v in control_policy):
            parser.error("--controller-map requires --map and three separate control validation thresholds")
        if args.compare_linear:
            parser.error("--compare-linear cannot be combined with --controller-map; use --compare-controllers")
    elif (args.compare_controllers or args.model_failure != "disable" or
          any(v is not None for v in control_policy)):
        parser.error("Control comparison, policy and thresholds require --controller-map")
    tuning_values = {name.removeprefix("tune_"): value for name, value in vars(args).items()
                     if name.startswith("tune_") and value is not None}
    tuning = None
    if tuning_values:
        if (not args.controller_map or args.model_failure != "disable" or
                "bandwidth" not in tuning_values or "resistance" not in tuning_values):
            parser.error("Tuning requires --controller-map, --tune-bandwidth, --tune-resistance and disable policy")
        for short, long in (("error", "design_error"), ("speed", "electrical_speed")):
            if short in tuning_values:
                tuning_values[long] = tuning_values.pop(short)
        tuning = CurrentTuning(**tuning_values)
    config = {key: getattr(args, key) for key in (
        "steps", "seed", "dt", "id", "iq", "vdc", "load", "trace_stride",
    )}
    try:
        if args.map:
            model = FluxMap.from_json(args.library, args.map, limits=FluxMapLimits(*policy))
            controller_map = None
            if args.controller_map:
                controller_map = FluxMap.from_json(
                    args.library, args.controller_map, limits=FluxMapLimits(*control_policy),
                )
            result = run_flux_map_reference(args.executable, model, **config,
                                           controller_map=controller_map, model_failure=args.model_failure,
                                           tuning=tuning)
            if args.compare_controllers:
                tuned_result = result if tuning is not None else None
                if tuning is not None:
                    result = run_flux_map_reference(args.executable, model, **config,
                                                   controller_map=controller_map)
                baseline = run_flux_map_reference(args.executable, model, **config)
                result = {
                    "schema": "namc-controller-comparison-experimental-v1",
                    "evidence": "simulation-only",
                    "comparison": "same hidden plant, PI gains, limits and scenario; different compensation",
                    "metric_delta_model_minus_nominal": {
                        key: value - baseline["metrics"][key] for key, value in result["metrics"].items()
                    },
                    "nominal": baseline, "model_aware": result,
                }
                if tuned_result is not None:
                    result["schema"] = "namc-tuning-comparison-experimental-v1"
                    result["comparison"] = (
                        "same hidden plant, limits and scenario; nominal PI, fixed-gain flux PI, tuned flux PI"
                    )
                    result["tuned_model_aware"] = tuned_result
                    result["metric_delta_tuned_minus_fixed"] = {
                        key: value - result["model_aware"]["metrics"][key]
                        for key, value in tuned_result["metrics"].items()
                    }
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
