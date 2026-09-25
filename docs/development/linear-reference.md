# Linear Reference System

This is the first Phase 1 implementation slice: a simulation-only, linear PMSM
baseline for later comparison with coupled flux-linkage LUTs. It does not
implement saturation, cross-saturation, identification, or adaptive control.
All interfaces and the JSON report are experimental and may change.
See [ADR-0005](../adr/0005-linear-reference-conventions.md) for the proposed
conventions and [build instructions](build.md) for prerequisites.

## Components and information boundary

| Component | Responsibility | Inputs |
| --- | --- | --- |
| `namc_core` | Transforms, nominal PI control, limits, modulation | Phase currents, encoder angle/speed, bus voltage, references, nominal priors |
| `namc_plant` | Linear motor truth and average inverter | Applied stationary voltage, load torque, hidden parameters |
| `namc_sim` | Fixed scenario, ideal sensor adapter, metrics and report | CLI configuration and deterministic seed |
| Python | Launch C executable and read its report | Explicit executable path and scenario options |

The plant header lives under `plant/include`, outside the core's include path.
The core has no plant link dependency. The simulator reads state to synthesize
ideal sensors; it does not copy plant magnetic or mechanical parameters into
the controller. Its controller priors deliberately differ from the motor.
Pole-pair count is independently configured for the encoder adapter.

## Units, transforms and signs

Use A, V, Wb, H, ohm, s, N m, kg m^2 and rad/s. Phase currents sum to zero.
The amplitude-invariant convention is:

```text
alpha = (2*a - b - c)/3        beta = (b - c)/sqrt(3)
d = cos(theta)*alpha + sin(theta)*beta
q = -sin(theta)*alpha + cos(theta)*beta
alpha = cos(theta)*d - sin(theta)*q
beta  = sin(theta)*d + cos(theta)*q
a = alpha; b = -alpha/2 + sqrt(3)*beta/2; c = -alpha/2 - sqrt(3)*beta/2
p_abc = 1.5 * (vd*id + vq*iq)  [balanced three-wire signals]
```

`theta` is electrical angle, d aligns with PM flux, and `omega_e = p*omega_m`
for `p` pole pairs. Clarke alone projects out zero sequence; the controller
rejects phase-current sums exceeding `1e-6 * current_limit`. This tolerance
is for ideal sensors, not a proposed physical sensor diagnostic threshold.

## Plant and integration

```text
psi_d = Ld*id + psi_pm             psi_q = Lq*iq
did/dt = (vd - R*id + omega_e*psi_q)/Ld
diq/dt = (vq - R*iq - omega_e*psi_d)/Lq
Te = 1.5*p*(psi_d*iq - psi_q*id)
domega_m/dt = (Te - Tload - B*omega_m)/J
dtheta/dt = p*omega_m
```

Positive load torque opposes positive motor torque; negative rotation does
not automatically change the sign of a user-supplied load. Viscous friction
does oppose either rotation direction. The fixed-step RK4 solver holds
alpha/beta voltage constant and uses the intermediate rotor angle at every
stage. Electrical angle is wrapped to [0, 2*pi) after a successful step.
All stages and the resulting derivative must be finite before committing
state. Invalid parameters or numerical overflow leave the previous state
unchanged. These checks do not guarantee stability for arbitrary step sizes.

## Controller and inverter

Each sample measures currents, angle, speed and bus voltage before computing
the duty command, then advances the plant by one sample. There is no additional
sampling delay, noise, dead time, quantization, or PWM switching ripple.

```text
e = limited_current_reference - measured_dq_current
vd_raw = kp_d*ed + integral_d - omega_e*Lq_prior*iq
vq_raw = kp_q*eq + integral_q + omega_e*(Ld_prior*id + pm_flux_prior)
v_limited = radial_limit(v_raw, Vdc/sqrt(3))
integral_next = integral + dt*(ki*e + kaw*(v_limited - v_raw))
```

