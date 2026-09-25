# Phase 2 Initial Implementation Evidence

## Scope and provenance

- Date: 2026-09-25.
- Base: `33f2650` (merged Phase 1 PR #4), plus this uncommitted Phase 2 patch.
- Evidence: analytical fixtures, native/Python tests and synthetic simulation.
- Environment: Windows x64, MSVC 19.44.35222.0, Visual Studio 17 2022 generator,
  CMake 4.3.2, Python 3.12.14 and pytest 8.4.2.
- Material AI assistance: implementation, test design/execution and documents
  by the coding assistant. Independent human review/acceptance is outstanding.
- No third-party motor source, firmware or dataset was imported. Equation
  references and approximation limitations are in the [map guide](flux-maps.md).

The fixture is project-original synthetic data, not FEA or hardware data.
Source revisions and the accompanying patch must be retained for archival
reproduction. A dirty marker by itself is not an exact source identifier.

## Commands executed

```text
cmake -S . -B build-phase2 -DBUILD_TESTING=ON -DNAMC_WARNINGS_AS_ERRORS=ON -DNAMC_REQUIRE_PYTEST=ON -DPython3_EXECUTABLE=F:/Projetos/NAMC/.venv/Scripts/python.exe
cmake --build build-phase2 --config Debug --parallel
ctest --test-dir build-phase2 -C Debug --output-on-failure -V -R namc_coupled_flux_map
ctest --test-dir build-phase2 -C Debug --output-on-failure
ctest --test-dir build-phase2 -C Debug -V -R namc_python_binding
cmake --build build-phase2 --config Release --parallel
ctest --test-dir build-phase2 -C Release --output-on-failure
.\.venv\Scripts\python.exe .github/scripts/check_repository.py
git diff --check
```

Configure/build used Python subprocesses with environment keys normalized to
uppercase, avoiding the host's duplicate `Path`/`PATH` MSBuild issue described
in the [Phase 1 evidence](phase1-evidence.md). No machine environment change
or new production dependency was required.

Debug and Release builds passed with warnings treated as errors. Both passed
all five CTest entries. The Python entry passed 42 tests, including the existing
Phase 1 tests and 24 map-import/query tests. Repository policy/link validation
and whitespace checks passed.

## Observed native results

The nonlinear fixture uses 33x33 samples, [-8,8] A on each axis, at 0.5 A
spacing. Each of its 1,024 cells is checked at a held-out point offset by
0.17 A and 0.29 A from its lower corner. Its equations and coefficients are
recorded in `tests/flux_map_test.c` and the map guide.

| Measured quantity | Observed | Test threshold |
| --- | --- | --- |
| Maximum held-out flux error, either axis | 4.7795058e-6 Wb | <8e-6 Wb |
| Maximum held-out Jacobian-entry error | 7.59968e-6 H | <2e-5 H |
| Maximum held-out torque error | 7.60393979e-5 N m | <0.001 N m |
| Sub-cell flux circulation magnitude | 3.06e-8 Wb A | <=6e-7 Wb A plus roundoff |

Derivatives of the interpolated fields also matched independent test-only
central differences to 1e-10 H at the held-out points. The fixture is rejected
if its allowed reciprocity error is tightened to 1e-12 H: bilinear interpolation
is not reported as an exactly conservative magnetic model.

The nonlinear closed-loop regression uses no randomness: zero initial
currents/speed, 0.37 rad initial electrical angle, 48 V bus, references
`id=-2 A, iq=5 A`, and 0.2 N m signed load. The plant uses 0.4 ohm resistance,
0.01 kg m^2 inertia, 0.001 N m s/rad friction and four pole pairs. The unchanged
PI algorithm receives only ideal measurements and independent nominal priors;
its configuration is explicit in `run_nonlinear_loop` in the test source.

Observed results for 0.1 s:

- 2,000 steps at 50 us: final currents approximately -1.9912714 A and
  4.99371567 A; tail dq error RMS 0.0107576589 A.
- 4,000 steps at 25 us: final currents approximately -1.99134378 A and
  4.99368493 A; tail dq error RMS 0.0107241644 A.
- Refinement differences: 7.23789087e-5 A in d current, 3.07385354e-5 A in
  q current and 0.00199172654 rad/s in mechanical speed.
- Both tail RMS values passed the 0.06 A scenario threshold; refinement
  differences passed 0.02 A per current axis and 0.02 rad/s in speed.
- Duties remained in [0,1], and current magnitude stayed below the 7 A limit.

These are executed scenario observations, not embedded result constants or
claims about an identified motor. The affine-map plant reproduced the Phase 1
plant to 1e-10 in each state component over 2,000 steps at 25 us.

## Rejection and ownership checks

Tests cover unsupported model/schema versions, units/conventions, malformed
dimensions, non-monotone axes, NaN/Inf, missing provenance, invalid temperature
or error metadata, singular/ill-conditioned derivatives, non-reciprocal and
negative incremental responses. Scaled solves are checked at 1e-200 and 1e200
matrix scales. Query errors preserve output; failed preparation invalidates
the candidate; failed RK stages preserve the previous plant state.

Python checks buffer ownership after caller mutation/deletion, native domain
errors, duplicate JSON keys, nonstandard numeric tokens, import size limits,
and rejection of acceptance thresholds injected into dataset metadata.

## Simulator integration follow-up

The next local delivery adds imported-map execution to the native simulator,
full accepted-map/configuration reports, bounded decimated traces, and a CLI
comparison with the linear plant. See the
[experiment guide](magnetic-experiments.md) for runnable commands and replay.
This remains part of the same uncommitted Phase 2 patch/base identified above.

Additional commands executed:

```text
python -m pytest -q python/tests/test_magnetic_simulation.py
cmake --build build-phase2 --config Debug --parallel
ctest --test-dir build-phase2 -C Debug --output-on-failure
cmake --build build-phase2 --config Release --parallel
ctest --test-dir build-phase2 -C Release --output-on-failure
ctest --test-dir build-phase2 -C Debug -V -R namc_python_binding
```

The narrow pytest command used the workspace `.venv` interpreter and explicit
`PYTHONPATH`, `NAMC_CORE_LIBRARY` and `NAMC_SIM_EXECUTABLE` paths. The final full
suite passed all five CTest entries in Debug and Release, including **69 Python
tests** (27 new simulator/fixture integration cases). Both native builds passed
with warnings as errors. Repository policy/link and whitespace checks passed.

The node fixture is checked against the documented analytical polynomial.
Integration tests cover complete map replay, source metadata escaping,
caller-copy ownership, positive/negative/zero-current scenarios, same-controller
comparison, trace decimation and metrics, invalid configuration/transport,
and runtime map-domain failure with no success report. A map representing
the Phase 1 linear equations agrees with its independent native plant within
absolute `1e-10` for final state, metrics, and sampled trace fields.

Observed Debug results using seed 42, 2,000 steps at 50 us, `id=-2 A`,
`iq=5 A`, 48 V, signed load 0.2 N m, and the checked-in nonlinear fixture:

| Quantity | Linear plant | Coupled-map plant |
| --- | --- | --- |
| Full-run dq tracking RMS, A | 0.278406017 | 0.359180337 |
| Last-half dq tracking RMS, A | 0.00380768737 | 0.0107576589 |
| Final d current, A | -1.99832493 | -1.99127140 |
| Final q current, A | 4.99659159 | 4.99371567 |
| Final torque, N m | 1.38502506 | 1.55528864 |
| Peak current magnitude, A | 5.38137808 | 5.51952293 |

For the same nonlinear experiment at 4,000 steps/25 us, tail RMS was
0.0107241644 A. The final-current differences were approximately
7.23789e-5 A (d) and 3.07385e-5 A (q); mechanical-speed difference was
0.00199173 rad/s, below the existing refinement thresholds.

The comparison uses different magnetic plants and the same nominal PI, not
an improved controller or a learned model. The nonlinear plant has different
flux/torque behavior; no efficiency or real-motor performance conclusion
follows from these numbers. Evidence remains synthetic software simulation.

## Outstanding work

- Human review of the implementation and proposed ADR-0006.
- GCC/Clang CI for this patch; these compilers were not run locally.
- Broader map-quality/operating-domain regression coverage before declaring
  Phase 2 complete; imported-map CLI/logging is now implemented as described above.
- No real motor/FEA comparison, fitting, identification, adaptive activation,
  hardware test, embedded timing or safety certification was performed.
- No commit, push, release, PR creation or repository-setting change was made.
