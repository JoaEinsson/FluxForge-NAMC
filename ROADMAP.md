# FluxForge-NAMC Roadmap

## Roadmap policy

This roadmap is capability-based. A phase is complete only when its exit
criteria are demonstrated by repository evidence. Dates are intentionally not
promised until contributor capacity and implementation velocity are known.

GitHub milestones should mirror these phases. Issues may be explored early,
but a later phase must not be presented as complete while an earlier safety or
validation dependency remains unresolved.

## Current status

| Phase | State | Intended release |
| --- | --- | --- |
| G0 — Governance foundation | Complete | Initial commit |
| 0 — Repository and toolchain foundation | In progress | `0.1.0` candidate |
| 1 — Linear reference system | In progress | Pre-`1.0` |
| 2 — Nonlinear magnetic plant | In progress | Pre-`1.0` |
| 3 — Model-aware current control | In progress | Pre-`1.0` |
| 4 — Local identification | Planned | Pre-`1.0` |
| 5 — Adaptive global magnetic surface | Planned | Pre-`1.0` |
| 6 — Inverter and sensor nonidealities | Planned | Pre-`1.0` |
| 7 — Thermal and battery constraints | Planned | Pre-`1.0` |
| 8 — Torque and efficiency optimization | Planned | Pre-`1.0` |
| 9 — Monte Carlo evidence | Planned | Pre-`1.0` |
| 10 — Embedded portability evidence | Planned | Pre-`1.0` |

No `1.0.0` criteria have been defined. Hardware readiness is not implied by
any phase below.

The magnetic-model direction is coupled flux-linkage lookup tables, initially
`FluxD(id, iq)` and `FluxQ(id, iq)`, with extraction, fitting, and progressive
correction from identification data. Local incremental matrices are derived
when needed by a selected algorithm; coenergy is an optional modeling or
fitting tool. [ADR-0004](docs/adr/0004-use-coupled-flux-linkage-maps.md) records
this decision. The initial Phase 2 LUT/plant slice is implemented; the broader
magnetic, identification and adaptive capabilities below are not yet complete.

## G0 — Governance foundation

### Scope

- Apache-2.0 licensing and notices;
- founder-led governance with an explicit transition trigger;
- contribution, DCO, conduct, support, security, and safety policies;
- human and AI-assisted development rules;
- issue, pull-request, ADR, and project-management conventions;
- English-language project charter and initial roadmap.

### Exit criteria

- all foundational documents are internally consistent and linked;
- the repository clearly states its pre-alpha, simulation-only status;
- contribution provenance and AI disclosure requirements are explicit;
- no implementation capability is claimed.

## Phase 0 — Repository and toolchain foundation

### Scope

- modular repository tree for core, plant, bindings, simulations, tests, tools,
  platforms, and documentation;
- CMake targets for portable core, plant, shared binding library, simulator,
  and tests;
- Linux GCC and Clang continuous integration;
- Python package skeleton and test orchestration;
- formatting, warnings, static checks, and reproducible configuration.

### Exit criteria

- a clean Linux checkout configures, builds, and tests with documented
  commands;
- the core has no operating-system dependency;
- CI validates GCC, Clang, CTest, and pytest;
- version, seed, and configuration can be recorded for experiments.

## Phase 1 — Linear reference system

The first implementation slice includes a native closed-loop reference,
analytical and guard tests, and Python orchestration. See the
[linear reference guide](docs/development/linear-reference.md) and
[local evidence](docs/development/phase1-evidence.md). Phase completion still
requires review of the proposed conventions and CI evidence for this patch;
local simulation results do not imply hardware validation.

### Scope

- documented Clarke/Park conventions and power-consistency tests;
- linear PMSM reference model, mechanical dynamics, and encoder truth;
- baseline PI current control with decoupling;
- inverse transforms, voltage limiting, and average-inverter modulation;
- deterministic closed-loop executable and Python-visible results.

### Exit criteria

