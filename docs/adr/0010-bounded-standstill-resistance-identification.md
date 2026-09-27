# ADR-0010: Bounded Standstill Resistance Identification

- Status: Proposed
- Date: 2026-09-26
- Decision owners: Lead Maintainer
- Related: [ADR-0004](0004-use-coupled-flux-linkage-maps.md),
  [ADR-0005](0005-linear-reference-conventions.md), [Phase 4](../../ROADMAP.md)

## Context

The next practical identification increment needs an independently estimated
stator resistance without reading hidden plant parameters. Coupled magnetic
response, PM-flux references and global fitting need additional experiments;
this slice must not pretend a resistance estimate identifies a flux map.

## Decision

- Implement a portable C state machine with fixed storage and bounded per-sample
  work. It requests d-axis currents, never PWM or changes to protection limits.
  Copy experiment policy and independently supplied limits at explicit start.
- At near standstill with settled currents, use two distinct positive current
  plateaus to estimate `R = delta(vd)/delta(id)` and a constant voltage intercept.
  Require a third, negative-current plateau as a held-out consistency check.
  This excitation schedule is an experimental policy, not a hardware procedure.
- Discard a fixed settling interval before each bounded measurement window.
  Require tracking, small q current/voltage and bounded current/voltage spans.
  Reject insufficient separation, out-of-range R/intercept, excessive declared
  measurement uncertainty or a failed held-out prediction. Publish no estimate
  before every check passes. Timeout yields a degraded result with excitation off.
- Enforce independent observed current, terminal voltage, speed and sampled
  apparent-energy budgets. Invalid/nonfinite observations and sequence gaps
  latch faults. Terminal states require explicit restart; no silent retries.
- Consume aligned measured dq currents, terminal voltage, encoder speed and
  sample sequence only. Do not accept flux maps, inductance, plant R or mechanical
  parameters. The terminal-voltage source must explicitly identify ideal
  simulation, measurement or calibrated reconstruction; raw command voltage is
  rejected. Absolute error bounds are caller-declared, not inferred from truth.
- Report conditional resistance bounds, voltage offset, validation residual,
  observed current coverage and state/rejection reason. Bounds are not statistical
  confidence and omit unmodeled transients, calibration gain errors and drift.
- Add a separate native experiment runner against the existing nonlinear plant.
  The Python layer transports maps only to the hidden plant and orchestrates C;
  it does not implement an independent estimator. No automatic activation into
  current gains, nominal priors or magnetic tables is introduced.

## Alternatives and consequences

A single `v/i` ratio absorbs a constant voltage offset. Two-level differencing
avoids that ambiguity, while a reverse-current holdout can expose sign-dependent
errors that two positive levels would hide. This does not separate every possible
inverter or sensor error: a coherent voltage/current gain error is indistinguishable
from resistance without independent calibration. Confidence depends on honest
source/uncertainty metadata and the standstill/steady-state assumptions.

RLS or transient impedance identification would need dynamic flux information
and additional observability decisions. They are not necessary for this bounded
DC increment. No finite difference, inverse matrix or fitted magnetic approximation
is introduced into the runtime path. Coupled maps remain the primary plant model.

An observation-based budget is checked after the measured interval. It can detect
an overrun but cannot undo energy already applied. Sampled apparent energy is
neither winding temperature nor a physical shutdown guarantee. Actuator gating,
hardware protection and terminal-current decay remain outside this API.

## Validation and provenance

Native tests cover the exact two-level slope/intercept, uncertainty interval,
holdout rejection, lack of excitation, state transitions, sticky terminal states,
abort/restart, malformed inputs, sequence faults and hard limits. Integration
tests vary hidden R independently of the controller, angle seeds, observation
noise/errors, duration and sample time, with complete same-build replay.
See the [guide](../development/resistance-identification.md) and
[local evidence](../development/phase4-resistance-evidence.md).

The standard differential resistance relation is documented in
[MathWorks' stator-resistance estimation example](https://www.mathworks.com/help/mcb/gs/run-time-parameter-estimation-pmsm-sensor-feedback.html).
The state machine, reverse holdout, uncertainty gates and implementation are
project-specific; no vendor code, model or dataset was copied. No MATLAB
dependency, compatibility commitment or hardware-readiness claim is introduced.
Human acceptance of this proposed decision remains pending.