Integral voltage is also bounded to +/- `Vdc/sqrt(3)`; `0 <= kaw*dt <= 1` is
required. Gains have units V/A, V/(A s), and 1/s respectively. Reference
current magnitude is limited independently of voltage. Measured phase-current
peak and dq magnitude are both checked against the hard current limit.

After inverse Park/Clarke, common mode is `-(max(vabc)+min(vabc))/2` and each
duty is `0.5 + (vphase + common_mode)/Vdc`. Duties stay in [0, 1]. The average
inverter reconstructs phase-neutral voltages as `Vdc*(duty - mean(duties))`.
This exercises phase duties without simulating transistor switching.

Invalid inputs/configuration, unknown model version, overflow, excessive
measured current/speed, or out-of-range bus voltage latch a fault. The output
then has `enabled=0` and neutral numeric duties. An explicit reset clears the
latch; it is the caller's responsibility to resolve the cause before restart.
The low-level inverter models disabled output as zero applied voltage, not
physical open-circuit behavior. The executable aborts immediately on a control
fault instead of simulating a claim of safe hardware shutdown.

## Run and reproduce

Linux:

```text
./build/namc_sim --steps 2000 --seed 42 --dt 0.00005 --iq 5 --vdc 48 --load 0
```

Windows with the Visual Studio generator:

```text
.\build\Debug\namc_sim.exe --steps 2000 --seed 42 --dt 0.00005 --iq 5 --vdc 48 --load 0
```

Python (after installing the package or setting `PYTHONPATH=python`):

```python
from fluxforge_namc import run_linear_reference

report = run_linear_reference("build/namc_sim", seed=42)
print(report["metrics"])
```

The report records all fixed plant/controller parameters, variable options,
initial conditions, model/core versions, compiler/build configuration, and a configure-time Git
revision/dirty marker. Reconfigure after source changes. A dirty marker is
not a content hash: retain the source diff or commit before treating a run as
archival/reproducible evidence. No stable report serialization is promised.
The scenario also accepts `--id` (default 0 A) and `--trace-stride` (default 0,
disabled). `--plant linear` is the default; the map-backed mode and report
trace semantics are described in [magnetic experiments](magnetic-experiments.md).
Other parameters are fixed in the recorded source revision.

The seed selects only initial electrical angle with one modulo-2^32 LCG draw
(`1664525*seed + 1013904223`), scaled by `2*pi/2^32`. It is not a Monte Carlo
motor population. Identical binary, configuration and seed produce identical
reports; cross-compiler bitwise equivalence is not promised. RMS metrics use
post-step dq current errors; tail RMS covers the last half of samples.

CLI domains: 2..1,000,000 steps, 32-bit unsigned seed, 1e-7..1e-4 s sample time,
d/q references each +/-10 A with magnitude at most 12 A, 12..60 V bus, and
+/-2 N m load. Domain-valid input is not
a promise that every run remains within current/speed limits. A failed run
exits nonzero with a step diagnostic and emits no success JSON.

## Evidence and limitations

Tests use independent analytical values for transform power, flux/torque,
RL response and mechanical decay. The energy-rate balance includes copper
loss, friction, load power and magnetic/mechanical stored energy. Closed-loop
tests cover positive, negative and zero current, nominal-prior mismatch,
step-size refinement, duty bounds, and saturation recovery.

This is not a speed controller, nonlinear model, fault-tolerant supervisor,
embedded timing demonstration, hardware experiment, or validation of a real
motor. Acceptance thresholds apply only to these synthetic fixtures. Phase 2
adds a separate [coupled-map plant](flux-maps.md), not a change to the linear
baseline physics described here.

## Equation references and provenance

- [MathWorks PMSM equations](https://www.mathworks.com/help/sps/ref/pmsm.html)
  provide the amplitude-invariant dq baseline and torque convention.
- [MathWorks Park Transform](https://www.mathworks.com/help/mcb/ref/parktransform.html)
  describes the selected d-axis alignment and transform signs.

The C implementation and test fixtures were written for this project from
the stated equations; no third-party source code, motor datasets, or firmware
were imported. References are documentation sources, not runtime dependencies.
