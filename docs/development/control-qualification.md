# Phase 3 Reserve-Gain and Operating-Point Qualification

This increment implements separately configured reserve gains for tuned
flux-map PI and a reproducible finite simulation campaign. It does not provide
motor identification, online gain scheduling, hardware readiness or a universal
fallback/stability guarantee. See [ADR-0009](../adr/0009-separate-reserve-gains-and-qualify-transitions.md)
and [executed evidence](phase3-qualification-evidence.md).

## Separate gains and deterministic transition

The experimental `namc_model_current_config_t` now uses
`NAMC_MODEL_CURRENT_VERSION == 2`. Rebuild callers and initialize the new
`mapped_gains` field explicitly to NULL or a valid immutable
`namc_current_gains_t` with caller-owned lifetime. There is no ABI compatibility
promise. A NULL pointer preserves fixed gains for both modes.

`nominal` remains the independent reserve configuration, including hard limits,
magnetic priors, antiwindup and sample time. Mapped operation overrides only
Kpd/Kpq/Kid/Kiq in a local copy. An offline tuning candidate is placed in
`mapped_gains`, never in `nominal`. Reserve values in the simulator remain
Kpd=3, Kpq=4 V/A, Kid=Kiq=600 V/(A s), kaw=500/s; the nominal magnetic
priors remain Ld=1.5 mH, Lq=2 mH and PM flux=0.04 Wb.

Disable/latch is still default. Explicit `--model-failure nominal` permits
tuned operation to enter the reserve path on a failed model lookup. The first
transition resets both integrals once and immediately uses the reserve gains.
Subsequent samples retain their integrals and do not retry the map; explicit
reset is required after resolving the cause. A gain/configuration/sensor/state
fault or current/bus/speed violation cannot be bypassed through fallback.
Malformed maps and infeasible tuning requests are rejected before activation.

Resetting integrals is deliberately **not bumpless**. The measured command jump
can matter physically. The software bounds duty/voltage; it does not establish
a safe torque transient for hardware. Reserve qualification is conditional on
the particular synthetic scenarios and thresholds below.

## New experiment controls

| Python keyword | Native option | Meaning |
| --- | --- | --- |
| `initial_speed` | `--initial-speed` | Initial **mechanical** rad/s, default 0, range -500..500; not a speed command |
| `model_fault_step` | `--model-fault-step` | Zero-based sample where the controller map handle becomes NULL; default -1 disables injection |
| `timing` | `--timing 0\|1` | Optional host timing; Python CLI uses flag `--timing` |

Model-loss injection requires an explicit controller map, affects no plant
data, and persists for the remaining run. It cannot fall outside `0..steps-1`
unless disabled with -1. Existing out-of-domain lookup tests remain separate.
`--tune-speed` is an independent **electrical** design speed; it is not silently
replaced by initial speed. The campaign explicitly sets it to four times the
scenario's initial mechanical speed using the independent encoder configuration.

Example after building in `build-phase3-validation`:

```powershell
$env:PYTHONPATH = "$PWD/python"
.\.venv\Scripts\python.exe -m fluxforge_namc `
  --executable build-phase3-validation/Release/namc_sim.exe `
  --library build-phase3-validation/Release/namc_core_binding.dll `
  --map tests/fixtures/nonlinear_flux_map.json `
  --controller-map tests/fixtures/nonlinear_flux_map.json `
  --min-incremental-h 1e-5 --max-reciprocity-error-h 1e-5 --min-rcond 1e-6 `
  --control-min-incremental-h 1e-5 --control-max-reciprocity-error-h 1e-5 --control-min-rcond 1e-6 `
  --id -2 --iq 5 --vdc 48 --load 0.2 --seed 42 --initial-speed 20 `
  --tune-bandwidth 1500 --tune-resistance 0.4 --tune-speed 80 `
  --model-failure nominal --model-fault-step 200 --trace-stride 1 --timing
