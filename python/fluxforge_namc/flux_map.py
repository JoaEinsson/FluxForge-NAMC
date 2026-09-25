"""Experimental map import and lifetime management; all lookup/physics is in C."""

from __future__ import annotations

import ctypes as ct
import json
import os
from dataclasses import dataclass
from pathlib import Path


class _DQ(ct.Structure):
    _fields_ = [("d", ct.c_double), ("q", ct.c_double)]


class _Jacobian(ct.Structure):
    _fields_ = [(name, ct.c_double) for name in ("dd", "dq", "qd", "qq")]


class _Data(ct.Structure):
    _fields_ = [
        ("version", ct.c_uint), ("units", ct.c_uint), ("convention", ct.c_uint),
        ("nd", ct.c_size_t), ("nq", ct.c_size_t), ("count", ct.c_size_t),
        *[(name, ct.POINTER(ct.c_double)) for name in ("id_axis", "iq_axis", "psi_d", "psi_q")],
        ("evidence", ct.c_int), ("source_id", ct.c_char_p),
        ("temperature_k", ct.c_double), ("declared_flux_error_wb", ct.c_double),
    ]


class _Limits(ct.Structure):
    _fields_ = [(name, ct.c_double) for name in (
        "min_incremental_h", "max_reciprocity_error_h", "min_rcond",
    )]


class _Map(ct.Structure):
    _fields_ = [("data", _Data), ("limits", _Limits), ("prepared", ct.c_uint)]


@dataclass(frozen=True)
class FluxMapLimits:
    """Caller-owned acceptance thresholds; never loaded from untrusted map data."""

    min_incremental_h: float
    max_reciprocity_error_h: float
    min_rcond: float


class FluxMapError(ValueError):
    """Native map validation, domain, or numerical failure."""

    def __init__(self, status: int) -> None:
        self.status = status
        names = (
            "OK", "invalid input", "invalid map", "out of domain",
            "ill conditioned", "non-reciprocal", "non-passive incremental model",
        )
        super().__init__(f"Native flux map: {names[status] if 0 <= status < len(names) else status}")


def _number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("Map values must be numbers, not strings or booleans")
    return float(value)


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_constant(value):
    raise ValueError(f"Non-standard JSON number: {value}")


