# ADR-0005: Linear Reference Conventions and Simulation Boundary

- Status: Proposed
- Date: 2026-09-25
- Decision owners: Lead Maintainer
- Related: [Phase 1](../../ROADMAP.md), [ADR-0004](0004-use-coupled-flux-linkage-maps.md)

## Context

The first closed-loop reference needs explicit signs, normalization, plant
integration, and a boundary between observable signals and hidden truth.
It must provide a useful regression baseline without replacing the planned
coupled flux-linkage maps or claiming hardware readiness.

## Decision

The experimental implementation in this patch proposes:

- amplitude-invariant Clarke/Park transforms for balanced, three-wire currents;
- d-axis aligned with the permanent-magnet flux, positive electrical angle
  advancing from alpha toward beta, and q-axis leading d by 90 degrees;
- positive q current producing positive motor torque for positive PM flux;
- SI units and electrical speed equal to pole pairs times mechanical speed;
- fixed-step RK4 plant integration with stationary-frame applied voltage held
  over a controller sample, rotating that voltage at each RK stage;
- a radial voltage limit and common-mode-injection average inverter;
- discrete PI control with nominal decoupling and back-calculation anti-windup;
- independent nominal controller priors, ideal phase-current and encoder
  measurements, and latched numerical/limit faults with disabled duty output.

The equations, timing, and limitations are specified in the
[linear reference guide](../development/linear-reference.md). These choices
are implemented for evaluation but await human acceptance of this ADR. They
make no stable C ABI, report-format, serialization, or hardware commitment.

## Alternatives considered

- Power-invariant normalization avoids the 3/2 power factor, but requires
  differently scaled currents, fluxes, gains, and torque equations.
- Forward Euler is smaller, but RK4 provides a more accurate simulation oracle
  without adding a production dependency. The plant is not a firmware target.
- Ideal dq voltage applied directly would bypass the modulation/inverter path
  and hide frame-rotation effects within a sample.
- Reading plant parameters into the controller would give misleading tracking
  evidence. Controller priors are specified independently and intentionally
  differ from the simulated motor in the executable scenario.

## Consequences

All later maps must declare compatible transform conventions or undergo an
explicit conversion. Constant inductances are confined to this named linear
baseline. The core cannot include the plant-only header through its target
include paths; the simulator owns both sides and the measurement adapter.

Disabled output is a software diagnostic, not a validated physical inverter
shutdown strategy. No identification, map fitting, adaptive activation,
switching effects, hardware interface, or safety-certified supervisor is added.

## Validation

Native tests cover transform bases, round trips, power balance, flux, torque,
electrical/mechanical dynamics, modulation, saturation recovery, invalid input,
model versions, and sticky faults. Python invokes the C executable to check
repeatability, replay, tracking with mismatched priors, reverse operation, and
sample-time refinement. Local results and unperformed checks are recorded in
the [implementation evidence](../development/phase1-evidence.md).
