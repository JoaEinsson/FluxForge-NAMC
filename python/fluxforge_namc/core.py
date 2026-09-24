"""Explicitly load and query the C core without duplicating its behavior."""

from __future__ import annotations

import ctypes
import os
from dataclasses import dataclass


class _NativeVersion(ctypes.Structure):
    _fields_ = [
        ("major", ctypes.c_uint),
        ("minor", ctypes.c_uint),
        ("patch", ctypes.c_uint),
    ]


@dataclass(frozen=True)
class CoreVersion:
    major: int
    minor: int
    patch: int


class CoreLibrary:
    """A caller-selected native library; no implicit search or build occurs."""

    def __init__(self, library_path: str | os.PathLike[str]) -> None:
        native = ctypes.CDLL(os.fspath(library_path))
        native.namc_core_get_version.argtypes = [ctypes.POINTER(_NativeVersion)]
        native.namc_core_get_version.restype = ctypes.c_int
        native.namc_core_version_string.argtypes = []
        native.namc_core_version_string.restype = ctypes.c_char_p
        self._native = native

    def version(self) -> CoreVersion:
        result = _NativeVersion()
        if self._native.namc_core_get_version(ctypes.byref(result)) != 1:
            raise RuntimeError("native core rejected a valid version output")
        return CoreVersion(result.major, result.minor, result.patch)

    def version_string(self) -> str:
        result = self._native.namc_core_version_string()
        if result is None:
            raise RuntimeError("native core returned a null version string")
        return result.decode("ascii")