- analytical flux, torque, transform, and dynamic cases pass;
- the controller closes the current loop without hidden access to plant truth;
- invalid numerical inputs cannot silently reach duty commands;
- logs reproduce a run from configuration and seed.

## Phase 2 — Nonlinear magnetic plant

The first slice provides coupled bilinear LUTs, offline validation, strict
domain checks, experimental Python JSON import/native queries, and a guarded
nonlinear current-state plant. Synthetic tests cover cross-saturation,
interpolation/derivative error, the linear limit and a baseline current loop.
See the [guide](docs/development/flux-maps.md),
[proposed ADR-0006](docs/adr/0006-coupled-lut-runtime-and-plant.md), and
[local evidence](docs/development/phase2-evidence.md). Imported-map experiments
now run in the native simulator with complete map/configuration/seed reports,
bounded traces and linear-plant comparisons; see
[magnetic experiments](docs/development/magnetic-experiments.md).
Broader map-quality assessment, review and CI evidence for this patch remain
outstanding; this is not real-motor validation.

### Scope

- coupled `FluxD(id, iq)` and `FluxQ(id, iq)` lookup-table backend;
- map import and validation with units, conventions, domain, and provenance;
- analytical reference fixtures and linear-limit cases for testing;
- saturation and cross-saturation across the mapped current domain;
- nonlinear electrical dynamics and model-domain validation;
- interpolation, flux, torque, and energy-consistency tests, including local
  derivative and reciprocity checks where required by the chosen formulation.

### Exit criteria

- the maps preserve dependence on both currents and reproduce reference
  cross-saturation cases;
- flux and torque predictions meet documented tolerances at reference points
  and at points not used for fitting;
- derivatives, when used, agree with independent test oracles and retain
  cross-coupling terms;
- invalid data, unsupported operating points, and any singular matrix solves
  produce deterministic diagnostics;
- the linear backend is recovered as a controlled special case.

## Phase 3 — Model-aware current control

The first slice adds fixed-gain PI with flux-map rotational compensation,
independent model inputs, explicit disable/latched nominal fallback policies,
and same-plant controller comparisons with replayable diagnostics. See the
[controller guide](docs/development/model-aware-current.md),
[proposed ADR-0007](docs/adr/0007-flux-map-current-compensation.md), and
[local evidence](docs/development/phase3-evidence.md). Bounded automatic gain
tuning, timing evidence, broader stability assessment and review remain
outstanding; this is not complete Phase 3 or adaptive identification.

### Scope

- deterministic controller using accepted flux maps for model-based
  compensation and tuning;
- local sensitivities only as required by the selected control algorithm,
  with documented derivative evaluation and bounded computation;
- invalid-model fallback and conditioning checks for any matrix solves;
- auto-tuning constrained by sample rate, bandwidth, voltage, and current;
- controlled comparison with the baseline PI controller.

### Exit criteria

- both controllers run against identical hidden plants and scenarios;
- tracking, saturation, timing, and stability metrics are reported;
- known-model and mismatched-model cases are included;
- no improvement claim is made without generated evidence.

## Phase 4 — Local identification

### Scope

- identification state machine and confidence reporting;
- low-energy stator-resistance estimation;
- excitation and estimation of coupled magnetic response over sampled current
  operating points, including the references needed to reconstruct flux maps;
- permanent-magnet flux estimation under observable conditions;
- map-fitting observations with coverage, uncertainty, and acquisition metadata;
- candidate validation, degradation, and fault states.

### Exit criteria

- controller inputs contain only simulated vehicle-observable measurements;
- truth-versus-estimate tests quantify error and observability;
- the procedure distinguishes observed map regions from unsupported regions
  and does not claim a complete map from insufficient excitation;
- non-convergence leads to degraded or fault behavior rather than unsafe
  continuation;
- inverter voltage error is not silently absorbed into magnetic parameters.

## Phase 5 — Adaptive global magnetic surface

### Scope

