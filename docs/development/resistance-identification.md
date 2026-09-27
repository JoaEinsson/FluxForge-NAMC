# Local Identification: Stator Resistance

The first Phase 4 slice estimates a **balanced equivalent stator resistance**
from observed currents and terminal voltages in a restricted near-standstill
simulation. It does not estimate PM flux, coupled magnetic response, full flux
tables, temperature dependence or inverter calibration. Earlier phase limitations
remain in force; starting this slice does not declare those phases complete.
See [ADR-0010](../adr/0010-bounded-standstill-resistance-identification.md).

## Method and observability

In the existing amplitude-invariant dq convention, the coupled plant follows
`v = R*i + d(psi)/dt + [-omega_e*psi_q, omega_e*psi_d]`. At standstill with
settled currents/flux, the inductive and rotational terms vanish regardless of
the nonlinear map shape. Fit the two positive plateaus using measured means:

```text
di = mean_id[1] - mean_id[0]
dv = mean_vd[1] - mean_vd[0]
R = dv / di
b = mean_vd[0] - R * mean_id[0]
holdout_residual = mean_vd[2] - (R * mean_id[2] + b)
```

The negative third plateau is held out from fitting. A consistent constant
voltage offset is represented by `b`, not silently added to R; excessive offset
or a failed third-point prediction rejects the estimate. Q current/voltage must
remain small and encoder speed inside the independent bound. There is no claim
that d-axis excitation guarantees no torque for every imported map; measured
motion is guarded, not forcibly suppressed in the plant.

Given caller-declared absolute current/voltage errors `uI,uV`, require
`di - 2*uI > min_current_separation`, then report:

```text
lower = (dv - 2*uV) / (di + 2*uI)
upper = (dv + 2*uV) / (di - 2*uI)
uncertainty = max(R - lower, upper - R)
```

The whole interval must lie inside the configured resistance range and its
uncertainty below the configured threshold. These are conditional measurement
bounds, not a probability/confidence percentage or an allowance for arbitrary
plant-model error. Samples do not average away a declared systematic error.

Important limitations:

- Low window spans do not prove zero flux derivative without additional model
  information. A slow transient or thermal drift can remain unobservable.
- A coherent voltage/current calibration gain error can pass all three levels
  while scaling the estimated R. Calibration/reconstruction error bounds must
  be independently justified; a source label cannot verify their truth.
- The fixed d-axis/encoder convention and equivalent balanced-winding assumption
  do not establish a physical line-to-line resistance conversion for every motor.
- Only the sampled low-current points are covered. No cross-saturation table,
  global magnetic coverage or PM-flux anchor can be inferred from this experiment.

## Native state machine and authority

The experimental API is `include/namc/resistance_id.h` (version 1, no ABI promise).
`namc_rs_start` copies configuration/limits and clears previous results.
`namc_rs_request` returns a bounded dq current request, with q fixed to zero,
only during settling/sampling. The caller routes this through its independent
controller and actuator protection. A return of zero requires **actuator disable**,
not continued PI regulation at zero reference.

The caller supplies one `namc_rs_observation_t` after each aligned sample interval,
numbered exactly 1,2,... . It contains measured dq current, applied terminal
voltage, electrical speed and voltage provenance. No map/plant parameters enter
this interface. Runtime work uses fixed-size fields/three windows and no heap,
blocking I/O, differentiation or unbounded loop.

| Phase value | Meaning |
| --- | --- |
| 0 — IDLE | Unstarted storage; call start before observation |
| 1 — SETTLING | Request the current level, discard estimator samples |
| 2 — SAMPLING | Accumulate means/ranges after quality checks |
| 3 — COMPLETE | All three levels passed; result published, excitation off |
| 4 — DEGRADED | Insufficient/unsupported data; no estimate, excitation off |
| 5 — FAULT | Invalid input, hard-limit violation or abort; excitation off |

Each of three stages settles then samples. Any terminal state is sticky until
explicit restart. `namc_rs_abort` invalidates the result and turns requests off.
The API has no authority to change current/voltage limits, activate R in tuning,
update flux maps or clear controller faults. COMPLETE is data acceptance only.

Reason codes are: 0 none, 1 bad configuration, 2 invalid observation/sequence,
3 limit, 4 untrusted/changed voltage source, 5 unsettled window, 6 tracking/q-axis
condition, 7 insufficient excitation, 8 R range, 9 offset, 10 uncertainty,
11 holdout validation, 12 timeout, 13 explicit abort.

The energy diagnostic is the sampled integral of `1.5*norm(v_dq)*norm(i_dq)`.
It is a conservative apparent-power proxy at the observed samples, not measured
winding heat. The violating interval is recorded before stopping subsequent
requests: limits are not an instantaneous hardware cutoff, and one interval can
already have exceeded a budget. Invalid/untrusted observations cannot supply a
meaningful energy update. No shutdown/current-decay transient is modeled.

