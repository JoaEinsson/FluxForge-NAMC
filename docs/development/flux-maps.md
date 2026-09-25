# Coupled Flux Maps and Nonlinear Plant

This initial Phase 2 slice implements rectangular coupled flux LUTs, import,
validation, bounded lookup and a simulation-only nonlinear plant. It does not
identify a real motor, fit data, optimize a map, or activate adaptive models.
The [roadmap](../../ROADMAP.md) remains capability-based and Phase 2 is still
in progress. See [ADR-0006](../adr/0006-coupled-lut-runtime-and-plant.md).

## Representation and ownership

`include/namc/flux_map.h` defines the experimental C API. Two current axes
are strictly increasing, with 2..256 finite samples each. Each flux table
contains `nd * nq` entries indexed by `d_index * nq + q_index`. Both outputs
depend on both currents; grid resolution is not restricted to 2x2.

Native preparation borrows arrays and metadata. The caller must supply their
actual lengths, retain the storage and keep it immutable for the lifetime of
the map. All-zero handles are unprepared. Preparation failure invalidates the
output handle; callers must not prepare over a live active model. There is
no adaptive model activation/rollback facility in this slice. Python copies
the input buffers and keeps them alive for the native handle.

Metadata records version, SI units, amplitude-invariant dq convention, source
identifier, evidence class, a single temperature in K and a source-declared
flux error bound in Wb. Currents are A, flux linkages Wb and derivatives H.
Temperature is metadata, not an interpolation axis or compensated effect.
The source-declared bound is not independently verified by the importer.

## Bilinear queries and physical gates

Within one cell, use normalized current coordinates `u,v` in [0,1]:

```text
f(u,v) = (1-u)*(1-v)*f00 + u*(1-v)*f10 + (1-u)*v*f01 + u*v*f11
df/did = ((1-v)*(f10-f00) + v*(f11-f01)) / cell_width_d
df/diq = ((1-u)*(f01-f00) + u*(f11-f10)) / cell_width_q
```

Apply this to both flux tables. Flux-only callers need not request a Jacobian.
No finite-difference probes or iterative fitting run in the lookup path.
Interior knots select the higher-current cell; the final knot selects the
last cell. The outer boundary is inclusive, but any value beyond it returns
`NAMC_FLUX_OUT_OF_DOMAIN`, with no extrapolation or silent clamping.

Offline preparation visits all four corners of every cell, including both
one-sided derivatives at shared boundaries. It rejects nonfinite values,
unsupported versions/units/conventions, malformed grids and these failures:

- Reciprocity: `abs(Jdq - Jqd) > max_reciprocity_error_h`.
- Incremental response: the symmetric part of `J`, minus
  `min_incremental_h * I`, is not positive definite.
- Conditioning: `rcond_inf(J) < min_rcond`, or a zero determinant.

The matrix is scaled by its largest absolute entry before determinant and
condition calculations. For `A = J/scale`, reciprocal condition is
`abs(det(A)) / (norm_inf(A) * norm_inf(adj(A)))`. No unguarded inversion is
performed. Runtime queries repeat these checks at the selected operating
point; corner checks alone are not a claim of a condition-number bound
everywhere inside a cell.

The symmetric part and reciprocity difference are affine within a cell, so
corner tests bound those criteria throughout the cell in exact arithmetic.
Reciprocity tolerance is an explicit allowance for interpolation/data error,
not permission to label an inconsistent map exactly conservative. The named
`NON_PASSIVE` diagnostic means failure of the incremental-response gate; a
passing map is not a proof of global passivity. Strict potential-consistent
fitting/interpolation remains an optional future alternative.

Lookup has two bounded binary searches (at most eight comparisons each) and
fixed-cost interpolation/checking. There is no heap allocation or I/O in the
core. A 33x33 map with two distinct axes stores 2,244 doubles (17,952 bytes
when double is 8 bytes), plus the descriptor. Timing and embedded suitability
have not been measured. Preparation scans the whole grid and belongs outside
the fast control path.

## Plant equations and failure semantics

`plant/include/namc/magnetic_plant.h` is outside the core's include path.
The plant owns its map and mechanical parameters. The PI controller does not
receive that map or read hidden magnetic truth.

```text
J = [[d(psi_d)/did, d(psi_d)/diq], [d(psi_q)/did, d(psi_q)/diq]]
rhs_d = vd - R*id + omega_e*psi_q
rhs_q = vq - R*iq - omega_e*psi_d
J * [did/dt, diq/dt] = [rhs_d, rhs_q]
Te = 1.5*p*(psi_d*iq - psi_q*id)
domega_m/dt = (Te - Tload - B*omega_m)/inertia
dtheta_e/dt = p*omega_m
```