class FluxMap:
    """Own copied buffers for one immutable, validated C map.

    No native fitting, sensor acquisition, model activation, or ABI compatibility
    promise. Input arrays are copied; later caller edits cannot change the map.
    """

    def __init__(self, library_path: str | os.PathLike[str], data: dict, limits: FluxMapLimits):
        keys = {
            "schema_version", "units", "convention", "id_A", "iq_A", "psi_d_Wb", "psi_q_Wb",
            "evidence", "source_id", "temperature_K", "declared_flux_error_Wb",
        }
        if not isinstance(data, dict) or set(data) != keys:
            raise ValueError("Map has missing or unknown fields")
        if type(data["schema_version"]) is not int or data["schema_version"] != 1:
            raise ValueError("Unsupported experimental map schema version")
        if data["units"] != "SI" or data["convention"] != "amplitude-invariant-dq":
            raise ValueError("Expected SI units and amplitude-invariant-dq convention")
        if not isinstance(data["source_id"], str):
            raise ValueError("source_id must be text")
        self._source = data["source_id"].encode("utf-8")
        if not 1 <= len(self._source) <= 127 or b"\0" in self._source:
            raise ValueError("source_id must contain 1..127 UTF-8 bytes without NUL")
        evidence = {"analytical": 1, "simulation": 2, "fea": 3, "measured": 4}
        if not isinstance(data["evidence"], str) or data["evidence"] not in evidence:
            raise ValueError("Unknown evidence level")
        axes = []
        for key in ("id_A", "iq_A"):
            values = data[key]
            if not isinstance(values, (list, tuple)) or not 2 <= len(values) <= 256:
                raise ValueError("Each current axis must contain 2..256 samples")
            axes.append((ct.c_double * len(values))(*map(_number, values)))
        nd, nq = len(axes[0]), len(axes[1])
        tables = []
        for key in ("psi_d_Wb", "psi_q_Wb"):
            rows = data[key]
            if not isinstance(rows, (list, tuple)) or len(rows) != nd or any(
                not isinstance(row, (list, tuple)) or len(row) != nq for row in rows
            ):
                raise ValueError("Flux table rows/columns must match id_A/iq_A")
            tables.append((ct.c_double * (nd * nq))(*(_number(v) for row in rows for v in row)))
        self._buffers = (*axes, *tables)
        native_data = _Data(
            1, 1, 1, nd, nq, nd * nq, *self._buffers, evidence[data["evidence"]], self._source,
            _number(data["temperature_K"]), _number(data["declared_flux_error_Wb"]),
        )
        native_limits = _Limits(
            limits.min_incremental_h, limits.max_reciprocity_error_h, limits.min_rcond,
        )
        self._native = ct.CDLL(os.fspath(library_path))
        self._native.namc_flux_map_prepare.argtypes = [ct.POINTER(_Data), ct.POINTER(_Limits), ct.POINTER(_Map)]
        self._native.namc_flux_map_prepare.restype = ct.c_int
        self._native.namc_flux_map_lookup.argtypes = [ct.POINTER(_Map), _DQ, ct.POINTER(_DQ), ct.POINTER(_Jacobian)]
        self._native.namc_flux_map_lookup.restype = ct.c_int
        self._map = _Map()
        status = self._native.namc_flux_map_prepare(ct.byref(native_data), ct.byref(native_limits), ct.byref(self._map))
        if status:
            raise FluxMapError(status)

    @classmethod
    def from_json(cls, library_path, path, *, limits: FluxMapLimits):
        """Load bounded UTF-8 JSON with no duplicate keys or NaN/Infinity tokens."""
        maximum = 8 * 1024 * 1024
        with Path(path).open("rb") as stream:
            raw = stream.read(maximum + 1)
        if len(raw) > maximum:
            raise ValueError("Map JSON exceeds the 8 MiB import limit")
        data = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_reject_constant)
        return cls(library_path, data, limits)

    def _lookup(self, id_a: float, iq_a: float, derivative: bool):
        flux, jacobian = _DQ(), _Jacobian()
        status = self._native.namc_flux_map_lookup(
            ct.byref(self._map), _DQ(_number(id_a), _number(iq_a)), ct.byref(flux),
            ct.byref(jacobian) if derivative else None,
        )
        if status:
            raise FluxMapError(status)
        return flux, jacobian

    def flux(self, id_a: float, iq_a: float) -> tuple[float, float]:
        """Return psi_d, psi_q in Wb from the C interpolator."""
        flux, _ = self._lookup(id_a, iq_a, False)
        return flux.d, flux.q

    def jacobian(self, id_a: float, iq_a: float) -> tuple[float, float, float, float]:
        """Return dd, dq, qd, qq in H from the C interpolant derivatives."""
        _, j = self._lookup(id_a, iq_a, True)
        return j.dd, j.dq, j.qd, j.qq

    def to_data(self) -> dict:
        """Copy the accepted map into the experimental JSON shape for replay.

        This exports source-declared metadata, not a certificate of accuracy.
        Mutating the returned object cannot change the native map.
        """
        d = self._map.data
        return {
            "schema_version": d.version, "units": "SI",
            "convention": "amplitude-invariant-dq",
            "id_A": list(self._buffers[0]), "iq_A": list(self._buffers[1]),
            "psi_d_Wb": [list(self._buffers[2][i * d.nq:(i + 1) * d.nq]) for i in range(d.nd)],
            "psi_q_Wb": [list(self._buffers[3][i * d.nq:(i + 1) * d.nq]) for i in range(d.nd)],
            "evidence": ("invalid", "analytical", "simulation", "fea", "measured")[d.evidence],
            "source_id": self._source.decode("utf-8"),
            "temperature_K": d.temperature_k,
            "declared_flux_error_Wb": d.declared_flux_error_wb,
        }

    def _simulation_transport(self) -> str:
        """Private host bridge; the simulator revalidates everything in C."""
        d, limits = self._map.data, self._map.limits
        values = [
            d.version, d.units, d.convention, d.nd, d.nq, d.evidence, len(self._source),
            d.temperature_k, d.declared_flux_error_wb,
            limits.min_incremental_h, limits.max_reciprocity_error_h, limits.min_rcond,
            *self._source,
        ]
        for buffer in self._buffers:
            values.extend(buffer)
        return "NAMC_FLUX_TRANSPORT_V1\n" + "\n".join(format(v, ".17g") for v in values) + "\n"