## Native experiment and defaults

`namc_ident_sim` connects the identifier to an independent nominal current PI,
average inverter, ideal observable-signal adapters and the existing coupled LUT
plant. The rotor is free, not artificially locked. The fixture used here remains
near standstill at q=0. Voltage comes from the actual average-inverter output,
not the PI's pre-limit voltage request. End-of-interval current and rotor-frame
voltage are used after settling; this alignment assumes negligible motion.

Defaults are experimental policy, not recommendations for energizing hardware:

| Setting | Default |
| --- | --- |
| d-current levels / q reference | +1, +2, -1 A / 0 A |
| Sample period | 50 us |
| Settling / measurement per level | 1000 / 500 samples |
| Overall timeout | 5000 samples |
| Observed current / voltage / electrical speed limits | 3 A / 12 V / 0.5 rad/s |
| Sampled apparent-energy budget | 1 J |
| Minimum current separation | 0.25 A after uncertainty allowance |
| Current / voltage window spans | 0.005 A / 0.005 V |
| Tracking / q current / q voltage tolerances | 0.05 A / 0.02 A / 0.02 V |
| Accepted R range | 0.05..2 ohm, including measurement interval |
| Maximum absolute fitted offset / holdout residual | 0.05 V / 0.005 V |
| Current / voltage declared errors | 0.0001 A / 0.0001 V |
| Maximum R uncertainty | 0.01 ohm |

The controller independently uses Kpd=3, Kpq=4 V/A, Kid=Kiq=600 V/(A s),
kaw=500/s, 1.5/2 mH nominal priors and 0.04 Wb nominal PM flux. It sees no plant
map or R. Its hard current limit is 3 A, bus range 12..60 V and electrical-speed
limit 2000 rad/s; the identifier imposes the additional experimental restrictions.

## Run and replay

Build `build-phase4-resistance` with the [standard options](build.md), then:

```powershell
$env:PYTHONPATH = "$PWD/python"
.\.venv\Scripts\python.exe -m fluxforge_namc.identify `
  --executable build-phase4-resistance/Debug/namc_ident_sim.exe `
  --library build-phase4-resistance/Debug/namc_core_binding.dll `
  --map tests/fixtures/nonlinear_flux_map.json `
  --min-incremental-h 1e-5 --max-reciprocity-error-h 1e-5 --min-rcond 1e-6 `
  --plant-resistance 0.4 --seed 42 `
  --output results/phase4-resistance-debug.json
```

`--plant-resistance` configures hidden simulation truth, **not an estimate input**.
The identifier never reads it. `truth_error_ohm` is calculated only by the harness
after identification has terminated. No dataset is transported to the identifier;
the map is private to the nonlinear plant.

The Python API uses `ResistanceExperiment` and `run_resistance_identification`.
All estimator math remains C. Reports retain the complete plant map/thresholds,
observed means/ranges, measurement source, experiment inputs, budgets, controller
policy, conditional bounds, seed and build provenance. Reconstruct the plant
`FluxMap` from `report.plant.map` / `map_limits` and the request using
`ResistanceExperiment(**report["experiment"])` for same-binary replay.

Optional native/Python CLI inputs also include `--vdc`, `--initial-speed`
(mechanical rad/s), `--dt`, `--settle-samples`, `--measure-samples`, `--max-samples`,
`--energy-limit`, `--current-uncertainty` and `--voltage-uncertainty`.
Estimator-observation-only fault injection uses `--current-noise`,
`--voltage-noise`, `--voltage-bias` and `--voltage-signed-error`; it does not alter
the current-controller measurements or implement a physical nonideal inverter.
Noise is bounded uniform LCG output; the seed also sets initial angle. Declared
uncertainty is deliberately not populated from injected truth/noise amplitude.

The harness uses `--voltage-source 1` (ideal simulation terminal adapter);
`--voltage-source 0` demonstrates rejection of command-only voltage. The native
API additionally recognizes measured/calibrated-reconstructed terminal voltage
labels for future adapters; no such hardware adapter is supplied or validated.

A process exit of zero means the experiment was recorded. Check `accepted`:
degraded/fault results have `estimate: null` and explicit phase/reason. Startup
configuration, plant or current-controller failures abort with nonzero exit
and no success report. Empty measurement windows have `samples: 0`; their zero
means/ranges are placeholders, not observed values.

See [executed resistance evidence](phase4-resistance-evidence.md). The
[local acquisition follow-up](local-identification.md) now consumes accepted R
to acquire coupled-flux observations and PM references with explicit coverage.
Global fitting/correction and adaptive activation remain Phase 5 work.
