# ADR-0006: Coupled LUT Runtime and Current-State Plant

- Status: Proposed
- Date: 2026-09-25
- Decision owners: Lead Maintainer
- Related: [ADR-0004](0004-use-coupled-flux-linkage-maps.md),
  [ADR-0005](0005-linear-reference-conventions.md), [Phase 2](../../ROADMAP.md)

## Context

The project needs executable coupled flux tables before identification or
model-aware control can consume them. Runtime cost, domain behavior, units,
and interpolation derivatives must be explicit. A local Jacobian must not
be confused with the dimensions of the global map.

## Decision

For this initial experimental implementation:

- Store two rectangular tables, `psi_d(id, iq)` and `psi_q(id, iq)`, on shared
  monotone current axes, including nonuniform axes. Use SI units and the
  amplitude-invariant dq convention of the linear baseline.
- Interpolate each table bilinearly. Obtain derivatives by differentiating
  that interpolant analytically, not by probing nearby runtime evaluations.
  Preserve both cross terms; never force reciprocity by averaging them.
- Bound each axis to 2..256 samples. Allocate no memory in native lookup or
  plant stepping. Reject out-of-domain currents; do not clamp or extrapolate.
- Separate offline preparation from bounded runtime queries. Validate data,
  model version, metadata, positive incremental response, reciprocity error,
  and conditioning. Repeat local numerical/physical checks during lookup.
- Keep validation thresholds caller-owned. Imported data cannot choose its
  own acceptance thresholds. An accepted in-memory map borrows immutable
  caller-owned arrays; the Python wrapper copies and owns its buffers.
- Use the full derived Jacobian only where required: the current-state plant
  solves `J * di/dt = v - R*i - rotational_emf`. Torque uses flux directly.
  The controller remains the independent nominal PI baseline in this slice.
- Provide a strict, experimental JSON importer in Python. It validates shape
  and metadata, then calls C for physical validation and runtime queries.
- Run imported maps through the same host C experiment loop as the linear
  baseline. A private, bounded numeric stdin bridge avoids introducing a
  second JSON parser or a production dependency in C. The executable validates
  the map independently; this is not the planned persistent/embedded format.
- Embed the complete map and caller thresholds in experiment reports, along
  with scenario, build and seed provenance. Optional bounded, decimated traces
  contain plant truth for evaluation only, never controller/identifier inputs.

See the [implementation guide](../development/flux-maps.md) for equations,
threshold semantics, borrowed-storage requirements and limitations. This ADR
is proposed for human review; neither it nor this patch promises a stable
public API, ABI, serialization format, or hardware behavior.

## Alternatives considered

- Bicubic/spline interpolation offers smoother derivatives but needs a
  separately justified overshoot/physical-consistency strategy and more data.
- Flux-state integration avoids a differential-inductance solve, but current
  recovery then requires a separately bounded inverse-map method.
- A potential-constrained representation can enforce exact reciprocity, but
  would prematurely require one fitting strategy. Coenergy is used only for
  an independent synthetic test oracle in this patch.

## Consequences

Bilinear flux is continuous; its derivatives generally jump at cell edges.
Independent interpolation of two flux fields does not guarantee exact global
energy conservation. Reciprocity error is explicitly bounded, not hidden.
The positive symmetric-part gate is a local incremental criterion, not a
proof of passivity of an approximately nonconservative map or complete drive.

The rectangular grid must be fully populated; it is not proof of measured
coverage. Evidence level, source, temperature and declared error are retained
but their truth/accuracy is not established by parsing or numeric checks.
No fitting, calibration, activation, rollback, or automatic motor measurement
is introduced. Such workflows remain in subsequent roadmap work.

## Validation

Tests cover nonuniform indexing, exact affine coupled fields, boundaries,
derivatives against independent test-only finite differences, both cross
terms, positive/negative curvature, singular solves, conditioning, metadata,
invalid data, and out-of-domain rollback of a plant step.

A 33x33 synthetic nonlinear grid is checked at 1,024 held-out locations and
in a current-control simulation. Energy consistency is assessed with a
bounded flux-circulation test, not claimed exactly. A linear map is compared
against the independent Phase 1 plant. See
[local evidence](../development/phase2-evidence.md) for executed checks.
The [experiment runner](../development/magnetic-experiments.md) also covers
end-to-end linear-limit equivalence, JSON-map replay, forward/reverse tracking,
sample-time refinement, malformed transport and map-domain failure.
