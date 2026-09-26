# ADR-0007: Flux-Map Current Compensation and Explicit Model Failure Policy

- Status: Proposed
- Date: 2026-09-25
- Decision owners: Lead Maintainer
- Related: [ADR-0004](0004-use-coupled-flux-linkage-maps.md),
  [ADR-0005](0005-linear-reference-conventions.md),
  [ADR-0006](0006-coupled-lut-runtime-and-plant.md), [Phase 3](../../ROADMAP.md)

## Context

Phase 2 introduced coupled maps in the hidden plant. The nominal PI still
compensates rotation using constant magnetic priors. The next reviewable slice
should consume an independently supplied accepted map without conflating
controller knowledge with plant truth, identification or complete Phase 3.

## Decision

- Reuse the existing measured-signal checks, independent limits, fixed PI gains,
  voltage circle, antiwindup and modulation in a shared internal C kernel.
- Replace only nominal rotational compensation with
  `vd_ff = -omega_e * psi_q(id, iq)` and
  `vq_ff = +omega_e * psi_d(id, iq)`, evaluated at measured currents.
- Preserve dependence of both fluxes on both currents. The controller does not
  request derivatives or invert a Jacobian. Existing map lookup acceptance
  gates remain in force. No finite-difference probes or fitting occur here.
- Keep gains and nominal fallback priors caller-owned and fixed in this slice.
  Full local dynamic decoupling, derivative-based tuning and bounded automatic
  gain selection are later Phase 3 work, not implied by this compensation.
- On lookup failure, default to disabled output with a latched fault. Permit
  nominal PI only under an explicit failure policy. That transition resets
  voltage integrals once, records the cause and stays in fallback until an
  explicit controller reset. It never silently retries a repaired/new map.
- Measurement, configuration, integral-state, arithmetic and hard-limit
  failures always disable/latch; they do not authorize model fallback.
- In the simulator, import the plant and controller datasets independently
  into disjoint storage. A known-model test may explicitly supply equal table
  values, but there is no default copy/read-through from hidden plant truth.
- Compare controllers on the same hidden plant, seed, initial state, PI gains,
  hard limits and scenario. Record both complete maps and acceptance policies.

## Alternatives and consequences

Local Jacobian-based dynamic decoupling/gain scheduling may improve transient
behavior but also requires handling changing gains, derivative discontinuities
and tuning constraints. This slice isolates rotational compensation first.
Constant per-axis parameters remain explicit baseline/fallback only, not a
replacement for the coupled accepted model.

Resetting integrals on fallback is deterministic, not bumpless. It can create
a voltage/current transient; independent voltage/current guards still apply.
This is validated only for stated synthetic scenarios, not as a universal safe
fallback or hardware shutdown. A full supervisor and hardware safety remain
outside scope. Maps remain immutable, caller-owned and prepared before use.

No automatic identification, active/candidate swapping, model learning,
compatibility promise, production dependency or hardware claim is introduced.
Human acceptance of this proposed ADR remains outstanding.

## Validation

Tests isolate flux compensation signs and both cross terms, reject malformed
models/configuration, check sensor/limit failures cannot enter fallback, and
exercise sticky faults, explicit reset and integral reset on fallback.
Integration tests compare nominal, known and mismatched models, including
negative/zero current, voltage limitation, sample refinement, full replay and
runtime domain rejection. A nominal-prior LUT recovers baseline results.
Results and limitations are in [Phase 3 evidence](../development/phase3-evidence.md).
