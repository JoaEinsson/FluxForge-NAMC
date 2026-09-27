# ADR-0011: Observable Coupled Flux-Point Acquisition

- Status: Proposed
- Date: 2026-09-26
- Decision owners: Lead Maintainer
- Related: [ADR-0004](0004-use-coupled-flux-linkage-maps.md),
  [ADR-0005](0005-linear-reference-conventions.md),
  [ADR-0010](0010-bounded-standstill-resistance-identification.md),
  [Phase 4](../../ROADMAP.md)

## Context

Resistance identification does not provide magnetic flux references or observed
cross-saturation. Phase 4 needs usable paired flux observations at sampled current
points, including a zero-current PM reference, without exposing hidden plant
parameters to the identifier. Global LUT fitting/activation belongs to Phase 5.

## Decision

- Acquire `psi_d` and `psi_q` directly from the steady rotating dq voltage
  equations at each requested `(id,iq)`. No constant-inductance magnetic model
  or mandatory local matrix replaces the coupled maps.
- Fit the two flux components using two opposite electrical speeds at the same
  current point. Estimate a separate constant dq voltage intercept. Test the
  prediction at a third, distinct speed not used for fitting.
- Use only interval-aligned current, terminal voltage, encoder speed, sequence,
  voltage provenance and an independently accepted R estimate/error bound.
  Raw command-only voltage is unsupported. No map/plant pointer enters the API.
- Implement bounded settling/sampling/complete/degraded/fault states in portable
  C with copied policy and independent limits. Current and shaft-speed outputs
  are requests, not commands with authority over protection. No automatic retry.
- Declare calibration, speed, resistance, omitted-dynamics/alignment and local
  flux-gradient bounds explicitly. Report conditional flux-error bounds and raw
  window coverage, not an unqualified confidence percentage. Reject candidates
  with excessive uncertainty, offset, residual, invalid data or inadequate motion.
- The native simulation represents independent ideal externally driven shaft
  runs. Preserve nonlinear electrical RK4 and rotor-angle evolution; constrain
  shaft speed instead of simulating acceleration/startup/reversal. This is a
  virtual dynamometer condition, not an on-vehicle commissioning procedure.
- At zero requested current, publish a PM reference only when d flux remains
  positive under its error bound and q flux is consistent with the configured
  alignment tolerance. Do not zero an observed q component by construction.
- Python orchestrates native R acquisition followed by native flux-point runs.
  Retain every rejected/skipped attempt; a fault stops subsequent experiments.
  Export separate fitting observations without truth fields and explicit sparse
  coverage. Do not manufacture unobserved points, fit a table or activate a model.

## Alternatives and consequences

Standstill integration yields flux differences but requires an absolute reference
and careful offset/drift handling. Signal injection can estimate local response,
but adds bandwidth/derivative and observability choices. The present rotating
method supplies absolute paired flux observations without differentiating noisy
current or committing to one global fitting method. It requires external motion
and settled conditions; it does not solve standstill PM observability.

Opposite speeds separate a constant voltage intercept from flux. The independent
third speed detects some speed-dependent voltage errors. Neither can distinguish
a coherent voltage gain error from flux without independent calibration. Tests
retain a deliberately false-calibration counterexample: it passes residual gates
with a biased estimate. Honest error bounds instead widen/reject the candidate.
No claim of arbitrary inverter-error immunity is permitted.

The ideal speed source can supply/absorb mechanical work. Electrical apparent
energy limits do not bound shaft startup, reversal or stored kinetic energy.
Each speed uses an independent run; no discontinuous physical reversal is claimed.
Other hardware, thermal, loss and inverter limitations remain unchanged.

## Validation and provenance

Native analytical tests check flux signs, intercept cancellation, conditional
bounds with perturbed observations, origin-reference gating and every rejection
path. Integration tests use the existing nonlinear LUT plant, both current axes,
three angle seeds, independent hidden R/PM/map changes, noise, sample refinement,
missing coverage/reference, source rejection and deterministic replay.
See the [guide](../development/local-identification.md) and
[evidence/exit matrix](../development/phase4-local-evidence.md).

The general dq voltage equations are documented in
[MathWorks' flux-based PMSM reference](https://www.mathworks.com/help/autoblks/ref/fluxbasedpmsm.html).
The equations already used by this repository are the derivation basis; the
three-speed schedule, bounded state machine, error budget and orchestration are
project-specific. No vendor code/model/data was copied and no dependency added.
The experimental C/report interfaces make no compatibility promise. Human
acceptance of this proposed decision remains pending.
