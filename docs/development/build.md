# Build and Test

This document covers the Phase 0 foundation and initial Phase 1/2/3 slices. It
builds the experimental, pre-1.0 core, shared binding library, separate linear
and coupled-map plants, native closed-loop simulator and tests. All motor
behavior is simulation-only; see the [linear reference](linear-reference.md)
and [magnetic experiment guide](magnetic-experiments.md).

## Prerequisites

- CMake 3.20 or newer;
- a C11 compiler (GCC and Clang are exercised in continuous integration);
- Python 3.10 or newer and pytest 8.x for optional binding/orchestration tests.

## Clean configure, build, and test

From a clean checkout, run:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DNAMC_WARNINGS_AS_ERRORS=ON -DNAMC_REQUIRE_PYTEST=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

To configure with Clang explicitly on Linux, use a separate build directory:

```text
CC=clang cmake -S . -B build-clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DNAMC_WARNINGS_AS_ERRORS=ON -DNAMC_REQUIRE_PYTEST=ON
cmake --build build-clang --parallel
ctest --test-dir build-clang --output-on-failure
```

Install pytest in a separate development environment before configuring. To
run only the C tests, omit `-DNAMC_REQUIRE_PYTEST=ON`; CMake then skips the
Python binding test when Python or pytest is unavailable. CI requires both.

The Python package is under `python/fluxforge_namc`. Its `CoreLibrary` takes
an explicit path to the built `namc_core_binding` shared library. It does not
build, search for, or silently replace native behavior. For example, after a
Linux build:

```python
from fluxforge_namc import CoreLibrary

core = CoreLibrary("build/libnamc_core_binding.so")
print(core.version_string())
```

For this example, install the package in a development environment or add
`python/` to `PYTHONPATH`. On Windows multi-configuration generators, build
with `--config Debug`, run CTest with `-C Debug`, and point `CoreLibrary` to
`build/Debug/namc_core_binding.dll` instead.

The `build` and `build-*` directories are local generated output and are
ignored by Git. The current API is experimental; a successful build is not
evidence of motor-control or hardware readiness.

The suite includes `namc_core_api`, `namc_linear_reference`, `namc_sim_smoke`,
`namc_coupled_flux_map`, `namc_model_current`, and `namc_python_binding` (including native simulator
orchestration and coupled-map import/query tests).
CTest passes explicit DLL and executable paths through `NAMC_CORE_LIBRARY`
and `NAMC_SIM_EXECUTABLE`; Python never substitutes its own motor equations.
Existing GCC/Clang CI jobs exercise these added targets without changing
their status-check names. Local Windows evidence is recorded
[separately](phase1-evidence.md).

The shared core also exports the experimental coupled flux-map API; the plant
target contains both linear and nonlinear simulation models. See the
[map guide](flux-maps.md) and [Phase 2 evidence](phase2-evidence.md). No extra
production dependency is needed for these additions.

The [first model-aware controller](model-aware-current.md) reuses core guards
and adds a separate accepted controller model to the experiment runner. Use
a fresh `build-phase3` directory with the same configuration options to keep
its binaries distinct from archived Phase 2 evidence.
