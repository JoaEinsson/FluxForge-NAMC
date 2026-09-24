# ADR-0004: Use Coupled Flux-Linkage Maps

- Status: Accepted
- Date: 2026-09-24
- Decision owners: Lead Maintainer
- Related: [Project Charter](../project-charter.md), [Roadmap](../../ROADMAP.md)

## Context

FluxForge-NAMC needs a practical representation of motor saturation and
cross-saturation that can be characterized, inspected, fitted, and improved
from observations. The intended outcome is useful motor identification,
control, and optimization with quantified error.

Earlier project text made the full local 2x2 incremental matrix a defining
interface and prescribed coenergy for the magnetic backend. Those constraints
were stronger than the requirement. A local sensitivity matrix does not
describe the storage dimensions of a global cross-saturation map, and a
coenergy formulation is one modeling choice rather than the project goal.

## Decision

For the initial PMSM scope, use coupled flux-linkage maps as the primary
magnetic representation, with runtime lookup tables (LUTs):

```text
FluxD(id, iq) -> psi_d
FluxQ(id, iq) -> psi_q
```

Both fluxes depend on both currents, preserving saturation and cross-saturation
throughout the modeled domain. Grid resolution is selected using accuracy and
memory requirements; it is not restricted to a 2x2 array.

The intended workflow is:

1. Acquire or import characterization data with explicit units, transform
   conventions, operating conditions, provenance, and evidence level.
2. Fit an initial pair of coupled maps, accounting for noise, coverage, and
   uncertainty, then generate runtime tables.
3. Validate prediction error and physical consistency at reference and
   held-out operating points before using a candidate model.
4. Use bounded lookup for control and derive torque, operating envelopes, and
   control-reference current tables as needed by the selected algorithms.
5. Refine the maps when identification supplies sufficiently informative
   observations. Validate each candidate before activation and retain rollback.

Fitting and candidate validation remain outside the fast control path.
Incremental inductances or a local Jacobian may be derived when an algorithm
needs them. Their evaluation must preserve cross-coupling, document numerical
behavior, and satisfy that algorithm's conditioning requirements. They are not
a mandatory interface for all model consumers.

Coenergy may supply analytical test fixtures or constrain a fit. Physical
consistency remains required with any representation: a LUT by itself does not
guarantee it. Reciprocity and energy assumptions must be documented and checked
for the chosen magnetic formulation, with dissipative effects kept explicit.

Prefer established engineering methods evaluated by prediction error, control
performance, portability, and bounded cost. Research novelty is not an
acceptance criterion. This decision does not select a universal best fitting,
interpolation, or identification algorithm.

## Alternatives considered

- **Coenergy as the only backend:** useful for enforcing consistency, but
  unnecessarily fixes the representation and fitting strategy before evaluating
  available data and implementation costs.
- **A local incremental matrix as the primary model:** useful for local
  dynamics, but alone does not provide the flux reference or global map needed
  for all intended uses.
- **Constant parameters or independent per-axis curves:** useful baselines and
  fallbacks, but insufficient as the primary cross-saturation representation.

## Consequences

Phases 2 through 5 will build the map representation, consume it in control,
identify observable magnetic behavior, and progressively fit and correct the
maps. Phase names and ordering remain unchanged.

Table resolution, interpolation, regularization, excitation, and correction
methods remain implementation decisions to justify with evidence. Additional
temperature or angle dimensions require appropriate data and cost analysis.
No stable public API or serialization format is established by this ADR.

Current development remains simulation-only. Analytical fixtures, synthetic
observations, FEA data, and measured data must retain distinct provenance and
evidence labels. New hardware characterization still requires the existing
[safety process](../safety-scope.md). Referenced tools are examples of established
workflows, not project dependencies or sources of imported code or datasets.

## Validation

Planned implementation evidence must cover cross-saturation cases, interpolation
error, physical consistency, required derivatives, unsupported-domain behavior,
invalid data, and candidate rejection or rollback. Identification must use only
observable signals; hidden plant truth remains available to the test harness
for evaluation, not to the controller or estimator.

Tests must distinguish fit quality from predictive performance using held-out
points and scenarios. Passing an analytical fixture alone does not demonstrate
accuracy on a real motor. Runtime memory and execution costs must be reported
when an implementation is available. This documentation update claims no
implemented magnetic capability or completed validation.

## References

- [MathWorks: Nonlinear Characterization](https://www.mathworks.com/help/mcb/nonlinear-characterization.html)
  describes characterization data, flux and torque surfaces, and reference LUTs.
- [MathWorks: LUT based PMSM Control Reference](https://www.mathworks.com/help/mcb/ref/lutbasedpmsmcontrolreference.html)
  documents flux-linkage LUT inputs and current references for MTPA and
  field-weakening operation.
- [Plexim: Lookup Table-Based PMSM](https://www.plexim.com/content/look-table-based-pmsm)
  demonstrates saturation and cross-saturation modeling using FEA-based tables.
