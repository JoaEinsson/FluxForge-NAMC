# Bounded Offline Current-PI Tuning

This Phase 3 increment automatically computes **local PI candidate gains**
from an accepted controller map and an independent resistance estimate. It
is a model-based startup design aid, not excitation-based motor identification,
online adaptation, full dynamic decoupling or a completed Phase 3.
See [proposed ADR-0008](../adr/0008-bounded-offline-pi-tuning.md).

## Equations and limits

`namc_current_tune` in `include/namc/current_tuning.h` accepts a prepared,
immutable controller map, read-only sample time/hard limits, and an explicit
request. It does not accept plant parameters or PI runtime state. All design
calculations are C; Python only transports inputs and reports.

At the chosen dq current, query flux and the full local interpolant derivative
`J = [[Jdd,Jdq],[Jqd,Jqq]]`. This is a derivative of the two global flux tables,
not their storage shape. Two guarded solves yield columns of `J^-1`:

```text
D = diag(Jdd, Jqq)               # proportional-gain shape, NOT the plant model
a = ||J^-1||_infinity
b = ||J^-1 D||_infinity
Kpd = omega * Jdd
Kpq = omega * Jqq
Kid = Kiq = omega * R_est
local_rate_sample = Ts * (R_est*a + omega*b)
```

Both `Jdq` and `Jqd` remain intact in these calculations and are reported.
No reciprocity averaging, finite differencing or matrix solve enters the fast
controller loop. The scalar PI relation is a standard starting point; this
local coupled adaptation still requires closed-loop tests. Provenance is
recorded in ADR-0008.

Estimate the steady voltage at the design point from the controller model:

```text
vd0 = R_est*id - electrical_speed*psi_q
vq0 = R_est*iq + electrical_speed*psi_d
headroom = Vdc/sqrt(3) - hypot(vd0, vq0)
Lmax = max(Jdd, Jqq)
```

Select the minimum of the requested bandwidth parameter and these caps:

| Cap | Upper bound on omega, rad/s |
| --- | --- |
| Configured bandwidth | `max_bandwidth` |
| Local sample-rate policy | `(max_rate_sample/Ts - R_est*a)/b` |
| Proportional gains | `max_kp/Lmax` |
| Integral gains | `max_ki/R_est` |
| Voltage/error budget | `voltage_fraction*headroom / (design_error*(Lmax + R_est*Ts))` |

All caps must be finite and positive; the result must reach `min_bandwidth`.
Invalid fields, rejected local models, exhausted voltage headroom, a sample
rate too low for the policy, or infeasible minimum bandwidth reject the
candidate. Output is unchanged on rejection: do not activate stale output.

The current design check requires `hypot(id,iq)+design_error <= current_limit`.
Bus and electrical speed must be inside the independently supplied limits.
The function never changes those limits, antiwindup gain, nominal magnetic
priors or controller state. It checks only the base fields used by the design;
the runtime still validates its entire controller configuration before enabling.

These caps are **local design constraints, not stability certification**.
`max_rate_sample` is restricted to `(0,0.25]`, an experimental policy, not a
universal sampled-control theorem. The voltage cap budgets proportional action
plus one integral increment for the specified error; it does not bound all
later integral accumulation, speed changes or the full startup transient.
The error budget is not a reference slew limiter. Runtime current/bus/speed
guards, voltage limiting and antiwindup retain authority.

## Execution and failure behavior

The simulator uses reference dq currents and its bus voltage as the design
point; `--tune-speed` is an explicit design speed, defaulting to zero. This
does not command the simulated motor speed. Gains are applied only after a
successful design, before controller reset/first sample, and remain fixed for
the whole run. This is not gain scheduling across the map.

The core design cost is bounded: one lookup and two scaled solves, no search,
allocation or plant simulation. It runs outside the fast path. Map domain,
conditioning and physics gates remain in force. Native result values are
0 success, 1 invalid input, 2 rejected model, 3 infeasible design.

