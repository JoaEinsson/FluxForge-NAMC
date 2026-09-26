# Phase 3 First-Slice Evidence

## Scope and provenance

- Date: 2026-09-25.
- Base: `aa771d6` (merged Phase 2 PR #5), plus this uncommitted patch on
  `codex/phase3-model-aware-current`.
- Scope: fixed-gain PI with coupled flux-map rotational compensation, explicit
  model-failure policy and same-plant comparisons. This is not complete Phase 3.
- Evidence: analytical fixtures, native/Python tests and synthetic simulation.
- Environment: Windows x64, MSVC 19.44.35222.0, Visual Studio 17 2022 generator,
  CMake 4.3.2, Python 3.12.14 and pytest 8.4.2.
- Material AI assistance: implementation, test design/execution and documents
  by the coding assistant. Independent human review/acceptance is outstanding.
- No new production dependency or third-party motor dataset/source was imported.

The coupled fixture is project-original synthetic data, not measured motor
data or FEA. Retain this patch together with the base revision to reproduce
the source; a dirty marker alone is not an exact source identifier. Equations,
failure semantics and executable comparison commands are in the
[controller guide](model-aware-current.md) and
[proposed ADR-0007](../adr/0007-flux-map-current-compensation.md).

## Commands executed

```text
cmake -S . -B build-phase3 -DBUILD_TESTING=ON -DNAMC_WARNINGS_AS_ERRORS=ON -DNAMC_REQUIRE_PYTEST=ON -DPython3_EXECUTABLE=F:/Projetos/NAMC/.venv/Scripts/python.exe
cmake --build build-phase3 --config Debug --parallel
cmake --build build-phase3 --config Release --parallel
ctest --test-dir build-phase3 -C Debug --output-on-failure -R namc_model_current
python -m pytest -q python/tests/test_model_control.py
ctest --test-dir build-phase3 -C Debug --output-on-failure
ctest --test-dir build-phase3 -C Release --output-on-failure
ctest --test-dir build-phase3 -C Debug -V -R namc_python_binding
.\.venv\Scripts\python.exe .github/scripts/check_repository.py
git diff --check
```

Configure/build ran via Python subprocesses with environment keys normalized
to uppercase to avoid this host's duplicate `Path`/`PATH` MSBuild issue, as
documented in [Phase 1 evidence](phase1-evidence.md). This did not modify the
machine environment. The narrow pytest command used the workspace `.venv`
interpreter with `PYTHONPATH=F:/Projetos/NAMC/python`,
`NAMC_CORE_LIBRARY=F:/Projetos/NAMC/build-phase3/Debug/namc_core_binding.dll`
and `NAMC_SIM_EXECUTABLE=F:/Projetos/NAMC/build-phase3/Debug/namc_sim.exe`.

Debug and Release builds passed with warnings treated as errors. Each passed
all six CTest entries, including the new native model-current test. The Python
entry passed **84 tests**, including 15 new controller integration cases.
Repository policy/link validation and whitespace checks passed.

## Same-plant observations

The comparison uses the nonlinear 33x33 fixture over [-8,8] A on both axes,
seed 42, zero initial currents/speed, initial electrical angle 1.585531494539493 rad,
2,000 steps at 50 us, references `id=-2 A, iq=5 A`, 48 V bus and signed
load 0.2 N m. The fixed PI gains, nominal priors and independent hard limits
are identical between controllers. The simulator's current limit is 12 A;
its bus range is 12 to 60 V and electrical-speed limit is 2,000 rad/s.
Full configuration is emitted by each report.

The known-model case explicitly imports two equal maps into separate storage;
the controller never obtains its model from the hidden plant. The fallback
case independently restricts the controller map to the central 3x3 samples
([-0.5,0.5] A per axis), leaves the plant unchanged and explicitly selects
the nominal fallback policy. This helper is `narrow_model` in
`python/tests/test_model_control.py`.

Observed Debug results, rounded for this table:

| Quantity | Nominal PI | Known-map PI | Narrow-map explicit fallback |
| --- | --- | --- | --- |
| Full-run dq tracking RMS, A | 0.359180337 | 0.359215303 | 0.363977613 |
| Last-half dq tracking RMS, A | 0.0107576589 | 0.000138187063 | 0.0107615877 |
| Peak current magnitude, A | 5.51952293 | 5.52351596 | 5.45795845 |
| Final d current, A | -1.99127140 | -1.99985514 | -1.99127143 |
| Final q current, A | 4.99371567 | 5.00006202 | 4.99371546 |
| Final mechanical speed, rad/s | 13.3869599 | 13.4074009 | 13.3643181 |
| Final torque, N m | 1.55528864 | 1.55750706 | 1.55528858 |
| Voltage-boundary occupancy, steps | 0 | 0 | 0 |
| Nominal-fallback iterations | 0 | 0 | 1997 |

All three observed duty ranges were approximately [0.132136134, 0.867863866].
The narrow-map case first entered fallback at iteration 3 (zero-based),
recorded native flux status 3 (outside domain) and did not retry the map.
The default disable policy instead aborts the simulation on domain exit,
with no successful JSON report.

The known-map case reduced tail error in this scenario, but full-run RMS was
slightly worse than nominal PI. Neither broad performance improvement nor
efficiency improvement follows from this single comparison.

At 4,000 steps/25 us, the known-map case produced tail RMS 0.0000766484999 A,
final `id=-1.99992761 A`, `iq=5.00003096 A` and mechanical speed
13.4054027 rad/s. Final-current differences versus 50 us were below 0.02 A
per axis; speed difference was below 0.02 rad/s. These are sample-refinement
checks, not an integration-order proof or continuous-time stability proof.

## Guards, mismatch and replay

- Native analytical tests isolate both flux cross terms and compensation
  signs at measured currents. A separately supplied affine nominal-prior
  LUT recovers baseline final state and metrics within absolute 1e-10.
- Invalid model/configuration versions, missing/unprepared models, invalid
  policies/handles, NaN/Inf, phase imbalance, PI arithmetic overflow and
  current/bus/speed violations are rejected without enabled invalid duties.
  Sensor/configuration/numerical and hard-limit errors cannot enter fallback.
- Model faults latch disabled output by default. Explicit fallback resets
  integrals once, retains the cause and remains nominal until explicit reset,
  even after changing the model pointer. Selecting disable after fallback
  stops output. This is deterministic, not bumpless or hardware-safe transfer.
- Known-model tests include positive/negative/zero current scenarios. Two
  additional cases perturb only the affine PM/inductance priors by +/-20%,
  preserving nonlinear cross terms and unchanged map-acceptance thresholds.
  Both meet the stated 0.06 A tail-error and 12 A current bounds.
- Scaling the entire flux field by 1.2 exceeds the fixture's unchanged
  reciprocity-error tolerance and is rejected before simulation. That
  negative result is retained as a regression; no acceptance gate was relaxed.
- A 12 V case exercises voltage limitation and closed-loop recovery within
  the scenario bounds. Boundary-occupancy counts are independently recomputed
  from the trace, not treated as a measure of clipping severity.
- Complete plant/controller maps, independent thresholds, scenario, seed and
  failure policy replay identically on the same binary. Missing controller
  transport cannot silently reuse plant data. Cross-platform bitwise identity
  is not promised.

## Outstanding work

- Human review and acceptance of the implementation and proposed ADR-0007.
- GCC/Clang CI for this patch; those compilers were not run locally.
- Bounded automatic gain tuning, timing measurements, wider operating-region
  and stability assessment before declaring Phase 3 complete.
- No fitting, identification, runtime map learning/activation, physical motor
  comparison, HIL, bench, vehicle, certification or embedded-readiness claim.
- No commit, push, PR creation, release or repository-setting change was made.
