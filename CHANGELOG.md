# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project intends to follow [Semantic Versioning](https://semver.org/)
once versioned releases begin.

## [Unreleased]

### Added

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

- Magnetic-model direction now centers on coupled flux-linkage LUTs, data
  fitting, and progressive identification-based correction. ADR-0004 records
  coenergy and local incremental matrices as algorithm-dependent tools rather
  than mandatory project-wide architecture.

[Unreleased]: https://github.com/JoaEinsson/FluxForge-NAMC/commits/main
