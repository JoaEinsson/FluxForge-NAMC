# Local Identification: Resistance, Coupled Flux and PM Reference

This Phase 4 implementation acquires local, absolute observations of both
`FluxD(id,iq)` and `FluxQ(id,iq)` after an independently accepted
[resistance estimate](resistance-identification.md). It includes bounded native
state machines, uncertainty/observability checks, an origin PM reference and
replayable sparse datasets. It **does not fit, interpolate, correct or activate a
global map**; those are Phase 5 responsibilities.

Evidence remains synthetic software simulation. The magnetic experiment needs
an externally driven shaft at prescribed speeds, modeled as an ideal virtual
dynamometer. No physical experiment, autonomous startup, encoder alignment or
safe hardware commissioning procedure is supplied. See
[proposed ADR-0011](../adr/0011-observable-coupled-flux-point-acquisition.md).

## What is observed

For the repository's amplitude-invariant dq convention:

```text
vd = R*id + d(psi_d)/dt - omega_e*psi_q
vq = R*iq + d(psi_q)/dt + omega_e*psi_d
```

At sufficiently settled current/flux, define `yd = vd-R*id`, `yq = vq-R*iq`.
Acquire three steady windows at the same requested `(id,iq)` with electrical
speeds +80, -80 and +40 rad/s by default. The first two fit each slope/intercept:

```text
D = mean_omega[0] - mean_omega[1]
slope_axis = (mean_y_axis[0] - mean_y_axis[1]) / D
offset_axis = mean_y_axis[0] - mean_omega[0]*slope_axis
psi_d = slope_q
psi_q = -slope_d
holdout_residual_axis = mean_y_axis[2] - (mean_omega[2]*slope_axis + offset_axis)
```

