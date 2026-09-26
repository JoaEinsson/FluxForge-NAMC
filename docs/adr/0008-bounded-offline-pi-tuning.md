# ADR-0008: Bounded Offline PI Gain Design from an Accepted Coupled Map

- Status: Proposed
- Date: 2026-09-26
- Decision owners: Lead Maintainer
- Related: [ADR-0004](0004-use-coupled-flux-linkage-maps.md),
  [ADR-0006](0006-coupled-lut-runtime-and-plant.md),
  [ADR-0007](0007-flux-map-current-compensation.md), [Phase 3](../../ROADMAP.md)

## Context

The first model-aware controller uses manually selected PI gains. The next
slice should calculate bounded candidate gains from an independently supplied
model and resistance estimate, without introducing hidden plant access or
unreviewed online gain changes. Physical motor identification is a later phase.

## Decision

- Add an experimental, offline-only C gain-design function. It performs one
  guarded lookup of flux and the full analytic interpolant Jacobian at an
  explicit operating point, then two existing guarded solves. No finite
  differences, fitting, allocation, gain search or fast-loop work is introduced.
- Retain the diagonal PI architecture and flux rotational compensation.
  Generate `Kpd = omega * Jdd`, `Kpq = omega * Jqq`, and
  `Kid = Kiq = omega * R_est`. These are local design gains, not full nonlinear
  dynamic decoupling or a claim of achieved closed-loop bandwidth.
- Both cross terms participate in a local rate cap through
  `||J^-1||_inf` and `||J^-1 diag(Jdd,Jqq)||_inf`. Do not symmetrize J or replace
  the magnetic map with its diagonal. Plant/runtime lookup remain unchanged.
- Bound the design parameter by requested/admissible bandwidth, sample time,
  explicit gain ceilings and estimated voltage headroom. Check the selected
  current/error budget, bus and speed against read-only independent limits.
- Return gains and diagnostics only, with output unchanged on failure.
  Activation is the caller's responsibility. The simulator applies a successful
  candidate before reset/first enable, never while the loop is running.
- Require an explicit positive resistance estimate; never read hidden plant R.
  The design point uses requested currents/bus and explicitly selected speed,
  not simulated future state. Reports retain all inputs and the full local J.
- Initially support tuned experiments only with disable-on-model-failure.
  Reject tuned plus nominal-fallback requests rather than reusing tuned gains
  as an unvalidated degraded configuration. Existing fixed-gain fallback is
  unchanged. Separately qualified fallback gains remain future work.
- Compare nominal PI, fixed-gain flux PI and tuned flux PI on the same plant,
  initial state and limits. Distinguish compensation effects from gain effects.

## Alternatives and consequences

A full matrix PI or online gain schedule could better address coupled dynamics
but changes the runtime controller, antiwindup and transition behavior. The
present slice retains that runtime architecture. Excitation-based autotuning
would require an identification/supervision workflow and is not introduced.

The rate/headroom caps are conservative local engineering design policies,
not a general stability theorem. They omit computation/PWM delay, noise,
uncertainty bounds, integral-state dynamics and variation outside the selected
point. The design-error budget is not enforced as a reference slew limit;
larger startup errors may still saturate under the existing voltage limiter.
Derivative jumps at LUT knots follow ADR-0006's deterministic cell selection.

No public compatibility, hardware behavior, production dependency or new safety
authority is promised. Human acceptance of this proposed ADR remains pending.

## Validation and provenance

Analytical coupled fixtures check both off-diagonal entries, gain equations,
all caps and rejection without output mutation. Simulator tests cover replay,
same-plant comparisons, reverse/zero currents, sample refinement, low voltage,
independent resistance/map mismatch, and unsupported input combinations.
See [tuning guide and evidence](../development/current-tuning.md).

The scalar PI starting relations are documented in
[MathWorks' current-regulator equations](https://www.mathworks.com/help/autoblks/ref/interiorpmcontroller.html).
The use of local coupled derivatives and the additional caps are this project's
experimental design, not a claim that the cited source validates nonlinear
maps or these limits. No source code, vendor tables or proprietary assets were
copied; implementation and independent test oracles are written for this project.
