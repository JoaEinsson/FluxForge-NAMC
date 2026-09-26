# Phase 3 Offline PI Tuning Evidence

## Scope and provenance

- Date: 2026-09-26.
- Base: `aa73855` (merged PR #6), plus this uncommitted patch on
  `codex/phase3-bounded-pi-tuning`.
- Scope: offline local PI gain calculation with explicit design constraints,
  startup-only application, diagnostics and three-way same-plant comparison.
- Evidence: analytical tests and synthetic software simulation only.
- Environment: Windows x64, MSVC 19.44.35222.0, Visual Studio 17 2022 generator,
  CMake 4.3.2, Python 3.12.14, pytest 8.4.2.
- Material AI assistance: implementation, test design/execution and documents
  by the coding assistant. Human review/acceptance remains outstanding.

The map fixture is project-original synthetic data. Resistance inputs are
explicit experiment priors, not estimates obtained by an identifier. No
production dependency, external dataset or vendor code was added. Equation
provenance and design rationale are in
[proposed ADR-0008](../adr/0008-bounded-offline-pi-tuning.md).
Retain the source patch with the base revision for reproduction; the binary's
configure-time dirty marker does not uniquely identify the patch.

## Executed validation

```text
cmake -S . -B build-phase3-tuning -DBUILD_TESTING=ON -DNAMC_WARNINGS_AS_ERRORS=ON -DNAMC_REQUIRE_PYTEST=ON -DPython3_EXECUTABLE=F:/Projetos/NAMC/.venv/Scripts/python.exe
cmake --build build-phase3-tuning --config Debug --target namc_current_tuning_test --parallel
ctest --test-dir build-phase3-tuning -C Debug --output-on-failure -R namc_current_tuning
cmake --build build-phase3-tuning --config Debug --parallel
python -m pytest -q python/tests/test_current_tuning.py
cmake --build build-phase3-tuning --config Release --parallel
ctest --test-dir build-phase3-tuning -C Debug --output-on-failure
ctest --test-dir build-phase3-tuning -C Release --output-on-failure
ctest --test-dir build-phase3-tuning -C Debug -V -R namc_python_binding
.\.venv\Scripts\python.exe .github/scripts/check_repository.py
git diff --check
```

The narrow pytest command used the workspace `.venv` interpreter with explicit
`PYTHONPATH`, `NAMC_CORE_LIBRARY` and `NAMC_SIM_EXECUTABLE` pointing to this
Debug build. Configure/build used Python subprocesses with environment keys
normalized to uppercase for the host's `Path`/`PATH` MSBuild issue. No machine
environment change or installation was needed.

Results:

- Debug and Release native builds passed with warnings treated as errors.
- All **7 CTest entries** passed in both configurations.
- The Python suite passed **106 tests**, including **22 new tuning cases**.
- The new native test checks analytical coupled-J gains, distinct off-diagonal
  entries, negative cross terms, each cap, invalid/nonfinite inputs, infeasible
  headroom/sample-rate/current budgets, rejected maps and unchanged outputs
  after rejection.
- Python tests check same-plant comparisons, independent limits, reverse and
  zero-current operation, resistance/map mismatch, low bus voltage, sample
  refinement, complete replay and unsupported input combinations.
- Repository policy/link validation and whitespace checks passed.

## Observed comparison

Reproduce with the [guide's CLI command](current-tuning.md#cli-example-and-three-way-comparison):
nonlinear fixture, seed 42, 2,000 steps at 50 us, references `id=-2 A, iq=5 A`,
48 V bus, signed load 0.2 N m, zero initial current/speed. The seed selects
initial electrical angle 1.585531494539493 rad. Controller gains are calculated
at the requested currents and design speed zero using independently supplied
`R_est=0.4 ohm` and requested bandwidth parameter 1,500 rad/s.

The accepted local derivative was approximately:

```text
Jdd = 0.0029515 H        Jdq = 0.0000252 H
Jqd = 0.0000210 H        Jqq = 0.00374695 H
```

Both cross terms are retained, including their small bilinear interpolation
reciprocity difference. The rate factor `||J^-1 diag(Jdd,Jqq)||_inf` was
1.0085862943 and the selected rate/sample quantity was 0.08246608724. No cap
reduced the request in this case. Generated gains were `Kpd=4.42725 V/A`,
`Kpq=5.620425 V/A`, `Kid=Kiq=600 V/(A s)`. The design does not claim that its
bandwidth parameter is the measured nonlinear closed-loop bandwidth.

Observed Debug results:

| Quantity | Nominal PI | Fixed-gain flux PI | Tuned flux PI |
| --- | --- | --- | --- |
| Full-run dq error RMS, A | 0.359180337 | 0.359215303 | 0.302935391 |
| Last-half dq error RMS, A | 0.0107576589 | 0.000138187063 | 0.000132254656 |
| Peak current, A | 5.51952293 | 5.52351596 | 5.39095859 |
| Voltage-boundary occupancy, steps | 0 | 0 | 1 |
| Fallback iterations | 0 | 0 | 0 |

The tuned case improved full-run error in this scenario but used a wider duty
range (approximately [0.0103302544, 0.989669746]) and reached the voltage
boundary once. Do not infer reduced voltage demand, optimality, general
robustness or real-motor efficiency from these results.

### Mismatch and constrained voltage

With unchanged hidden plant/map, resistance priors 0.3 and 0.5 ohm (+/-25%)
gave tail RMS errors approximately 0.000632204 A and 0.000115334 A respectively.
The lower prior degraded tail error relative to the matched case. Separate
tests perturb the controller map's affine PM/inductance components by +/-20%
without changing the nonlinear terms or relaxing acceptance thresholds. Those
cases meet the existing 0.06 A tail-error and 12 A current bounds; they are
not a coverage or uncertainty guarantee.

At 12 V, with the same reference/load/seed and design policy, the voltage/error
cap reduced the requested parameter from 1,500 to 633.6873741 rad/s. Tail RMS
was 0.000912859 A, full RMS 0.591127109 A and peak current 5.38515956 A.
Voltage-boundary occupancy was 28 iterations. This illustrates why the local
1 A design-error budget does not prevent saturation during a larger startup
step; the independent runtime voltage limiter still applies.

## Limitations and unperformed work

- Local rate/headroom constraints are engineering design policies, not a
  general sampled/nonlinear stability theorem or a phase-margin measurement.
- Gains are fixed after startup; no gain scheduling, online learning,
  excitation-based identification or runtime candidate activation is added.
- Only disable-on-model-failure is qualified for tuned runs here. The runner
  rejects tuned plus nominal-fallback requests. Previously tested fixed-gain
  fallback is unchanged.
- No GCC/Clang execution for this patch, hardware experiment, HIL, embedded
  timing/WCET, noise/delay characterization or safety certification was done.
- Human acceptance of ADR-0008 and broader Phase 3 validation remain pending.
- No commit, remote push, PR creation, release or repository setting changed.

The earlier Phase 3 evidence used 0.37 rad when describing the executable's
seed-42 angle. That prose is corrected in this patch to the recorded LCG
angle above. The earlier metric observations are unchanged; 0.37 rad belongs
to a separate native analytical/control test, not this seeded CLI scenario.
