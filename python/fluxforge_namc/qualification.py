"""Finite synthetic Phase 3 campaign; native C owns all control and physics.

This is regression/operating-point evidence, not a stability proof or a hardware
qualification. It deliberately includes an infeasible low-bus/high-speed case.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict
from itertools import product
import json
from pathlib import Path
import subprocess

from . import CurrentTuning, FluxMap, FluxMapLimits, run_flux_map_reference


LIMITS = FluxMapLimits(1e-5, 1e-5, 1e-6)
CRITERIA = {"tail_rms_max_A": 0.06, "settling_max_s": 0.08,
            "current_max_A": 12.0, "axis_overshoot_max_A": 1.0}


def cases():
    """Explicit deterministic grid, not Monte Carlo or an identified population."""
    for index, ((id, iq), speed, bus, scale) in enumerate(product(
        ((-2.0, 5.0), (-2.0, -5.0), (0.0, 0.0)),
        (-20.0, 0.0, 20.0), (12.0, 48.0), (0.8, 1.0, 1.2),
    )):
        yield {"name": f"grid-{index:02}", "id": id, "iq": iq,
               "initial_speed": speed, "vdc": bus, "map_scale": scale,
               "resistance": {0.8: 0.3, 1.0: 0.4, 1.2: 0.5}[scale],
               "load": 0.2 if iq > 0 else (-0.2 if iq < 0 else 0.0)}
    yield {"name": "positive-d", "id": 2.0, "iq": 3.0, "initial_speed": 20.0,
           "vdc": 48.0, "map_scale": 1.0, "resistance": 0.4, "load": 0.2}
    yield {"name": "infeasible-headroom", "id": -2.0, "iq": 5.0,
           "initial_speed": 100.0, "vdc": 12.0, "map_scale": 1.0,
           "resistance": 0.4, "load": 0.2}


def controller_data(data, scale):
    """Perturb only the synthetic fixture's affine priors, retaining cross terms.

    Constants describe the test perturbation, not identified motor parameters.
    The returned full data is independently validated and stored for replay.
    """
    copy = json.loads(json.dumps(data))
    copy["source_id"] += f"-qualification-affine-{scale}"
    copy["psi_d_Wb"] = [[v+(scale-1)*(0.05+0.003*copy["id_A"][i]) for v in row]
                        for i, row in enumerate(copy["psi_d_Wb"])]
    copy["psi_q_Wb"] = [[v+(scale-1)*0.004*copy["iq_A"][j] for j, v in enumerate(row)]
                        for row in copy["psi_q_Wb"]]
    return copy


def assess(report):
    """Predeclared finite-window engineering thresholds, never tuned to results."""
    m = report["metrics"]
    return {
        "tracking": m["tail_rms_current_error_A"] <= CRITERIA["tail_rms_max_A"],
        "settling": 0 <= m["settling_time_s"] <= CRITERIA["settling_max_s"],
        "current": m["max_current_A"] <= CRITERIA["current_max_A"],
        "overshoot": max(m["overshoot_d_A"], m["overshoot_q_A"]) <= CRITERIA["axis_overshoot_max_A"],
        "duty": 0 <= m["min_duty"] <= m["max_duty"] <= 1,
    }


def run_campaign(executable, library, fixture, *, timing=False):
    plant = FluxMap.from_json(library, fixture, limits=LIMITS)
    models = {scale: FluxMap(library, controller_data(plant.to_data(), scale), LIMITS)
              for scale in (0.8, 1.0, 1.2)}
    records = []
    observed_plant = None

    def execute(case, mode, fault=-1):
        nonlocal observed_plant
        scenario = {k: case[k] for k in ("id", "iq", "initial_speed", "vdc", "load")}
        scenario.update(steps=2000, dt=0.00005, seed=42)
        tuning = CurrentTuning(1500.0, case["resistance"], electrical_speed=4*case["initial_speed"])
        options = {"controller_map": models[case["map_scale"]] if mode != "nominal" else None,
                   "tuning": tuning if mode == "tuned" else None,
                   "model_failure": "nominal" if fault >= 0 else "disable",
                   "model_fault_step": fault, "timing": timing}
        record = {"case": dict(case), "mode": mode, "scenario": scenario,
                  "model_fault_step": fault, "model_failure": options["model_failure"],
                  "tuning_request": asdict(tuning) if mode == "tuned" else None}
        try:
            report = run_flux_map_reference(executable, plant, **scenario, **options)
        except subprocess.CalledProcessError as error:
            record.update(status="rejected-or-stopped", returncode=error.returncode, stderr=error.stderr)
        else:
            # Common full maps are retained once in the campaign. Nothing is
            # passed back from plant truth to the controller or design routine.
            actual_plant = report.pop("plant")
            if observed_plant is None:
                observed_plant = actual_plant
            elif observed_plant != actual_plant:
                raise ValueError("Campaign hidden plant changed between runs")
            report.pop("controller_model", None)
            criteria = assess(report)
            record.update(status="completed", criteria=criteria,
                          meets_criteria=all(criteria.values()), report=report)
        records.append(record)

    for case in cases():
        for mode in ("nominal", "mapped", "tuned"):
            execute(case, mode)
    for bus, speed, fault in product((12.0, 48.0), (-20.0, 20.0), (0, 200, 1000)):
        execute({"name": f"fallback-{bus}-{speed}-{fault}", "id": -2.0, "iq": 5.0,
                 "initial_speed": speed, "vdc": bus, "load": 0.2,
                 "map_scale": 1.2, "resistance": 0.5}, "tuned", fault)
    # Include failed/rejected runs in totals; never discard inconvenient cases.
    return {"schema": "namc-phase3-qualification-experimental-v1", "evidence": "simulation-only",
            "criteria": CRITERIA, "plant": observed_plant,
            "map_limits": asdict(LIMITS),
            "controller_maps": {str(k): v.to_data() for k, v in models.items()},
            "summary": {"runs": len(records),
                        "completed": sum(r["status"] == "completed" for r in records),
                        "meets_criteria": sum(r.get("meets_criteria", False) for r in records)},
            "runs": records}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--fixture", required=True, help="Synthetic nonlinear_flux_map.json fixture")
    parser.add_argument("--timing", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = run_campaign(args.executable, args.library, args.fixture, timing=args.timing)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, allow_nan=False)+"\n", encoding="utf-8")
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        parser.exit(2, f"Campaign failed: {error}\n")
    print(json.dumps(report["summary"]))
    # Completion means the campaign was recorded, not that every controller passed.
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
