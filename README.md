# FluxForge-NAMC

**Adaptive Nonlinear Motor Control & Identification Platform**

FluxForge-NAMC is an open-source research and development project for the
identification, nonlinear magnetic modeling, adaptive control, and constrained
optimization of electric motors for vehicle-oriented applications.

> **Project status: pre-alpha and simulation-only.** The repository is in its
> governance and engineering-foundation stage. It does not yet contain a
> functional motor controller, and nothing in this repository is approved for
> use on a physical vehicle or power inverter.

## Core idea

The project is not intended to be another constant-parameter field-oriented
controller. Its central objective is to let a controller progressively build
and validate an effective nonlinear electromagnetic model of a partially known
motor, including saturation and cross-saturation. The model should expose the
local flux linkages and full incremental magnetic matrix:

```text
               [ Ldd  Ldq ]
psi_d, psi_q,  [          ]
               [ Lqd  Lqq ]
```

That local model will support identification, current control, torque
estimation, operating-envelope calculation, and optimization while an
independent safety layer retains final authority.

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
- GitHub issue and pull-request templates.

The build system, C libraries, simulation plant, Python bindings, tests, and
first executable experiments are planned for the next development phase.

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
