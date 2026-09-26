# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project intends to follow [Semantic Versioning](https://semver.org/)
once versioned releases begin.

## [Unreleased]

### Added

- Separate reserve gains for tuned model-aware PI, deterministic model-loss
  transition tests, initial-speed/fault-injection experiments, transient metrics,
  opt-in host timing and a finite campaign retaining unsuccessful scenarios.
- Bounded offline PI gain design from accepted coupled-map derivatives and an
  independent resistance estimate, startup-only application, replayable tuning
  diagnostics and three-way same-plant controller comparisons.
- Initial Phase 3 fixed-gain PI with coupled flux-based rotational compensation,
  independently supplied controller maps, explicit disable/latched nominal
  fallback policies and same-plant controller comparisons with replay diagnostics.
- Configurable imported-map experiments in the native simulator, full-map
  replay reports, bounded decimated traces, torque diagnostics and a Python CLI
  for comparing nonlinear/linear plants with the same independent nominal PI.
- Experimental coupled flux-linkage LUTs with offline data/physical validation,
  bounded bilinear lookup, analytical interpolant derivatives and conditioned solves.
- Nonlinear current-state plant with domain-checked RK4, synthetic cross-saturation
  and linear-limit tests, plus Python import/query bindings using the native C backend.

- Initial governance, licensing, contribution, security, support, and conduct
  policies.
- Project charter, capability-based roadmap, safety scope, and development
  workflow.
- AI-assisted development policy and repository instructions for coding
  agents.
- Architecture decision record process and initial decisions.
- GitHub issue, pull-request, and ownership templates.
- Experimental, pre-1.0 portable C11 `namc_core` metadata API with CMake,
  CTest, a shared-library Python binding, and GCC/Clang CI foundation.
- Experimental Phase 1 linear PMSM reference, PI current control, Clarke/Park
  transforms, bounded common-mode modulation, average inverter, and fixed-step
  C simulator with deterministic configuration/seed reports and Python orchestration.
- Analytical physics, power, numerical-guard, saturation-recovery, tracking,
  and reproducibility regression tests; proposed conventions in ADR-0005.

### Changed

- Experimental model-current configuration version is now 2 with an optional
  immutable mapped-gain pointer; callers must rebuild and initialize that field.
  No stable API/ABI or report-format compatibility is promised.
- Corrected the Phase 3 first-slice evidence's initial angle for seed 42 to
  match the simulator's recorded LCG-selected angle; result metrics are unchanged.
- Magnetic-model direction now centers on coupled flux-linkage LUTs, data
  fitting, and progressive identification-based correction. ADR-0004 records
  coenergy and local incremental matrices as algorithm-dependent tools rather
  than mandatory project-wide architecture.

[Unreleased]: https://github.com/JoaEinsson/FluxForge-NAMC/commits/main