- initial flux-map fitting and progressive correction from identification data;
- coupled LUT generation with regularized fitting, coverage, and confidence
  over the identified domain;
- selection of grid resolution and interpolation using prediction error,
  physical consistency, memory, and runtime cost;
- optional coenergy-constrained fitting when supported by the model assumptions;
- active/candidate model separation, validation, atomic activation, and
  rollback;
- bounded extrapolation and conservative fallback models;
- serialization design with version, domain, CRC, and endianness.

### Exit criteria

- bounded runtime lookup is separated from fitting and candidate validation;
- candidates meet documented flux, torque, and closed-loop acceptance criteria
  on held-out points and scenarios;
- domain, finite-value, interpolation, and physical-consistency checks gate
  activation; derivative, reciprocity, curvature, and conditioning checks apply
  where required by the selected formulation;
- candidate rejection and rollback are tested;
- memory and execution costs are reported for representative map sizes.

## Phase 6 — Inverter and sensor nonidealities

### Scope

- average and switching/nonideal inverter models;
- PWM/SVPWM path, dead time, device drop, delay, and DC-bus effects;
- noise, quantization, offset, gain, drift, latency, clipping, and sampling;
- estimator-aware voltage reconstruction.

### Exit criteria

- detailed mode consumes phase duties rather than ideal `vd`/`vq` commands;
- each nonideality can be enabled independently and deterministically;
- model mismatch and timing delays have regression scenarios;
- safety checks cover invalid sensors and modulation limits.

## Phase 7 — Thermal and battery constraints

### Scope

- winding resistance and permanent-magnet flux temperature effects;
- lumped motor and inverter thermal models;
- battery OCV, internal resistance, SOC, motoring, and regenerative limits;
- dynamic derating and loss-component estimates.

### Exit criteria

- thermal, electrical, and battery limits remain distinct and traceable;
- motoring and regeneration use direction-appropriate constraints;
- near-zero efficiency calculations are numerically safe;
- derating behavior is deterministic and tested.

## Phase 8 — Torque and efficiency optimization

### Scope

- map-based MTPA, field weakening, voltage-constrained operation, and MTPV
  where applicable;
- generation of control-reference current tables from accepted magnetic maps
  and the applicable voltage, current, and loss models;
- dynamic torque/current/power envelope;
- configurable ECO, NORMAL, and SPORT objective weights;
- bounded firmware-suitable lookup or solver methods.

### Exit criteria

- requested, electromagnetic, battery, inverter, thermal, and final torque
  limits are separately observable;
- offline reference solutions validate runtime approximations;
- motoring and regenerative operating regions are covered;
- optimizer requests remain subordinate to hard safety limits.

## Phase 9 — Monte Carlo evidence

### Scope

- randomized hidden motor, inverter, sensor, thermal, and battery populations;
- estimator-model mismatch across controlled families;
- baseline, known-nonlinear, and adaptive-nonlinear comparisons;
- reproducible datasets, metrics, plots, and failure triage.

### Exit criteria

- initial campaigns cover at least 100 reproducible cases;
- convergence, failure, map-error, tracking, and stability distributions are
  reported without hard-coded results;
- worst cases are retained as regression scenarios;
- limitations and the sampled validity domain are explicit.

## Phase 10 — Embedded portability evidence

### Scope

- generic embedded HAL boundaries;
- STM32 and TI C2000 compile skeletons when toolchains are available;
- memory, execution-time, and deterministic-path instrumentation;
- supervisor/MCU split documentation and all-in-one Linux mode.

### Exit criteria

- portable core compilation is demonstrated on available embedded toolchains;
- fast paths have bounded memory and execution structure;
- platform code does not leak into the mathematical core;
- no hardware-validation claim is made without separately reviewed evidence.

## Roadmap changes

Roadmap changes use a focused pull request. Changes that alter the project
mission, safety boundary, public compatibility promise, or phase ordering
require Lead Maintainer approval and may require an ADR.
