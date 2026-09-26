# Phase 3 Reserve-Gain and Operating-Point Evidence

## Scope and provenance

- Date: 2026-09-26.
- Base: `6116ae2` (merged PR #7) plus the uncommitted patch on
  `codex/phase3-fallback-validation`.
- Environment: Windows x64, MSVC 19.44.35222.0, Visual Studio 17 2022 generator,
  CMake 4.3.2, Python 3.12.14, pytest 8.4.2.
- Evidence: native analytical/guard tests and synthetic software simulation.
  No measured motor, HIL, bench, vehicle or embedded execution evidence.
- Material AI assistance: implementation, test design/execution, result analysis
  and documentation by the coding assistant. Human acceptance remains pending.

The existing project-original nonlinear map fixture is unchanged. All critical
algorithms and physics remain native C. No new production dependency, proprietary
code, external motor data, governance or licensing change was introduced.
Design choices are in [proposed ADR-0009](../adr/0009-separate-reserve-gains-and-qualify-transitions.md).
Retain the patch with the base commit: the binary's configure-time dirty marker
does not uniquely identify the source changes.

## Executed checks

Commands below ran against this patch. Configure/build were invoked from Python
subprocesses with environment keys normalized to uppercase to avoid the host's
pre-existing Path/PATH MSBuild issue; no machine environment was changed.

```text
cmake -S . -B build-phase3-validation -DBUILD_TESTING=ON -DNAMC_WARNINGS_AS_ERRORS=ON -DNAMC_REQUIRE_PYTEST=ON -DPython3_EXECUTABLE=F:/Projetos/NAMC/.venv/Scripts/python.exe
cmake --build build-phase3-validation --config Debug
ctest --test-dir build-phase3-validation -C Debug --output-on-failure -R namc_model_current
python -m pytest python/tests/test_model_control.py python/tests/test_current_tuning.py -q
python -m pytest python/tests/test_control_qualification.py -q
cmake --build build-phase3-validation --config Release
ctest --test-dir build-phase3-validation -C Debug --output-on-failure
ctest --test-dir build-phase3-validation -C Release --output-on-failure
python .github/scripts/check_repository.py
git diff --check
```

Direct pytest runs used `.venv/Scripts/python.exe`, `PYTHONPATH=python` and
explicit `NAMC_CORE_LIBRARY` / `NAMC_SIM_EXECUTABLE` paths from the Debug build.

- Debug and Release built with warnings as errors.
- Repository policy/relative-link validation and whitespace checks passed.
- All **7 CTest entries passed in both configurations**, including **124 Python
  tests**. The new qualification module accounts for 19 tests; the previous
  rejection of tuned-plus-fallback was intentionally replaced with positive
  transition coverage.
- Native tests prove mapped gain selection, reserve independence, exact first
  fallback equivalence to a reset nominal PI, subsequent integral accumulation,
  no automatic map/gain retry, invalid reserve/mapped gains and hard-limit shutdown.
- Python tests cover early/late/final-sample loss, disable-by-default, input
  rejection, signed/zero-current overshoot and settling oracles, decimation,
  report replay, CLI comparison, timer path counts and identical physical outputs
  with timing enabled/disabled. The full finite campaign is part of the suite.

## Reproducing the campaign

See [the guide](control-qualification.md) for every scenario and acceptance
criterion. The following command ran twice, with Debug and Release paths and
matching output names:

```powershell
$env:PYTHONPATH = "$PWD/python"
.\.venv\Scripts\python.exe -m fluxforge_namc.qualification `
  --executable build-phase3-validation/Release/namc_sim.exe `
  --library build-phase3-validation/Release/namc_core_binding.dll `
  --fixture tests/fixtures/nonlinear_flux_map.json `
  --timing --output results/phase3-qualification-release.json
```

Generated local reports: `results/phase3-qualification-debug.json` and
`results/phase3-qualification-release.json` (ignored, reproducible artifacts).
Full maps, thresholds, observed plant parameters, scenario/tuning inputs,
per-run build provenance, metrics, timing and unsuccessful diagnostics are retained.
No failing case is excluded from totals. A campaign exit code of zero only means
that the campaign report was produced.

## Observed outcomes

Both configurations produced **180 attempted runs, 172 completed, 153 meeting all
criteria, 19 completed with unmet criteria, and 8 rejected/stopped**. These are
not 180 successful qualification cases. Rounded numerical examples below are
from Release; no cross-platform bitwise equivalence is claimed.

| Controller arm | Attempted | Completed | Meets all criteria |
| --- | ---: | ---: | ---: |
| Nominal PI | 56 | 55 | 49 |
| Fixed-gain mapped PI | 56 | 55 | 49 |
| Tuned mapped PI, including 12 fault-injection attempts | 68 | 62 | 55 |

All completed runs respected the tested 12 A current and [0,1] duty criteria.
Largest observed current norm was **5.640667 A**; largest per-axis overshoots
were **0.209349 A (d)** and **0.417857 A (q)**. These maxima exclude stopped runs,
which have no success metrics and must not be interpreted as safe completions.

### Low-voltage failures remain visible

- Grid cases 12..14 and 18..20 combine 12 V with speed/torque direction requiring
  substantial voltage. Six nominal and six fixed-mapped runs fail tracking and
  settling. Four corresponding tuned runs also fail those criteria; two tuned
  requests (grid-14 and grid-20) are rejected before activation as infeasible.
- Example grid-13: target (-2,5) A, initial speed 20 mechanical rad/s, 12 V,
  matched map and R=0.4 ohm. Tail RMS is 1.387549 A nominal, 1.394959 A fixed
  mapped and 1.309896 A tuned. None settles within the record. Voltage-boundary
  occupancy is 1268, 1272 and 1098 of 2000 samples respectively.
- At 100 mechanical rad/s and 12 V, nominal/fixed-mapped runs stop on plant-map
  domain rejection at samples 80/75. The tuned request is rejected at startup.
  Domain rejection is a simulation boundary, not proof of physical safety.

Tuning cannot manufacture bus-voltage headroom. A successful initial design is
also not a guarantee of later feasible operation as speed evolves. Future
reference supervision/field weakening would need separately scoped design;
this patch does not hide those cases or relax independent limits.

### Tuned fallback is conditional, not universal

Of 12 injected-loss attempts, 9 reach the transition and 6 meet all criteria:

- Passing scenarios use fault sample 0 or 200 with (12 V,-20 rad/s),
  (48 V,-20 rad/s), or (48 V,+20 rad/s), all at target (-2,5) A, affine map
  scale 1.2 and R estimate 0.5 ohm.
- The three sample-1000 cases in that same set settle again, but fail the
  0.06 A second-half RMS criterion: approximately 0.10, 0.11 and 0.21 A.
  The recovery transient starts inside the RMS window; criteria were not
  redefined after observing the result.
- The other three attempts, at (12 V,+20 rad/s), are rejected by tuning before
  activation. They do **not** provide fallback-transition evidence.

For (48 V,+20 rad/s), fault sample 200, Release records:

- first fallback sample 200; 1800 fallback samples; no automatic retry;
- voltage-command jump **3.093751 V**, post-loss peak error **0.618404 A**;
- tail RMS **0.010824 A**, final-window settling time **0.023750 s** from startup;
- current peak **5.394594 A**, duties within [0.010794,0.989206].

Sample-0 jump values are measured from a defined zero previous command and
include startup, not just gain switching. No bumpless-transfer claim is made.

## Host timing observations

Release campaign, Windows `QueryPerformanceCounter`, timestamp tick 0.1 us.
The averages below are weighted by sample count across completed runs; maxima
are observed maxima, not WCET. Dispatch/timer overhead and OS preemption remain.

| Path | Samples | Mean, us | Observed maximum, us |
| --- | ---: | ---: | ---: |
| Nominal | 110000 | 0.13 | 23.20 |
| Mapped (fixed and tuned) | 219600 | 0.22 | 36.90 |
| First fallback transition | 9 | 1.04 | 2.70 |
| Latched fallback | 14391 | 0.13 | 3.80 |

Only nine transition samples were observed. These numbers are host diagnostics,
not a target-period assertion, interrupt latency bound, embedded performance
claim or cross-machine benchmark. Failed runs have no timing success report.

## Remaining limitations

- Finite synthetic operating-point coverage, ideal sensors/average inverter;
  no noise, delay, thermal drift, hardware nonidealities or global stability proof.
- Paired map/R perturbations, not all independent uncertainty combinations.
- Separate reserve gains and deterministic reset do not guarantee safe physical
  torque transients or universal recovery across the map.
- POSIX timer implementation and GCC/Clang builds were not executed on this
  Windows host; existing CI must exercise them after an authorized push.
- No target WCET, release, remote push, repository-setting change or independent
  review was performed. Phase 3 is not declared fully accepted.
