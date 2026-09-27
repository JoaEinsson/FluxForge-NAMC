# Phase 4 Local Identification Evidence and Exit Matrix

## Scope and provenance

- Experiments executed: 2026-09-26; evidence record finalized: 2026-09-27.
- Base: `be4f38aaf132044283cbe08aa722e66f45fb6c88` (merged PR #8), plus the
  uncommitted Phase 4 patch on `codex/phase4-resistance-identification`.
- Windows: MSVC 19.44.35222.0, CMake 4.3.2, Python 3.12.14, pytest 8.4.2.
- Linux/WSL Ubuntu: GCC 15.2.0, Python 3.14.4. CMake, Clang and pytest were not
  available there; manual native builds and the Python campaign used no new packages.
- Evidence: analytical/unit tests and synthetic software simulation only.
- Material AI assistance: implementation, test design/execution, analysis and
  documentation. Human review and ADR acceptance remain pending.

The implementation now covers the Phase 4 simulation scope: resistance,
observable coupled flux points, PM reference, sparse acquisition metadata,
conditional uncertainty and deterministic rejection. It does not implement
Phase 5 global fitting/correction/activation or Phase 6 inverter calibration.
Read [the guide](local-identification.md),
[ADR-0010](../adr/0010-bounded-standstill-resistance-identification.md), and
[ADR-0011](../adr/0011-observable-coupled-flux-point-acquisition.md).

No vendor implementation, motor dataset or new production dependency was added.
The existing synthetic fixture is unchanged. Tests additionally transform that
fixture with explicit synthetic provenance. The configure-time `-dirty` marker
does not uniquely identify a patch; retain this source diff with its base commit.

## Executed Windows checks

The following ran against this patch. Configure/build used a Python subprocess
environment with keys normalized to uppercase for this host's pre-existing
Path/PATH MSBuild issue; no machine environment was modified.

```text
cmake -S . -B build-phase4-resistance -DBUILD_TESTING=ON -DNAMC_WARNINGS_AS_ERRORS=ON -DNAMC_REQUIRE_PYTEST=ON -DPython3_EXECUTABLE=F:/Projetos/NAMC/.venv/Scripts/python.exe
cmake --build build-phase4-resistance --config Debug
ctest --test-dir build-phase4-resistance -C Debug --output-on-failure -R "namc_flux_identification|namc_coupled_flux_map"
python -m pytest python/tests/test_flux_identification.py -q
ctest --test-dir build-phase4-resistance -C Debug --output-on-failure
cmake --build build-phase4-resistance --config Release
ctest --test-dir build-phase4-resistance -C Release --output-on-failure
python .github/scripts/check_repository.py
git diff --check
```

Direct pytest used `.venv/Scripts/python.exe`, `PYTHONPATH=python`, and explicit
Debug paths in `NAMC_CORE_LIBRARY`, `NAMC_IDENT_EXECUTABLE` and
`NAMC_FLUX_IDENT_EXECUTABLE`. CTest supplies all executable/library paths.

- **9/9 CTest entries passed in both Debug and Release**, with warnings as errors.
- **202 Python tests passed** per complete configuration, including 29 resistance
  tests and **49 new coupled-flux/campaign tests**.
- Narrow native and Python checks passed before the final complete runs.
- Repository policy/relative-link validation and `git diff --check` passed.

Native acquisition tests cover analytical coupled flux signs, intercepts,
uncertainty with perturbed current/speed/voltage/R, origin-reference checks,
copied configuration, no early publication, sticky terminal states, abort/restart,
NaN/Inf, sequencing, changing/unsupported voltage source, all independent limits,
range/uncertainty/holdout failures and timeout. The core-only test links no plant.

The driven-plant test independently checks exact zero-speed RL decay, constant
imposed speed, angle evolution and unchanged state after rejection. Existing
free-rotor, control, map, tuning and qualification regressions remain passing.

## Executed GCC and sanitizer checks

Manual GCC commands ran from `/mnt/f/Projetos/NAMC` in Ubuntu WSL. All seven
native test programs passed with strict C11 warnings as errors:

```sh
for name in core reference flux_map model_current current_tuning resistance_id flux_id; do
  gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion \
    -Wshadow -Wstrict-prototypes -Werror -Iinclude -Iplant/include \
    src/core/core.c src/core/reference.c src/core/flux_map.c \
    src/core/current_tuning.c src/core/resistance_id.c src/core/flux_id.c \
    src/plant/linear_plant.c src/plant/magnetic_plant.c \
    tests/${name}_test.c -lm -o build-phase4-gcc/${name}_test
  ./build-phase4-gcc/${name}_test
done
```

The ignored output directory was created before these commands. The two new
identifier tests also passed in C99 mode under AddressSanitizer and
UndefinedBehaviorSanitizer, with no reported diagnostics:

```sh
for name in resistance_id flux_id; do
  gcc -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Wall -Wextra -Wpedantic -Werror -Iinclude src/core/*.c \
    tests/${name}_test.c -lm -o build-phase4-gcc/${name}_sanitized
  ./build-phase4-gcc/${name}_sanitized
done
```

The shared core and both new native runners were built at `-O2` with the same
strict C11 warning flags. The shared library used `-shared -fPIC
-DNAMC_CORE_SHARED_BUILD`, core sources only, and `-lm`. Runners used core/plant
sources, `simulations/map_transport.c` and respectively
`simulations/resistance_reference.c` / `simulations/flux_identification.c`.
An ignored `build-phase4-gcc/provenance.h` passed through `-include` defined:

```c
#define NAMC_BUILD_REVISION "be4f38aaf132044283cbe08aa722e66f45fb6c88-dirty"
#define NAMC_BUILD_COMPILER "GCC-15.2.0"
#define NAMC_BUILD_CONFIGURATION "manual-O2"
```

The complete default campaign also passed with these GCC binaries and Linux
Python. This is additional native/experiment evidence, **not** a claim that the
full CMake/pytest Linux matrix or remote CI ran. No toolchain/package was installed.

## Reproducing the end-to-end campaign

The [guide's command](local-identification.md#running-the-complete-local-campaign)
ran for Debug and Release, producing ignored reproducible reports:

- `results/phase4-local-debug.json`;
- `results/phase4-local-release.json`.

The GCC run used:

```sh
PYTHONPATH=python python3 -m fluxforge_namc.local_identification \
  --resistance-executable build-phase4-gcc/namc_ident_sim \
  --flux-executable build-phase4-gcc/namc_flux_ident_sim \
  --library ./build-phase4-gcc/libnamc_core_binding.so \
  --map tests/fixtures/nonlinear_flux_map.json \
  --min-incremental-h 1e-5 --max-reciprocity-error-h 1e-5 --min-rcond 1e-6 \
  --output results/phase4-local-gcc.json
```

All defaults are documented in the guide and embedded in reports. The campaign
uses seed 42, hidden R=0.4 ohm and 48 V. A native resistance experiment hands its
accepted estimate/error to eleven independent flux-point experiments. Each point
uses three externally driven speed runs (+80/-80/+40 electrical rad/s), 1500
settling plus 500 measurement samples each, and a 50 us interval.

Full plant data is retained for replay/evaluation only. Exported fitting
observations omit hidden truth and explicitly mark temperature unmeasured. Every
requested point and failure is retained. No generated metric is hard-coded in
the implementation or asserted as physical-motor evidence.

## Observed baseline outcomes

Debug, Release and GCC each accepted all **11 requested points** and the origin
PM reference. These are 11 discrete observations, not complete coverage of the
motor map. Representative rounded Debug values follow; GCC differs at ordinary
floating-point roundoff, not by a promised bitwise-equivalence contract.

| Requested id, iq (A) | Estimated psi_d (Wb) | Estimated psi_q (Wb) |
| --- | ---: | ---: |
| 0, 0 | 0.049999936425 | approximately 0 |
| 4, 0 | 0.061871955254 | approximately 0 |
| 0, 4 | 0.049999934231 | 0.015808024770 |
| 4, 4 | 0.061795158269 | 0.015731215463 |
| -2.3, 1.7 (held out) | 0.043133217346 | 0.006773421614 |
| 2.1, -1.3 (held out) | 0.056276486521 | -0.005185791119 |

- R estimate: **0.400000000778 ohm**, from the preceding native experiment.
- Maximum absolute flux error across both axes and all 11 points:
  **8.21039e-8 Wb**, versus hidden LUT truth evaluated after acquisition.
- Reported conditional absolute bounds: **1.71088e-5..3.19062e-5 Wb**.
  Every baseline truth value lies inside its interval.
- Largest third-speed voltage residual: **9.66666e-6 V** (limit 0.005 V).
- Peak simulated current: **5.918434 A** (limit 8 A).
- Peak applied voltage: **22.641555 V** (limit 25 V).
- Largest per-point sampled apparent energy: **12.933693 J** (limit 30 J).
- Sum of independent experiments' sampled apparent-energy proxies, including R:
  **75.560960 J**. This excludes shaft startup/reversal and is not winding heat
  or a continuous hardware-session energy guarantee.

Cross effects are resolved in both maps: changing q from 0 to 4 A at id=4 A
changes estimated d flux by about **-7.6797e-5 Wb**; changing d from 0 to 4 A at
iq=4 A changes estimated q flux by about **-7.6809e-5 Wb**. Each difference exceeds
the sum of its two conditional error bounds, and agrees with the fixture's
-7.68e-5 Wb cross effect. These are paired observed flux differences, not a
global constant-inductance replacement or a fitted local matrix.

## Variation, rejection and negative evidence

- Five current points passed at each angle seed 1, 42 and 987. End-to-end tests
  also passed with hidden R=0.2/0.8 ohm while keeping controller priors fixed.
- Synthetic PM references 0.03/0.07 Wb and response scales 0.8/1.0 were recovered
  at the origin and at (4,-4) A with unchanged controller configuration.
- Halving the sample interval and doubling sample counts retained agreement
  within the fixed 2e-6 Wb regression tolerance.
- Bounded current/voltage noise of 0.0002 A/V with independently declared
  0.0003 A/V bounds passed, with truth inside the reported intervals.
- Same-build point and campaign replay reproduced complete reports exactly.

| Perturbation / missing condition | Observed outcome |
| --- | --- |
| Constant q-voltage offset +0.02 V | Accepted separately as offset; flux unchanged |
| Constant q-voltage offset +0.1 V | DEGRADED, excessive offset |
| q-voltage error +0.02/-0.02 V by speed sign | DEGRADED, third-speed validation failed |
| Shaft does not rotate | DEGRADED, unobservable; no PM/flux candidate |
| Shaft speed differs by 10% | DEGRADED, tracking |
| Only 2500 total samples allowed | DEGRADED, timeout |
| 0.01 J budget at (4,4) A | FAULT at sample 8; last observed energy 0.0115461 J retained |
| Command-only voltage source | DEGRADED, unsupported source |
| One settling sample at (4,4) A | DEGRADED, tracking |
| Declared voltage error 0.02 V | DEGRADED, excessive uncertainty |
| Current-noise amplitude 0.002 A | DEGRADED, unsettled window |
| No acquired origin point | Accepted local data retained, but no PM reference / complete-campaign claim |
| R rejected | No magnetic experiment launched |
| Native point fault/process error | Later requests skipped; earlier results and failure remain visible |

### Calibration ambiguity is not hidden

Deliberately injecting **+2% voltage gain** while falsely declaring ideal narrow
measurement errors passes the residual gates at the origin and estimates about
**0.050999935 Wb** for a true 0.05 Wb reference. Its approximately 0.001 Wb error
exceeds the falsely justified bound. This is an explicit negative regression,
not accepted evidence of accuracy or a correct calibration.

With that same injected error but an independently declared **0.1 V** voltage
bound, the candidate is rejected for uncertainty. This demonstrates the supported
boundary: trusted/calibrated terminal observations with justified bounds, not
arbitrary inverter commands or undetectable calibration errors. Later physical
voltage reconstruction requires separate validation.

### The map gate was not weakened

An initially positive test expectation for a coarse fixture scaled by 1.2 failed
because its bilinear reciprocity error exceeds the existing 1e-5 H threshold.
The threshold was retained. That scenario is now an explicit pre-acquisition
map-rejection regression; accepted PM/response variation tests use valid prepared
maps. No failing motor experiment is counted as a passing estimate.

## Phase 4 exit-evidence matrix

| Roadmap criterion | Implemented evidence and boundary |
| --- | --- |
| Controller/identifier inputs contain only observable measurements | Core-only APIs receive currents, terminal voltage, encoder speed and independent policy/accepted R; no plant/map access. Harness truth is evaluation-only. |
| Truth-versus-estimate tests quantify error and observability | Generated R/flux/PM comparisons, parameter/seed/noise/refinement tests, no-motion rejection and the false-calibration counterexample above. |
| Observed and unsupported regions remain distinct | Explicit accepted/unsupported requests, no imputation, no bounding-box interpolation, missing-origin handling and `complete_map: false`. |
| Non-convergence degrades/faults rather than continuing unsafely | Bounded windows/timeout, source/sequence/limit/quality rejection, inactive terminal requests, sticky states, explicit restart and campaign fault-stop. This is a software boundary, not a physical shutdown certification. |
| Inverter voltage error is not silently absorbed into magnetic parameters | Command-only sources rejected; offsets represented and bounded, speed-sign error rejected, declared voltage uncertainty propagated. Gain ambiguity is explicitly documented/tested and requires independent calibration; no arbitrary-error immunity is claimed. |

All planned Phase 4 implementation categories have local evidence within this
declared simulation/observability scope. Formal phase acceptance is **not** given
by the coding assistant: proposed ADRs need human acceptance, and roadmap policy
still requires review of earlier-phase dependencies. No existing exit criterion,
safety rule or protection threshold was removed to obtain this result.

## Not performed / outside this delivery

- No global LUT fit, interpolation/extrapolation, adaptive correction, active
  model swap/rollback or automatic controller retuning (Phase 5).
- No calibration recovery for arbitrary inverter/sensor errors, hysteresis,
  iron loss, thermal dependence or physical shaft controller (later scope).
- No full Linux CMake/pytest matrix, Clang run, remote CI result, target timing
  measurement, hardware/HIL/bench/vehicle experiment or certification claim.
- No commit, push, PR, release, dependency installation, repository-setting
  change or independent human approval. All Phase 4 changes remain local.