Tuned experiments currently require `--model-failure disable`. Invalid tuning
aborts before activation with no success report. Runtime model failure still
disables/latches through the existing controller. Combining tuned gains with
nominal fallback is rejected until a separately qualified fallback configuration
and transition are implemented. Fixed-gain fallback remains available unchanged.

## CLI example and three-way comparison

Build in `build-phase3-tuning` using the [standard build options](build.md).
In PowerShell:

```powershell
$env:PYTHONPATH = "$PWD/python"
.\.venv\Scripts\python.exe -m fluxforge_namc `
  --executable build-phase3-tuning/Debug/namc_sim.exe `
  --library build-phase3-tuning/Debug/namc_core_binding.dll `
  --map tests/fixtures/nonlinear_flux_map.json `
  --min-incremental-h 1e-5 --max-reciprocity-error-h 1e-5 --min-rcond 1e-6 `
  --controller-map tests/fixtures/nonlinear_flux_map.json `
  --control-min-incremental-h 1e-5 --control-max-reciprocity-error-h 1e-5 --control-min-rcond 1e-6 `
  --tune-bandwidth 1500 --tune-resistance 0.4 `
  --compare-controllers --id -2 --iq 5 --load 0.2 --seed 42 --trace-stride 20
```

`--tune-bandwidth` is **rad/s, not Hz**. Requested bandwidth and estimated
resistance are mandatory whenever any tuning option is supplied; no value
is borrowed from hidden plant truth. Using equal plant/controller data or R
is an explicit known-model experiment, not identification.

Optional design policy defaults (identical in the Python wrapper and native
runner):

| CLI option | Default | Unit |
| --- | --- | --- |
| `--tune-min-bandwidth` | 10 | rad/s |
| `--tune-max-bandwidth` | 2000 | rad/s |
| `--tune-max-rate-sample` | 0.2 | dimensionless |
| `--tune-max-kp` | 10 | V/A |
| `--tune-max-ki` | 2000 | V/(A s) |
| `--tune-error` | 1 | A, dq vector-error budget |
| `--tune-voltage-fraction` | 0.5 | fraction of estimated headroom |
| `--tune-speed` | 0 | electrical rad/s |

With tuning enabled, `--compare-controllers` returns three complete reports:
`nominal`, `model_aware` (original fixed gains) and `tuned_model_aware`.
`metric_delta_model_minus_nominal` isolates the original compensation change;
`metric_delta_tuned_minus_fixed` isolates gain changes. The same hidden plant,
initial state, scenario and independent limits are used throughout. Do not
claim identical gains between the tuned and fixed runs.

Without tuning options, existing CLI/API behavior and fixed-gain reports are
unchanged. `--compare-linear` remains a distinct comparison of plants and
cannot be combined with a controller map.

## Python and replay

Pass `tuning=CurrentTuning(bandwidth=1500, resistance=0.4)` to
`run_flux_map_reference`, alongside explicitly prepared `controller_map`.
The `CurrentTuning` dataclass uses SI values and is not itself evidence of
validation: the C executable rejects unsupported/nonnumeric/nonfinite inputs.

Reports add `tuning.request` (all policy inputs), the operating point/bus,
selected bandwidth, unmodified `jacobian_H`, norms, headroom and cap bitmask.
The actual four gains remain in `controller`. Bit values are 1 bandwidth,
2 sample-rate policy, 4 Kp, 8 Ki, 16 voltage/error budget. Bits indicate every
cap below the request, not only the minimum/active constraint.

To replay, reconstruct both maps and their thresholds, preserve the scenario,
then reconstruct `CurrentTuning(**report["tuning"]["request"])`. Same-binary
replay is tested; no cross-platform bitwise identity or report/API compatibility
is promised. The seed only selects initial angle, not a random motor population.

See [executed evidence](phase3-tuning-evidence.md) for results and limitations.
