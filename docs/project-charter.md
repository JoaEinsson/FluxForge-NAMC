# Project Charter

## Identity

**Project:** FluxForge-NAMC

**Subtitle:** Adaptive Nonlinear Motor Control & Identification Platform

**Repository:** <https://github.com/JoaEinsson/FluxForge-NAMC>

## Mission

FluxForge-NAMC exists to research and implement a portable platform that can
observe a partially known motor and inverter, identify their effective
electromagnetic behavior, build and validate a nonlinear magnetic model,
configure control from that model, and optimize operation within independent
electrical, thermal, battery, and safety limits.

The intended end state is not a single tune for a single motor. It is a
repeatable process for learning and safely using the observable behavior of a
defined class of motor-drive systems.

## Defining technical principle

The primary model must not be reduced to constant `Ld`, constant `Lq`, constant
stator resistance, and constant permanent-magnet flux. Those assumptions may
exist as baselines, analytical fixtures, and conservative fallbacks.

The central representation is an effective nonlinear magnetic model, ideally
derived from coenergy:

```text
W'(id, iq) -> psi_d, psi_q -> [ Ldd  Ldq ]
                           -> [ Lqd  Lqq ]
```

The full local incremental matrix is a first-class input to identification,
control, torque estimation, and optimization. Saturation and cross-saturation
must be representable and measurable. Temperature and electrical-angle
dimensions may be introduced progressively without invalidating public APIs.

## System concept

```text
partially known motor and inverter
                |
                v
      vehicle-observable signals
                |
                v
      bounded identification process
                |
                v
  validated nonlinear magnetic model
                |
                v
 local flux and 2x2 linearization
                |
                v
 model-aware current and torque control
                |
                v
 efficiency, thermal, and battery optimizer
                |
                v
 independent hard safety envelope
```

## Structural requirements

- The simulated plant owns hidden truth that the controller and identifier
  cannot access directly.
- Firmware-relevant reference algorithms are written in portable C11.
- Python orchestrates and analyzes the C implementation rather than replacing
  it with an independent critical implementation.
- The portable core has no dependency on an OS, filesystem, networking,
  Python, MATLAB, or a vendor HAL.
- Fast runtime paths use bounded execution and preallocated memory.
- Global model fitting and candidate validation are separated from the fast
  local control path.
- Adaptive components never acquire authority over hard safety constraints.
- All experiments are reproducible from version, configuration, and seed.
- Claims are backed by generated evidence and identify whether evidence is
  analytical, simulated, HIL, bench, or vehicle-derived.

## Initial platform scope

The first functional platform targets Linux simulation with GCC and Clang,
portable C libraries, simple Python bindings, deterministic tests, and
reproducible experiments. MATLAB/Simulink R2023b is an optional future
validation environment, not a dependency.

Embedded HAL skeletons, real-time deployment, and physical inverter support
follow only after the mathematical and safety architecture is validated in
simulation.

## Initial proof objectives

The first major proof of concept should show that a controller can:

1. receive only realistic measurements from a hidden nonlinear virtual motor;
2. begin with an inaccurate but conservative prior model;
3. execute bounded low-energy identification;
4. estimate local resistance, flux, and full incremental magnetic behavior;
5. refine a physically coherent model with confidence information;
6. auto-configure control and maintain stable, limited operation;
7. fall back deterministically when identification or model validation fails.

Later proofs will evaluate randomized motor populations and compare a constant
parameter baseline, a known nonlinear model, and an identified adaptive model
under saturation, temperature change, battery sag, and inverter error.

## Non-goals for the current project stage

FluxForge-NAMC is not currently:

- a production ECU or inverter firmware;
- a road-ready or certified control system;
- an ISO 26262-compliant product;
- a VESC, OpenInverter, or proprietary controller clone;
- a sensorless-control project;
- a neural-network or reinforcement-learning controller;
- an attempt to uniquely recover motor geometry, steel grade, or a physical
  B-H curve from terminal measurements;
- a MATLAB-only implementation.

## Decision priorities

When tradeoffs conflict, use this order:

1. physical correctness;
2. numerical correctness;
3. safety architecture;
4. testability;
5. portability;
6. determinism;
7. performance;
8. convenience.

## Success and claim discipline

The project advances by satisfying explicit phase exit criteria in the
roadmap. A capability is not complete because an interface or placeholder
exists. Simplifications and ignored dimensions must be documented. Results
must come from executable experiments, and negative or baseline-favoring
results must not be hidden.
