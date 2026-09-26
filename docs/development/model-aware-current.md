# Model-Aware Current Control: First Slice

This first Phase 3 delivery adds **fixed-gain PI with coupled flux-based
rotational compensation**. It does not yet implement automatic gain tuning,
Jacobian-based dynamic decoupling, identification or adaptive map correction.
See the [proposed ADR-0007](../adr/0007-flux-map-current-compensation.md).

## Controller boundary and equations

The experimental API in `include/namc/model_current.h` receives an independently
prepared controller map, measured phase currents, encoder angle/speed, bus
voltage and requested dq currents. It has no dependency on plant headers or
hidden plant state. C remains the only runtime controller implementation.

Using the units and signs of the [linear reference](linear-reference.md),
the existing plant equation contains rotational voltage `[-omega_e*psi_q,
+omega_e*psi_d]`. The model-aware controller compensates those terms:

```text
current = Park(Clarke(measured_phase_currents), measured_angle)
e = independently_limited_reference - current
[psi_d, psi_q] = accepted_controller_map(current.d, current.q)
vd_raw = kp_d*e.d + integral_d - omega_e*psi_q
vq_raw = kp_q*e.q + integral_q + omega_e*psi_d
```

The full flux surfaces retain both current dependencies. No constant-Ld/Lq
replacement, numerical differentiation or matrix inversion is used for this
compensation. Map lookup still applies its existing finite/domain/physical/
conditioning gates. The local current dynamics remain coupled and nonlinear;
this is not complete dynamic decoupling or proof of closed-loop stability.

Both PI variants share the same internal guard/limiter/antiwindup kernel.
Gains, current/bus/speed limits, balanced-phase tolerance and nominal fallback
priors are configured independently of the map. Voltage is limited to
`Vdc/sqrt(3)`. The initial nominal API remains available and preserves its
original arithmetic. Parameters are fixed; no map-driven gain scheduling or
auto-tuning is performed.

## Failure policy

`NAMC_MODEL_DISABLE` is the default policy: a failed lookup returns
`NAMC_MODEL_REJECTED`, records its native flux status, emits neutral disabled
duties and latches a fault. Subsequent calls remain disabled.

`NAMC_MODEL_NOMINAL_FALLBACK` must be explicitly selected. On the first lookup
failure it clears PI integrals once and runs the nominal PI under the same
independent limits. The fallback latch and first model-failure code remain
observable. There is no automatic map retry, including after replacing the
map pointer. Switching a latched fallback policy to disable stops output.

Only `namc_model_current_reset` clears the fault/fallback and integrals; the
caller must resolve the cause and validate the operating conditions first.
Invalid sensors, phase imbalance, invalid controller configuration/state,
nonfinite arithmetic and current/bus/speed violations **never** use fallback
to continue. Resetting integrals is not a bumpless transfer. These are software
simulation behaviors, not a certified supervisor or physical inverter safe
state. Supported reference/current limits do not guarantee transient stability.

## Same-plant comparison

After building both executable and library from the same source, run in
PowerShell (the map is synthetic, not measured data):

```powershell
$env:PYTHONPATH = "$PWD/python"
.\.venv\Scripts\python.exe -m fluxforge_namc `
  --executable build-phase3/Debug/namc_sim.exe `
  --library build-phase3/Debug/namc_core_binding.dll `
  --map tests/fixtures/nonlinear_flux_map.json `
  --min-incremental-h 1e-5 --max-reciprocity-error-h 1e-5 --min-rcond 1e-6 `
  --controller-map tests/fixtures/nonlinear_flux_map.json `
  --control-min-incremental-h 1e-5 --control-max-reciprocity-error-h 1e-5 --control-min-rcond 1e-6 `
  --compare-controllers --id -2 --iq 5 --load 0.2 --seed 42 --trace-stride 20
```

The two paths are deliberately explicit even for a known-model test. The
simulator stores their decoded arrays separately; omitting the controller
dataset cannot borrow the hidden plant map. Different accepted maps can be
supplied to evaluate model mismatch. `--model-failure nominal` explicitly
enables fallback; omitting it disables on model error. Malformed/rejected
datasets abort at import/startup rather than being activated for fallback.

`--compare-controllers` compares **the same hidden plant under two controllers**.
It is mutually exclusive with the Phase 2 `--compare-linear` plant comparison
when a controller map is supplied. Reports contain `nominal`, `model_aware`
and `metric_delta_model_minus_nominal`. Values are generated at runtime, not
fixed expected results.

In Python:

```python
from fluxforge_namc import FluxMap, FluxMapLimits, run_flux_map_reference

library = "build-phase3/Debug/namc_core_binding.dll"
fixture = "tests/fixtures/nonlinear_flux_map.json"
plant = FluxMap.from_json(library, fixture, limits=FluxMapLimits(1e-5, 1e-5, 1e-6))
accepted = FluxMap.from_json(library, fixture, limits=FluxMapLimits(1e-5, 1e-5, 1e-6))
report = run_flux_map_reference(
    "build-phase3/Debug/namc_sim.exe", plant, controller_map=accepted,
    id=-2, iq=5, load=0.2, seed=42, trace_stride=20,
)
```

Without `controller_map`, the runner keeps the original nominal controller.
Thresholds are separate caller-owned policies, not imported from map metadata.
Do not assume these fixture thresholds suit a real dataset.

## Diagnostics and replay

Reports retain the existing scenario/plant/build/seed fields and add:

- `controller.kind`: `nominal-pi` or `flux-map-pi`.
- `controller.model_failure_policy`: `disable` or `nominal`.
- `controller_model`: complete accepted controller map and its validation
  thresholds, separate from `plant.map`/`plant.map_limits`.
- `model_diagnostics.first_fallback_step_zero_based`: controller iteration
  starting at 0; -1 means fallback was never entered.
- `model_diagnostics.flux_status`: first rejected lookup, or 0 if none.
- `metrics.fallback_steps`: number of completed iterations using fallback.
- `metrics.voltage_limit_steps`: iterations with applied alpha/beta voltage
  magnitude at least `(Vdc/sqrt(3))*(1-1e-12)`. This boundary-occupancy metric
  is evaluated identically for both controllers; it does not prove how much
  requested voltage was clipped.
- `trace[].controller_mode`: 0 nominal, 1 flux-map, 2 latched nominal fallback.
  This describes the controller used over the preceding integration interval.

Reconstruct both `FluxMap` objects from their recorded `map`/`map_limits`, then
replay with the same scenario and `controller.model_failure_policy`. Same-binary
replay is tested; cross-platform bitwise identity is not promised. The private
stdin bridge reads one block per requested model, in plant/controller order,
then enforces end-of-stream. No extra dataset is accepted silently.

Host-only storage now reserves two maximum-size map slots and the bounded
trace, roughly 3 MB on the tested host. This is not an embedded footprint.
The runtime core adds one bounded lookup per mapped iteration and, on the
single fallback transition, a second bounded guard/nominal pass. No heap,
blocking I/O, recursion or unbounded iteration is added to the control path.

## Remaining Phase 3 work

Bounded automatic tuning, derivative-based dynamic compensation if selected,
timing instrumentation, wider stability/operating-region assessment and
independent review remain outstanding. No runtime map learning/activation,
motor identification, hardware validation or general fallback guarantee is
claimed. See [executed evidence](phase3-evidence.md).
