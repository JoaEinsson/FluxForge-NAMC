# Phase 1 Initial Implementation Evidence

## Scope and provenance

- Date: 2026-09-25.
- Evidence level: analytical fixtures and synthetic simulation only.
- Base: `4f6ebf821e3eed15809fecb255b3431249717a8c` plus the uncommitted
  Phase 1 patch accompanying this report. The simulator reports `-dirty`.
- Host: Windows x64, Visual Studio 17 2022 generator, MSVC 19.44.35222.0,
  CMake 4.3.2, Python 3.12.14, pytest 8.4.2.
- Material AI assistance: implementation, tests, documentation, and local
  execution by the coding assistant. Human review and acceptance remain pending.
- No third-party motor code or data imported; equation references are in the
  [linear reference guide](linear-reference.md).

This record describes observed local results, not approval, a release, or
evidence from a physical motor. A dirty revision alone cannot identify an
exact source snapshot; retain this patch with the eventual review commit.

## Commands and results

From the repository root, the following CMake commands were executed:

```text
cmake -S . -B build-phase1 -DBUILD_TESTING=ON -DNAMC_WARNINGS_AS_ERRORS=ON -DNAMC_REQUIRE_PYTEST=ON -DPython3_EXECUTABLE=F:/Projetos/NAMC/.venv/Scripts/python.exe
cmake --build build-phase1 --config Debug --parallel
ctest --test-dir build-phase1 -C Debug --output-on-failure -R namc_linear_reference
ctest --test-dir build-phase1 -C Debug --output-on-failure
ctest --test-dir build-phase1 -C Debug -V -R namc_python_binding
cmake --build build-phase1 --config Release --parallel
ctest --test-dir build-phase1 -C Release --output-on-failure
```

Both builds passed with warnings treated as errors. Debug and Release each
passed all four CTest entries. The Python entry contains 18 passing pytest
cases, including the existing metadata binding test. Tests remain active in
Release; the C checks do not depend on `assert` or `NDEBUG`.

The local process environment contained duplicate case variants of `Path`.
CMake configure/build commands were launched through Python `subprocess.run`
with `env={k.upper(): v for k, v in os.environ.items()}` to avoid the known
MSBuild duplicate-environment-key failure. No machine environment was changed.
Pytest was installed only in the ignored local `.venv`; no production
dependency was added.

Coverage includes transform signs and power, linear flux/torque, energy-rate
balance, analytical RL and mechanical decay, rotating-frame integration,
inverter reconstruction at 360 angles, reference/voltage limits, nominal
decoupling, anti-windup recovery, numerical overflow, invalid model versions,
invalid sensors, and latched faults. Native simulator tests exercise positive,
negative and zero current references, load, mismatched nominal priors,
configuration replay, deterministic seed behavior, and invalid CLI options.

## Observed example

```text
.\build-phase1\Debug\namc_sim.exe --seed 42
```

Defaults were 2,000 steps of 50 us (0.1 s), 5 A q reference, 48 V bus, and
zero external load. Output included:

| Quantity | Observed value, rounded |
| --- | --- |
| Final d current | 0.00192564 A |
| Final q current | 4.99557147 A |
| Final mechanical speed | 13.33557694 rad/s |
| Whole-run dq error RMS | 0.25844488 A |
| Last-half dq error RMS | 0.00484084 A |
| Maximum dq current magnitude | 4.99557184 A |
| Duty range | 0.18487547 to 0.81512453 |

These numbers were read from the generated report, not embedded as simulated
results in the implementation. The whole-run error includes startup from
zero current. The tracking regression threshold is 0.06 A for tail RMS and
final per-axis error in its specified scenarios, not a real-motor claim.

A seed-1 refinement comparison of 2,000 x 50 us against 4,000 x 25 us gave
absolute final differences of approximately 0.00001340 A in q current and
0.00167019 rad/s in mechanical speed. It checks this scenario's discretization
sensitivity, not stability across arbitrary motors or gains.

## Outstanding checks and limitations

- GCC/Clang Linux CI for this patch has not been run locally or remotely.
- The proposed transform/controller conventions need human review in
  [ADR-0005](../adr/0005-linear-reference-conventions.md).
- No hardware, HIL, embedded compiler, real-time timing, characterization,
  nonlinear LUT, identification, or general robustness campaign was performed.
- No commit, push, pull request, or repository setting change is part of this
  implementation task. The local checkout remains on `main` as requested.
