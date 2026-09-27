# Phase 4 Initial Resistance Identification Evidence

This is the historical first-slice record. Subsequent paired-flux/PM acquisition
and combined validation are recorded in [Phase 4 local evidence](phase4-local-evidence.md).
The limitations below describe this resistance slice, not the later combined scope.

## Scope and provenance

- Date: 2026-09-26.
- Base: `be4f38aaf132044283cbe08aa722e66f45fb6c88` (merged PR #8), plus the
  uncommitted patch on `codex/phase4-resistance-identification`.
- Environment: Windows x64, MSVC 19.44.35222.0, Visual Studio 17 2022 generator,
  CMake 4.3.2, Python 3.12.14, pytest 8.4.2.
- Evidence: native analytical/guard tests and synthetic software simulation.
  No measured motor, HIL, bench, vehicle or embedded execution evidence.
- Material AI assistance: implementation, test design/execution, result analysis
  and documentation by the coding assistant. Human review/acceptance is pending.

The unchanged project-original nonlinear flux-map fixture supplies hidden plant
truth. The identifier receives only observable currents, terminal voltages,
electrical speed, sequence and voltage-source metadata. Its native test links
only the portable core. Plant R is varied independently of the fixed controller
priors; truth-error comparisons occur only in the experiment report after the
identifier stops. No production dependency or vendor implementation was added.

See [proposed ADR-0010](../adr/0010-bounded-standstill-resistance-identification.md)
and the [method/defaults guide](resistance-identification.md). Retain the patch
with the base commit: the configure-time `-dirty` marker does not uniquely
identify its source changes. Reports and interfaces remain experimental.

## Executed checks

Commands below ran against this patch. Configure/build were invoked from Python
subprocesses with environment keys normalized to uppercase to avoid this host's
pre-existing Path/PATH MSBuild issue; no machine environment was changed.

```text
cmake -S . -B build-phase4-resistance -DBUILD_TESTING=ON -DNAMC_WARNINGS_AS_ERRORS=ON -DNAMC_REQUIRE_PYTEST=ON -DPython3_EXECUTABLE=F:/Projetos/NAMC/.venv/Scripts/python.exe
cmake --build build-phase4-resistance --config Debug
ctest --test-dir build-phase4-resistance -C Debug --output-on-failure -R namc_resistance_identification
python -m pytest python/tests/test_resistance_identification.py -q
ctest --test-dir build-phase4-resistance -C Debug --output-on-failure
cmake --build build-phase4-resistance --config Release
ctest --test-dir build-phase4-resistance -C Release --output-on-failure
python .github/scripts/check_repository.py
git diff --check
```

Direct Python commands used `.venv/Scripts/python.exe`. The narrow pytest run
set `PYTHONPATH=python`, `NAMC_CORE_LIBRARY` and `NAMC_IDENT_EXECUTABLE` to the
Debug build's explicit library/executable paths. CTest supplies these paths for
the complete suite.

- Both builds passed with warnings as errors.
- All **8 CTest entries passed in Debug and Release**, including **153 Python
  tests**. The new resistance integration module contains **29 passing tests**.
- Repository policy/relative-link and whitespace checks passed.
- Native coverage includes exact slope/intercept/conditional bounds, no early
  publication, three-stage transitions, terminal-state latching, abort/restart,
  copied configuration, malformed inputs, sequence/source changes, nonfinite
  observations, current/voltage/speed/energy limits and every quality gate.
- Integration coverage includes three hidden resistances at three angle seeds,
  same-build full-report replay, sample-period refinement, voltage offsets,
  observation errors, explicit CLI thresholds and invalid configurations.

## Reproducing the baseline

This command ran with both Debug and Release paths and matching output names:

```powershell
$env:PYTHONPATH = "$PWD/python"
.\.venv\Scripts\python.exe -m fluxforge_namc.identify `
  --executable build-phase4-resistance/Debug/namc_ident_sim.exe `
  --library build-phase4-resistance/Debug/namc_core_binding.dll `
  --map tests/fixtures/nonlinear_flux_map.json `
  --min-incremental-h 1e-5 --max-reciprocity-error-h 1e-5 --min-rcond 1e-6 `
  --output results/phase4-resistance-debug.json
```

Generated reports `results/phase4-resistance-debug.json` and
`results/phase4-resistance-release.json` are ignored, reproducible artifacts.
They include the complete plant map/validation thresholds, build provenance,
seed, controller priors, experiment policy, independent budgets, observed window
means/minima/maxima, source/errors, state/reason and conditional estimate bounds.

Baseline: seed 42, R=0.4 ohm, initial mechanical speed zero, 48 V bus, 50 us
sample period, +1/+2/-1 A d-current levels, 1000 settling and 500 measurement
samples per level. No observation noise/bias is injected; declared current and
voltage errors are each 0.0001 in SI units. All other defaults are in the guide.

## Observed outcomes

Both baseline builds accepted the same numerical estimate after **4500 samples
(0.225 s)**, with three 500-sample measurement windows and no remaining excitation
request. Rounded observed results:

- R: **0.400000000778 ohm**.
- Conditional interval: **[0.399720056767, 0.400280056789] ohm**.
- Fitted voltage intercept: **-2.47482e-7 V**.
- Negative-level prediction residual: **9.85734e-7 V**.
- Sampled apparent-energy proxy: **0.287183651 J**, against a 1 J budget.
- Peak simulated current: approximately **2.03 A**, against 3 A.
- Peak applied voltage: approximately **8.20 V**, against 12 V.

The following additional Debug observations used the same fixture and seed 42,
changing only hidden plant R. The automated suite repeats each at seeds 1, 42
and 987; every case contains the truth inside its declared measurement interval.

| Hidden R, ohm | Estimated R, ohm | Estimate minus truth, ohm |
| ---: | ---: | ---: |
| 0.2 | 0.199999997907 | -2.09302e-9 |
| 0.4 | 0.400000000778 | 7.78051e-10 |
| 0.8 | 0.800000013107 | 1.31074e-8 |

These tiny errors describe settled, ideal synthetic observations. They are not
an accuracy specification for sensors, reconstructed voltage or physical motors.
Halving the sample period to 25 us and doubling all duration counts also passed
the 2e-6 ohm test tolerance. No cross-platform bitwise equivalence is claimed.

### Perturbation and rejection observations

The rows below use baseline inputs with only the listed changes. The noisy case
uses independently declared 0.001 A/V error bounds, not automatic knowledge of
the injected noise. All terminal outcomes disable further excitation requests;
rejected estimates are `null` rather than partially published candidates.

| Changed input | Observed result |
| --- | --- |
| Constant voltage bias +0.02 V | Accepted; unchanged R, intercept about 0.01999975 V |
| Current and voltage bounded noise amplitudes 0.0005; declared errors 0.001 | Accepted; R=0.400007322 ohm, interval [0.397212934, 0.402812910] ohm |
| Constant voltage bias +0.1 V | Rejected: offset, reason 9, sample 4500 |
| Voltage error +0.01 V for positive current / -0.01 V for negative current | Rejected: holdout validation, reason 11, sample 4500 |
| Current-noise amplitude 0.02 A | Rejected: unsettled window, reason 5, sample 1003 |
| Declared voltage uncertainty 0.02 V | Rejected: uncertainty, reason 10, sample 4500 |
| Initial mechanical speed 0.2 rad/s | Stopped: electrical-speed limit, reason 3, sample 1 |
| Maximum duration 2000 samples | Rejected: timeout, reason 12, sample 2000 |
| Apparent-energy budget 0.01 J | Stopped: energy limit, reason 3, sample 273 |
| Command-only voltage provenance | Rejected: untrusted voltage, reason 4, sample 1 |
| Settling interval one sample | Rejected: tracking, reason 6, sample 2 |

These rejections are expected test outcomes, not successful resistance estimates.
A zero experiment-process exit means a report was produced; callers must inspect
`accepted` and `state.reason`. Invalid startup configuration and plant/controller
failures instead exit nonzero without a success report.

## Remaining limitations

- Bounds are conditional absolute measurement bounds, not statistical confidence.
  Small sample spans do not prove settled flux: slow transients or drift may
  remain. A coherent calibration gain error can pass all three current levels
  while scaling R. Source labels and declared errors need independent justification.
- The average inverter and controller sensors remain ideal. Added noise/bias
  perturb only identifier observations; this is not a physical nonideal inverter,
  sensor, thermal or battery model.
- Speed/energy guards act after an observed interval; they cannot undo its energy
  or prove instantaneous physical shutdown. No terminal-current decay is simulated.
- No PM-flux reference, coupled magnetic-response identification, flux-table
  fitting/correction, global coverage or automatic model/gain activation is included.
- The identifier has not been exercised on embedded targets or hardware. GCC and
  Clang were not run locally; existing CI remains to be exercised after an
  authorized push. No timing/WCET or hardware-readiness claim is made.
- No commit, push, release, repository-setting change or independent approval was
  performed. Phase 4 remains in progress; earlier phase limitations are unchanged.
