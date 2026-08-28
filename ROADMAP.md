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
| 0 — Repository and toolchain foundation | Planned | `0.1.0` candidate |
| 1 — Linear reference system | Planned | Pre-`1.0` |
| 2 — Nonlinear magnetic plant | Planned | Pre-`1.0` |
| 3 — Model-aware current control | Planned | Pre-`1.0` |
| 4 — Local identification | Planned | Pre-`1.0` |
| 5 — Adaptive global magnetic surface | Planned | Pre-`1.0` |
| 6 — Inverter and sensor nonidealities | Planned | Pre-`1.0` |
| 7 — Thermal and battery constraints | Planned | Pre-`1.0` |
| 8 — Torque and efficiency optimization | Planned | Pre-`1.0` |
| 9 — Monte Carlo evidence | Planned | Pre-`1.0` |
| 10 — Embedded portability evidence | Planned | Pre-`1.0` |

No `1.0.0` criteria have been defined. Hardware readiness is not implied by
any phase below.

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

### Scope

- coenergy-based analytical backend;
- flux gradients and full 2x2 Hessian evaluation;
- progressive saturation and cross-saturation;
- nonlinear electrical dynamics and model-domain validation;
- derivative, reciprocity, determinant, and energy-consistency tests.

### Exit criteria

- `Ldq` and `Lqd` are measurable and not artificially zeroed;
- analytical derivatives agree with numerical test oracles within documented
  tolerances;
- singular and nonphysical local models produce deterministic diagnostics;
- the linear backend is recovered as a controlled special case.

## Phase 3 — Model-aware current control

### Scope

- deterministic controller using local flux and the full incremental matrix;
- bounded 2x2 inversion with fallback;
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
- local full-matrix estimation using safe independent excitation;
- permanent-magnet flux estimation under observable conditions;
- candidate validation, degradation, and fault states.

### Exit criteria

- controller inputs contain only simulated vehicle-observable measurements;
- truth-versus-estimate tests quantify error and observability;
- non-convergence leads to degraded or fault behavior rather than unsafe
  continuation;
- inverter voltage error is not silently absorbed into magnetic parameters.

## Phase 5 — Adaptive global magnetic surface

### Scope

- compact basis or tensor-product spline representation of coenergy;
- regularized fitting and confidence over the identified domain;
- active/candidate model separation, validation, atomic activation, and
  rollback;
- bounded extrapolation and conservative fallback models;
- serialization design with version, domain, CRC, and endianness.

### Exit criteria

- the model derives flux and Hessian analytically in runtime code;
- physical smoothness, reciprocity, curvature, and determinant checks gate
  activation;
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