```

`--compare-controllers` applies the same loss time/policy to both mapped arms;
the nominal arm has no map to lose. All arms keep the same hidden plant and
initial state. Failure aborts that interactive comparison with no success
report; the campaign below instead records and continues past failed runs.

## Metrics and replay

The C simulator evaluates every integrated sample, not just retained traces:

- `peak_current_error_A`: maximum dq error norm including the zero-current
  initial state.
- `overshoot_d_A` / `overshoot_q_A`: excursion past each target in the direction
  of the initial zero-to-target step; at zero target, maximum absolute current.
  Opposite-direction excursions are represented by peak error, not overshoot.
- `settling_band_A`: `max(0.02 A, 0.02*norm(reference))`.
- `settling_time_s`: first sampled time after the last sample outside that
  vector-error band, counting the initial state. Zero means always inside;
  -1 means the final sample remains outside. Remaining inside until the end of
  this finite record is not a minimum dwell guarantee or a stability proof.
- `fallback_peak_error_A`: maximum error from the pre-transition measurement
  through the end; zero if no fallback.
- `fallback_transition_voltage_jump_V`: norm of the alpha/beta voltage-command
  difference between first fallback and previous sample. For a sample-0 fault,
  the preceding command is defined as zero. Zero if no fallback.
- Existing `voltage_limit_steps`, `fallback_steps`, full/tail RMS and duty/current
  extrema are retained. Tail RMS uses the second half of the entire run: a late
  fault's recovery transient can fail that criterion despite later settling.

`controller` records mapped gains when tuned; `fallback_gains` separately
records reserve gains when fallback is enabled. They share the recorded limits,
sample time, antiwindup and nominal priors. Restore both recorded maps/thresholds,
the original scenario, `initial.speed_rad_s`, `model_fault_step`, failure policy
and `CurrentTuning(**tuning.request)` for replay. Timing is opt-in; remove its
object when comparing physical outputs. Default same-binary replay is exact.

Timing reports samples/min/mean/max in seconds for nominal, mapped, transition
and latched fallback paths. A path with no samples has no time estimate.
`seconds_per_tick` describes timestamp units, **not measured resolution**.
Dispatch/timer overhead is not subtracted; OS preemption is included. These are
host observations, not an embedded deadline guarantee, target WCET or interrupt
latency measurements. Failed runs do not emit partial timing success reports.

## Finite campaign and acceptance criteria

```powershell
.\.venv\Scripts\python.exe -m fluxforge_namc.qualification `
  --executable build-phase3-validation/Release/namc_sim.exe `
  --library build-phase3-validation/Release/namc_core_binding.dll `
  --fixture tests/fixtures/nonlinear_flux_map.json `
  --timing --output results/phase3-qualification-release.json
```

The project-original synthetic fixture is the intended input. This is not a
generic motor-qualification tool. The 180 runs use seed 42, 2,000 samples at
50 us, zero initial currents, and:

- 54 grid scenarios: dq targets (-2,5), (-2,-5), (0,0) A; initial mechanical
  speed -20/0/20 rad/s; bus 12/48 V; affine-prior scale 0.8/1/1.2 paired with
  independently supplied R estimates 0.3/0.4/0.5 ohm. Each uses nominal, fixed
  mapped and tuned mapped PI. Signed loads are +0.2/-0.2/0 N m.
- Positive-d case (2,3) A at 20 rad/s, 48 V and 0.2 N m, all three modes.
- Intentionally infeasible (-2,5) A, 100 rad/s, 12 V case, all three modes.
- 12 tuned model-loss cases: bus 12/48 V, speed -20/20 rad/s and fault sample
  0/200/1000, target (-2,5) A, load 0.2 N m, scale 1.2 and R estimate 0.5 ohm.

Map mismatch adds `(scale-1)*(0.05+0.003*id)` Wb to psi_d and
`(scale-1)*0.004*iq` Wb to psi_q. Both nonlinear cross dependencies and acceptance
thresholds are preserved. This controlled affine perturbation is not a full
uncertainty distribution; paired map/R errors do not cover their full Cartesian
product. The hidden plant remains identical across all runs.

Predeclared engineering acceptance criteria: tail RMS <=0.06 A, settling in
0..0.08 s, current norm <=12 A, per-axis overshoot <=1 A, all duties in [0,1].
These are regression thresholds for the stated fixture, not physical safety
certification. All must pass to mark a completed scenario `meets_criteria`.
The campaign preserves each rejection/stop with return code and diagnostic;
failed or unmet cases stay in totals. CLI exit 0 means the campaign was recorded,
**not that every case passed**. Read the summary and per-case criteria.

Full accepted maps/thresholds are stored once, plus observed plant parameters,
per-run inputs, build provenance and results. Reports under `results/` are
generated/ignored; retain the source patch and evidence summary for reproduction.
No integration/controller equations are independently reimplemented in Python.