No per-axis flux independence is assumed: each point supplies **both** fluxes at
**both** currents. Varying d and q requests observes cross-saturation directly.
No current differentiation, local matrix inversion or hidden flux lookup occurs
inside the identifier. The [general voltage equations](https://www.mathworks.com/help/autoblks/ref/fluxbasedpmsm.html)
are consistent with the existing nonlinear plant; the estimator schedule and
guards are project-specific.

The third speed is a validation observation, never part of the two-speed fit.
Constant voltage offsets are reported separately. Each per-axis residual and
offset must meet the policy before any candidate is published.

At requested `(0,0)`, the paired flux is the PM reference in the declared aligned
dq convention, not a separately recovered magnet geometry/material parameter.
The candidate requires `psi_d - uncertainty_d > 0` and
`abs(psi_q) + uncertainty_q <= max_origin_q_flux`. PM flux is unobservable from
the standstill DC experiment alone. No origin observation means no PM reference.

## Conditional error bounds

The caller supplies independent accepted R and absolute errors `uR,uI,uV,uW`.
Neither simulation truth nor injected noise amplitude fills these bounds.
Additional assumptions are an omitted-dynamics/alignment allowance `eModel` in
volts and a local Lipschitz bound `G` in Wb/A:

```text
abs(psi_axis(actual_current) - psi_axis(requested_current)) <= G*norm(delta_current)
```

For each window, `distance` is the maximum Euclidean current deviation from the
request over its recorded min/max box, plus `sqrt(2)*uI`. For each voltage axis:

```text
e[k] = uV + R*uI + uR*(abs(mean_current_axis[k])+uI) + eModel
       + (max_abs_speed[k]+uW)*G*distance[k]
flux_uncertainty = (e[0]+e[1]+abs(slope_axis)*2*uW) / (D-2*uW)
```

`D-2*uW` must exceed the minimum speed separation. This follows the bounded
numerator/denominator error of a ratio and includes unknown flux variation within
the current windows; using just the distance of the mean would understate it.
The whole flux interval must stay inside the configured per-axis magnitude limit
and its width below the uncertainty threshold. There is no statistical confidence
percentage, and repeated samples do not average away systematic error.

These bounds are **conditional** on calibrated sensors/reconstruction, settled
flux and the declared `eModel`/`G`. Window spans cannot independently prove zero
flux derivative or validate an unknown gradient globally. The independent holdout
is an additional check, not a proof of those assumptions.

## Native state and limits

`include/namc/flux_id.h` is experimental (no ABI promise). `namc_fi_start` copies
policy and limits, clearing prior candidates. `namc_fi_request` exposes a dq
current request and electrical shaft-speed request only while active.
`namc_fi_observe` consumes one sequential `namc_rs_observation_t` per interval.
`namc_fi_abort` invalidates the candidate and stops requests.

Phases: 0 IDLE, 1 SETTLING, 2 SAMPLING, 3 COMPLETE, 4 DEGRADED, 5 FAULT.
Each speed has a fixed settling interval then a bounded measurement window.
Terminal states latch until explicit restart; requests become zero/inactive.
The caller must stop excitation, **not continue a zero-reference PI**. This API
does not implement physical discharge, braking, independent protection or a HAL.

Reason codes: 0 none; 1 configuration; 2 observation/sequence; 3 hard limit;
4 voltage source; 5 unsettled window; 6 tracking; 7 unobservable speed/separation;
8 flux range; 9 voltage offset; 10 uncertainty; 11 held-out prediction;
12 timeout; 13 abort; 14 invalid PM reference/alignment.

| Default magnetic policy | Value |
| --- | --- |
| Sample period | 50 us |
| Electrical speeds | +80, -80, +40 rad/s |
| Settling / measurement samples per speed | 1500 / 500 |
| Total timeout | 7000 samples; nominal acquisition uses 6000 |
| Independent current / voltage / speed limits | 8 A / 25 V / 200 electrical rad/s |
| Per-point apparent-energy budget | 30 J |
| Maximum current / voltage / speed spans | 0.002 A / 0.002 V / 0.01 rad/s |
| Tracking current norm / speed error | 0.01 A / 0.1 rad/s |
| Minimum speed magnitude / separation | 5 / 10 electrical rad/s, after error allowances |
| Current / voltage / speed declared errors | 0.0001 A / 0.0001 V / 0.001 rad/s |
| Omitted-dynamics voltage allowance / gradient bound | 0.001 V / 0.02 H |
| Maximum flux magnitude / uncertainty, each axis | 0.2 Wb / 0.0001 Wb |
| Offset / held-out residual limits, each axis | 0.05 V / 0.005 V |
| Origin q-flux tolerance | 0.001 Wb, including uncertainty |

Limits check observed interval quantities, not unknowable true hardware peaks.
Apparent energy is the sampled sum of `1.5*norm(v)*norm(i)*dt`, not winding heat.
The last finite violating interval is included before stopping. Overshoot within
an interval cannot be undone. The current PI separately limits requests/duties
and guards current, bus and speed; identification cannot alter these limits.

## Simulation boundary

`namc_flux_ident_sim` owns an imported hidden coupled LUT and an independent
nominal PI. Its prior inductances/PM flux are fixed at 1.5/2 mH and 0.04 Wb;
gains are Kpd/Kpq=3/4 V/A, Kid/Kiq=600 V/(A s), kaw=500/s. The PI receives no
hidden map or resistance. The accepted R estimate goes to the identifier only,
not to automatic controller tuning or activation.

The new plant-only `namc_magnetic_driven_step` shares the nonlinear electrical
RK4 with the free-rotor plant but holds mechanical speed constant throughout all
stages. Each speed starts an independent run at zero current with a reset PI and
the same seed-derived angle. There is **no instantaneous physical shaft reversal**
or modeled acceleration/startup energy. The external shaft can supply/absorb
work; its signed work is reported only as a harness diagnostic. Free-rotor
behavior and its existing tests are unchanged.

The observable adapter uses the interval-average of actual average-inverter
terminal voltage, integrated exactly over uniform rotation of held alpha/beta
voltage. Current uses the two measured interval endpoints' trapezoid. No flux
or current derivative is read into the observation. Remaining alignment error
is part of `eModel`. Current-controller sensing is ideal; optional noise/bias
perturb only the identifier adapter, not a physical sensor/inverter model.

## Running the complete local campaign

Build as in [build instructions](build.md), then:

```powershell
$env:PYTHONPATH = "$PWD/python"
.\.venv\Scripts\python.exe -m fluxforge_namc.local_identification `
  --resistance-executable build-phase4-resistance/Debug/namc_ident_sim.exe `
  --flux-executable build-phase4-resistance/Debug/namc_flux_ident_sim.exe `
  --library build-phase4-resistance/Debug/namc_core_binding.dll `
  --map tests/fixtures/nonlinear_flux_map.json `
  --min-incremental-h 1e-5 --max-reciprocity-error-h 1e-5 --min-rcond 1e-6 `
  --plant-resistance 0.4 --seed 42 --output results/phase4-local-debug.json
```

The default plan requests nine points on d/q axes `[-4,0,4] A`, including the
origin, plus two held-out current points `(-2.3,1.7)` and `(2.1,-1.3) A`. These
are sparse acquisition locations, **not a 3x3 runtime LUT**. The held-out role
preserves independent points for later fitting; no Phase 5 fit is performed.

`run_local_identification` accepts 1..64 distinct points with explicit
`acquisition`/`held-out` roles. It first runs native resistance identification.
Only an accepted R and its conservative symmetric error are handed onward. If R
fails, all magnetic requests remain unsupported. Flux options cannot override
that estimate, hidden plant R, seed or requested points.

Each native point runs within its own fixed bounds. A DEGRADED point is retained
and the next independent experiment may run; a FAULT, timeout of the host process
or executable failure stops subsequent points, which are recorded as skipped.
No retry, imputation or relaxed thresholds are used. At most 64 points bound the
campaign; this does not establish a continuous cumulative thermal safety budget.

`complete` means every requested point was accepted and a PM reference exists;
it is **not** full motor-domain coverage, phase acceptance or hardware readiness.
A zero CLI exit means a report was written, including recorded failures. Inspect
`complete`, `state`, `reason`, and every run. Startup/import failures exit nonzero.

## Dataset and replay

Reports retain full per-run policy, map/configuration/seed/build provenance,
observed statistics, error assumptions and negative results. Hidden truth and
shaft diagnostics live in the simulation/evaluation section. `observations`
contains only accepted paired flux candidates, conditional errors, current/speed
windows, source/units/convention, requested role and run references. Temperature
is explicitly **unmeasured**, never copied from the hidden map as a measurement.

Coverage is `accepted-points-only`; `interpolation_supported` and `complete_map`
are false. A location inside the requested points' bounding box is still
unsupported if it was not acquired. No extrapolated or invented confidence is
provided. Cross-point physical consistency and global interpolation accuracy
must be assessed by Phase 5 before creating a usable runtime map.

For same-build replay, reconstruct `FluxMap` from the resistance report's
`plant.map` and `plant.map_limits`, `ResistanceExperiment` from its `experiment`,
and rerun with top-level `requests` and `flux_options`. A single-point replay
uses `FluxPointExperiment(**point_report["experiment"])` and the native flux
runner. Python contains no independent critical estimation/physics equations.

## Voltage calibration limitation

Raw command-only voltage and unknown/changing sources are rejected. Large
constant offset and the tested speed-sign-dependent error are also rejected;
a small constant offset is identified separately from flux.

A coherent gain error in terminal voltage is fundamentally ambiguous with flux,
especially at zero current. A deliberate test injecting +2% voltage gain while
falsely declaring ideal narrow error bounds yields an accepted approximately
0.051 Wb estimate for a 0.05 Wb PM fixture. This negative evidence is retained.
Declaring the corresponding 0.1 V error bound instead triggers uncertainty
rejection. Neither source labels nor residual tests replace calibration.

Thus the supported claim is identification from terminal measurements with
independently justified bounds, **not** universal identification from arbitrary
inverter command voltage. Nonideal inverter identification/reconstruction and
loss/thermal models remain Phase 6/7 work. See the full
[evidence and Phase 4 exit matrix](phase4-local-evidence.md).