The Jacobian is a local derived quantity used for this solve, not a replacement
for the global tables. Rotational, load and mechanical signs follow the
[linear reference](linear-reference.md). Fixed-step RK4 holds alpha/beta
voltage constant and rotates it at every intermediate stage.

Each RK stage and the final state must pass domain, finite-value and matrix
checks. Failure leaves the previous state unchanged and returns a diagnostic.
The simulation driver must stop on failure; retaining the old state is not a
validated control fallback. Derivative discontinuities at cell crossings may
reduce the effective integration order. Finite checks do not prove stability
for arbitrary time steps, controllers or operating conditions.

## Python import example

Build with the [usual CMake commands](build.md), then set `PYTHONPATH=python`
or install the package in a development environment. On Windows:

```python
from fluxforge_namc import FluxMap, FluxMapLimits

model = FluxMap.from_json(
    "build-phase2/Debug/namc_core_binding.dll",
    "tests/fixtures/affine_flux_map.json",
    limits=FluxMapLimits(
        min_incremental_h=1e-5,
        max_reciprocity_error_h=1e-10,
        min_rcond=1e-6,
    ),
)
print(model.flux(1.5, -2.0))
print(model.jacobian(1.5, -2.0))
```

On Linux use the explicitly built `libnamc_core_binding.so` path instead.
The fixture is an affine coupled parser/lookup oracle, not a measured motor
or nonlinear demonstration. The nonlinear fixture is generated separately
in `tests/flux_map_test.c`.

The experimental JSON schema is illustrated by that fixture. `psi_d_Wb` and
`psi_q_Wb` have one row per `id_A` value and one column per `iq_A` value. Units
must be `SI`, convention `amplitude-invariant-dq`, and schema version 1.
Evidence is `analytical`, `simulation`, `fea` or `measured`; unsupported/missing
fields, duplicate keys, malformed dimensions and nonstandard NaN/Infinity JSON
tokens are rejected. UTF-8 input is limited to 8 MiB. Native C additionally
rejects nonfinite numeric values and failed physical criteria. There is no
implicit unit conversion or standard-format compatibility promise.

Limits are provided separately by the caller, not imported from the file.
Dataset provenance, licensing, coordinate compatibility, coverage and declared
uncertainty must be checked by the contributor. A rectangular array does not
establish that every region was actually measured or sufficiently excited.

`namc_sim` defaults to the Phase 1 linear scenario and can now execute imported
maps through the Python runner, with replayable full-map reports and optional
traces. See [magnetic experiments](magnetic-experiments.md) for JSON selection,
CLI/Python usage and linear comparisons. The native oracle tests remain:

```text
ctest --test-dir build-phase2 -C Debug -V -R namc_coupled_flux_map
```

## Synthetic nonlinear oracle

The C tests sample a project-original polynomial fixture on a 33x33 grid
spanning [-8,8] A on both axes:

```text
psi_d = .05 + .003*id - 2e-6*id^3 - 1.2e-6*id*iq^2
psi_q = .004*iq - 3e-6*iq^3 - 1.2e-6*id^2*iq
```

These equations are the gradient of a known polynomial coenergy, used only
as an independent test oracle. Import and runtime lookup do not require
coenergy coefficients. Both differential self-inductances decrease with
current magnitude and both cross terms are nonzero away from the axes.
This polynomial is not asserted valid beyond the tested domain or representative
of a particular motor.

Preparation thresholds are 10 uH minimum incremental response, 10 uH allowed
reciprocity error and `1e-6` minimum reciprocal condition. The source-declared
8 uWb flux bound is checked at 1,024 held-out points, not certified globally.
The tests also bound sub-cell flux circulation, reject a stricter reciprocal
tolerance, compare the linear limit with the Phase 1 plant, and close the PI
loop against the nonlinear fixture without exposing the map to the controller.
See [executed evidence](phase2-evidence.md) for results.

## Sources and limitations

[MathWorks FEM-Parameterized PMSM](https://www.mathworks.com/help/simscape-electrical/ref/femparameterizedpmsm.html)
documents tabulated flux, current-dependent magnetic behavior and flux partial
derivatives. The sinusoidal torque convention is consistent with the existing
[PMSM reference](https://www.mathworks.com/help/simscape-electrical/ref/pmsm.html).
The interpolator, guarded solve, import code and fixtures were written for
this project; no third-party source code or motor dataset was imported.

No new production dependency, hardware acquisition, fitting, identification,
adaptive map update, optimization, or hardware-readiness claim is included.
Broader map-quality assessment, human review and CI evidence remain follow-up
work before declaring Phase 2 complete.
