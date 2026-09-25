# FluxForge-NAMC

**Adaptive Nonlinear Motor Control & Identification Platform**

FluxForge-NAMC is an open-source engineering project for the
identification, nonlinear magnetic modeling, adaptive control, and constrained
optimization of electric motors for vehicle-oriented applications.

> **Project status: pre-alpha and simulation-only.** The repository is in its
> engineering-foundation stage, with an experimental linear current-control
> simulation. Nothing in this repository is approved for
> use on a physical vehicle or power inverter.

## Core idea

The central objective is to characterize a partially known motor, build a
usable nonlinear magnetic model, and refine it through identification. For
the initial PMSM scope, the primary representation is a pair of coupled
flux-linkage maps stored as lookup tables (LUTs):

```text
FluxD(id, iq) -> psi_d
FluxQ(id, iq) -> psi_q
```

Both maps depend on both currents so they can capture saturation and
cross-saturation. The planned workflow covers data acquisition or import,
fitting, table generation, and evidence-based corrections as identification
improves coverage. The maps will support current control, torque estimation,
operating-envelope calculation, and optimization while an independent safety
layer retains final authority.

Incremental inductances and a local 2x2 Jacobian may be derived when an
algorithm needs them; they do not define the table dimensions or a mandatory
interface for every subsystem. Coenergy is an optional modeling or fitting
tool. See [ADR-0004](docs/adr/0004-use-coupled-flux-linkage-maps.md) for the
accepted direction and the choices still open.

## Engineering direction

- The reference implementation of firmware-relevant algorithms will be C11,
  with reasonable C99 portability where practical.
- Python will orchestrate simulation, experiments, analysis, and reporting by
  calling the C implementation instead of duplicating critical algorithms.
- MATLAB/Simulink R2023b may be used later for validation, but it is not a
  project dependency.
- Controller code, simulated plant truth, bindings, platform adapters, tests,
  and tools will remain structurally separated.
- Physical correctness, numerical correctness, safety architecture,
  testability, portability, and determinism take precedence over convenience.

The complete project intent and boundaries are summarized in the
[project charter](docs/project-charter.md). Capability phases and their exit
criteria are tracked in the [roadmap](ROADMAP.md), while current work is
organized in the
[public GitHub Project](https://github.com/users/JoaEinsson/projects/3).

## Current repository scope

The initial repository establishes:

- project governance and contribution rules;
- licensing and Developer Certificate of Origin requirements;
- security, support, conduct, and research-safety boundaries;
- an auditable workflow for human and AI-assisted development;
- architecture decision records and capability-based roadmap planning;
- GitHub issue and pull-request templates;
- an experimental, pre-1.0 portable C11 `namc_core` metadata library, a
  shared-library Python binding, and CMake/CTest tests
  ([build instructions](docs/development/build.md));
- a Phase 1 linear PMSM baseline, PI current control, transforms, bounded
  modulation, average inverter, and deterministic native simulator with
  Python orchestration ([linear reference](docs/development/linear-reference.md)).

Coupled nonlinear flux maps, identification, adaptive control, nonideal
inverters/sensors, and hardware integration remain planned. The linear
reference is a comparison baseline, not the project's primary magnetic model.

## Safety boundary

FluxForge-NAMC is currently a research software project. It is not an ECU, a
production inverter, a road-ready controller, or a functionally safe system.
It has not been validated on physical hardware and makes no claim of compliance
with ISO 26262 or any other safety standard. See [Safety Scope](docs/safety-scope.md).

## Contributing

Start with [CONTRIBUTING.md](CONTRIBUTING.md), follow the
[Code of Conduct](CODE_OF_CONDUCT.md), and sign every commit according to the
[Developer Certificate of Origin](DCO). Material AI assistance must be
disclosed as described in the
[AI-assisted development policy](docs/development/ai-assisted-development.md).

## License

Code and documentation are licensed under the
[Apache License 2.0](LICENSE). Future datasets or imported motor maps may carry
separate, explicitly documented licenses.
