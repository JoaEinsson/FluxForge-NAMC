# Safety Scope

## Current status

FluxForge-NAMC is pre-alpha, simulation-only research software. It has not been
validated on a physical motor, inverter, battery, dynamometer, or vehicle. It
is not certified and must not be treated as a safety mechanism.

## Four distinct meanings of safety

### Simulation safety

Simulation safety means experiments are bounded, deterministic where intended,
reproducible, and protected against invalid numerical state. It does not prove
that the same behavior will occur on hardware.

### Software constraints

The architecture will include independent checks for invalid measurements,
NaN/Inf, singular magnetic models, overcurrent, overvoltage, undervoltage,
overspeed, overtemperature, watchdog failures, and invalid modulation output.
Software checks reduce risk but cannot replace hardware protection.

### Hardware protection

A future physical system requires independently engineered gate-driver
protection, desaturation or short-circuit protection, fusing, contactors,
precharge, isolation, emergency shutdown, safe current and voltage measurement,
thermal protection, and a controlled test environment. This repository does
not currently provide or validate those systems.

### Functional safety

Functional safety requires a defined item, hazard analysis, safety lifecycle,
requirements traceability, independence, verification, validation, and
organizational evidence. The project makes no claim of ISO 26262 compliance or
certification.

## Authority boundary

Adaptive identification and optimization may request operating points. They
must not bypass, weaken, tune, or own fundamental protection limits. The
intended authority order is:

```text
optimizer request
      |
      v
adaptive confidence and operating limits
      |
      v
independent hard safety envelope
      |
      v
bounded current and duty command
```

An invalid adaptive model must lead to a validated fallback, derating, or
torque disablement. It must never be accepted merely to preserve performance.

## Evidence labels

Every result intended for publication must identify its evidence level:

- analytical;
- unit or numerical test;
- software simulation;
- hardware-in-the-loop;
- controlled bench or dynamometer;
- vehicle test.

Only the first three are presently in project scope. Claims must not imply a
higher evidence level.

## Future hardware work

Hardware work may enter scope only through an explicit governance decision,
updated safety documentation, reviewed test procedures, and clearly identified
responsible personnel. High-energy identification must require a controlled
bench and must never disable vendor or hardware protection.
