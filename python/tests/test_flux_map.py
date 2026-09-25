"""Import, ownership, and native lookup tests; fixture is not measured data."""

import json
import math
import os
from pathlib import Path

import pytest

from fluxforge_namc import FluxMap, FluxMapError, FluxMapLimits


FIXTURE = Path(__file__).resolve().parents[2] / "tests/fixtures/affine_flux_map.json"
LIMITS = FluxMapLimits(1e-5, 1e-10, 1e-6)


def native(data=None):
    library = os.environ["NAMC_CORE_LIBRARY"]
    if data is None:
        return FluxMap.from_json(library, FIXTURE, limits=LIMITS)
    return FluxMap(library, data, LIMITS)


def test_native_map_queries():
    model = native()
    assert model.flux(1.5, -2.0) == pytest.approx((0.0537, -0.0074), abs=1e-14)
    assert model.jacobian(1.5, -2.0) == pytest.approx((0.003, 0.0004, 0.0004, 0.004), abs=1e-14)
    assert model.flux(4.0, 4.0) == pytest.approx((0.0636, 0.0176), abs=1e-14)


def test_input_buffers_are_copied_and_kept_alive():
    data = json.loads(FIXTURE.read_text(encoding="utf-8"))
    model = native(data)
    before = model.flux(1.0, 2.0)
    data["psi_d_Wb"][0][0] = math.nan
    data["id_A"].clear()
    del data
    assert model.flux(1.0, 2.0) == before


@pytest.mark.parametrize("current,status", [((4.01, 0), 3), ((0, -4.01), 3), ((math.nan, 0), 1)])
def test_native_lookup_rejection(current, status):
    with pytest.raises(FluxMapError) as error:
        native().flux(*current)
    assert error.value.status == status


@pytest.mark.parametrize("key,value", [
    ("schema_version", 2), ("schema_version", True), ("units", "mH"),
    ("convention", "power-invariant-dq"), ("id_A", [0]), ("psi_d_Wb", [[1]]),
    ("source_id", ""), ("source_id", "bad\0source"), ("evidence", "unknown"),
])
def test_import_schema_rejection(key, value):
    data = json.loads(FIXTURE.read_text(encoding="utf-8"))
    data[key] = value
    with pytest.raises(ValueError):
        native(data)


@pytest.mark.parametrize("key,value", [
    ("id_A", [0, 0, 4]), ("temperature_K", -1), ("declared_flux_error_Wb", math.nan),
    ("psi_d_Wb", [[math.inf] * 3] * 3), ("psi_q_Wb", [[0] * 3] * 3),
])
def test_native_validation_rejection(key, value):
    data = json.loads(FIXTURE.read_text(encoding="utf-8"))
    data[key] = value
    with pytest.raises(FluxMapError):
        native(data)


@pytest.mark.parametrize("text", ['{"x":1,"x":2}', '{"x":NaN}', '{"x":Infinity}'])
def test_json_parser_rejection(tmp_path, text):
    path = tmp_path / "invalid.json"
    path.write_text(text, encoding="utf-8")
    with pytest.raises(ValueError):
        FluxMap.from_json(os.environ["NAMC_CORE_LIBRARY"], path, limits=LIMITS)


def test_import_size_limit(tmp_path):
    path = tmp_path / "oversized.json"
    path.write_bytes(b" " * (8 * 1024 * 1024 + 1))
    with pytest.raises(ValueError, match="8 MiB"):
        FluxMap.from_json(os.environ["NAMC_CORE_LIBRARY"], path, limits=LIMITS)


def test_dataset_cannot_override_acceptance_policy():
    data = json.loads(FIXTURE.read_text(encoding="utf-8"))
    data["min_rcond"] = 0
    with pytest.raises(ValueError, match="unknown fields"):
        native(data)
