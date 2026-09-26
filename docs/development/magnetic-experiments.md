# Reproducible Magnetic-Plant Experiments

This Phase 2 delivery connects imported coupled flux maps to `namc_sim`.
One native C driver now runs either the linear reference or the nonlinear
current-state plant, with identical scenario options, measurement adapter,
nominal PI controller and independent limits. Python only imports, launches,
and packages results. No controller, integration or magnetic equations move
to Python.

All results remain **simulation-only**, including when a map declares measured
provenance. This is not motor identification or model-aware control. The
controller never receives the hidden plant map. The interfaces and report
formats are experimental, without a compatibility commitment.

## Run from PowerShell

Build using the [build guide](build.md), with both the executable and shared
library from the same build. The following assumes `build-phase2/Debug`:

```powershell
$env:PYTHONPATH = "$PWD/python"
.\.venv\Scripts\python.exe -m fluxforge_namc `
  --executable build-phase2/Debug/namc_sim.exe `
  --library build-phase2/Debug/namc_core_binding.dll `
  --map tests/fixtures/nonlinear_flux_map.json `
  --min-incremental-h 1e-5 --max-reciprocity-error-h 1e-5 --min-rcond 1e-6 `
  --id -2 --iq 5 --load 0.2 --seed 42 --steps 2000 --dt 0.00005 `
  --trace-stride 20 --compare-linear
```

This prints a JSON comparison containing both complete experiments and the
metric differences `LUT minus linear`. Omit `--compare-linear` to run only the
map. To run only the linear baseline, omit `--map`, `--library`, and the three
map thresholds. On Linux use the built `namc_sim`/`libnamc_core_binding.so`
paths and the environment's Python interpreter.

The checked-in 33x33 map is a project-original analytical fixture over
[-8,8] A, sampled from the polynomial in the [map guide](flux-maps.md).
Its nodes are checked against those equations in tests. It is not a measured
motor, FEA dataset, fitted model, or claim about a real motor. The thresholds
above apply to this fixture, not universally to imported motor data.

`--map` selects another JSON dataset using the same schema. All three
thresholds must be supplied explicitly by the caller, outside that file.
Map import and the native simulator each run C validation. The dataset
cannot silently choose its own acceptance policy. Use the guide's provenance,
unit/convention and coverage requirements before evaluating other data.

## Python and replay

```python
from fluxforge_namc import FluxMap, FluxMapLimits, run_flux_map_reference

library = "build-phase2/Debug/namc_core_binding.dll"
executable = "build-phase2/Debug/namc_sim.exe"
model = FluxMap.from_json(
    library, "tests/fixtures/nonlinear_flux_map.json",
    limits=FluxMapLimits(1e-5, 1e-5, 1e-6),
)
report = run_flux_map_reference(
    executable, model, id=-2, iq=5, load=0.2, seed=42, trace_stride=20,
)
print(report["metrics"])

restored = FluxMap(
    library, report["plant"]["map"],
    FluxMapLimits(**report["plant"]["map_limits"]),
)
replay = run_flux_map_reference(
    executable, restored, steps=report["steps"], seed=report["seed"],
    dt=report["dt_s"], id=report["id_reference_A"], iq=report["iq_reference_A"],
    vdc=report["vdc_V"], load=report["load_Nm"], trace_stride=report["trace_stride"],
)
assert replay == report
```

Reports embed the **entire accepted table**, provenance metadata and separate
validation thresholds, not just a mutable file path or source label.
`FluxMap.to_data()` also returns an independent copy in the import schema.
Initial conditions, all variable options, fixed electrical/mechanical and
controller parameters, core version, compiler, build configuration and the
configure-time revision/dirty marker are recorded. Reconfigure after changes;
retain the binary/source patch for a dirty build. A dirty marker is not a
source-content hash. Bitwise replay is tested with the same binary, not
promised across compilers or platforms.

## Scenario and log semantics

- `id` and `iq`: constant d/q references, each within +/-10 A and together
  within the independent 12 A magnitude limit. Oversized requests are rejected,
  not silently compared against a clipped reference.
- `steps`: 2..1,000,000; `dt`: 1e-7..1e-4 s; `seed`: unsigned 32-bit.
- `vdc`: 12..60 V; `load`: signed +/-2 N m, with the Phase 1 sign convention.
- The map must support both zero initial currents and the requested reference.
  This is not proof that every transient intermediate point will be supported.
- Resistance 0.4 ohm, inertia 0.01 kg m^2, friction 0.001 N m s/rad and four
  pole pairs are fixed scenario parameters for both plants. Map temperature
  remains metadata; it does not change these parameters or model losses.
- `trace_stride=0` disables the trace. A positive stride records each Nth
  completed step and always the final step. There is no initial trace row;
  initial state is recorded separately. A maximum of 10,000 rows is enforced
  before execution, with no silent truncation.

Trace currents, speed, angle and torque are **post-step plant truth for
evaluation**, not a sensor/identifier input interface. Each row's duties and
alpha/beta voltage describe the preceding held integration interval, not the
next command. Torque is computed by the selected native plant at the logged
state. No torque truth is handed to the controller.

Metrics use every completed step, independent of decimation: dq tracking
error RMS, last-half tracking RMS, maximum current magnitude, duty extrema
and maximum absolute torque. Final state includes torque. Traces are stored
in bounded host-only static memory and printed only after a successful run;
they add no allocation or I/O to portable controller/plant stepping.

`--compare-linear` compares **different plants under the same nominal PI**,
not different controllers on the same motor. Magnetic parameters differ;
there is no inference of efficiency, controller superiority, learning or
real-motor fidelity from a larger torque or different tracking error.

## Failure and transport boundaries

Invalid options, unsupported initial/reference points or rejected maps exit
with code 2. Runtime domain, conditioning, numerical or hard-limit failures
stop immediately with code 1, a stage/step diagnostic and no success JSON.
Map-domain errors are not extrapolated and do not switch silently to a linear
plant. The wrapper raises `CalledProcessError` and enforces a 60-second host
timeout. Neither stopping the simulator nor retaining the previous RK state
is a validated hardware shutdown/fallback procedure.

The native `--plant lut` option consumes a **private host transport** on stdin,
emitted by the Python importer; it does not parse a JSON file itself. The
versioned numeric stream contains dimensions, metadata, separate thresholds,
source UTF-8 bytes, axes and row-major tables. The C reader bounds tokens,
dimensions and storage, rejects missing/extra/nonfinite data, then prepares
the map again. Direct native JSON diagnostics encode source bytes as hex;
the Python wrapper restores `source_id` as UTF-8. This bridge is not the
future persistent/embedded serialization format or a CRC/activation protocol.

The simulator now reserves two independent map slots (each with two 256x256
tables and two axes) and 10,000 trace rows, roughly 3 MB on the tested host,
even with tracing off. The second slot supports the explicit, separate
[model-aware controller](model-aware-current.md); nominal PI remains the default.
That is host harness memory, not a claimed embedded footprint. Current
limits/guards do not establish stability for every accepted scenario.

## Validation and remaining work

Tests exercise strict import/transport, full-map replay, provenance escaping,
linear-limit equivalence, nonlinear forward/reverse/zero tracking, time-step
refinement, trace metrics/decimation and runtime domain rejection. See
[Phase 2 evidence](phase2-evidence.md) for observed results.

Broader map-quality/operating-region assessment, independent review and CI
remain required before Phase 2 closure. Map-aware control, identification,
fitting, candidate correction/activation and hardware integration are not
implemented by this Phase 2 delivery. The first separate Phase 3 slice adds
flux-map compensation and same-plant controller comparisons as linked above.
