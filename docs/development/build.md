# Build and Test

This document covers the first Phase 0 implementation slice. It builds the
experimental, pre-1.0 `namc_core` metadata API, a shared library for the
Python binding, and their tests. No motor-control, plant, controller, or
hardware behavior is included.

## Prerequisites

- CMake 3.20 or newer;
- a C11 compiler (GCC and Clang are exercised in continuous integration);
- Python 3.10 or newer and pytest 8.x for the optional local binding test.

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
run only the C test, omit `-DNAMC_REQUIRE_PYTEST=ON`; CMake then skips the
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
