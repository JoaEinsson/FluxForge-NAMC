"""Verify that Python observes values produced by the native C library."""

import os

from fluxforge_namc import CoreLibrary, CoreVersion


def test_native_core_version() -> None:
    core = CoreLibrary(os.environ["NAMC_CORE_LIBRARY"])

    version = core.version()
    assert isinstance(version, CoreVersion)
    assert version.major == 0
    assert core.version_string() == (
        f"{version.major}.{version.minor}.{version.patch}"
    )
