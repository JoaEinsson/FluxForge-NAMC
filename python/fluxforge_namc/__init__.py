"""Experimental, pre-1.0 bindings for the native FluxForge-NAMC core."""

from .core import CoreLibrary, CoreVersion
from .simulation import CurrentTuning, run_flux_map_reference, run_linear_reference
from .flux_map import FluxMap, FluxMapError, FluxMapLimits
from .resistance import ResistanceExperiment, run_resistance_identification
from .flux_identification import FluxPointExperiment, run_flux_identification, run_local_identification

__all__ = [
    "CoreLibrary", "CoreVersion", "run_linear_reference", "run_flux_map_reference",
    "FluxMap", "FluxMapError", "FluxMapLimits",
    "CurrentTuning",
    "ResistanceExperiment", "run_resistance_identification",
    "FluxPointExperiment", "run_flux_identification", "run_local_identification",
]
