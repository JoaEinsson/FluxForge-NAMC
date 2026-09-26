# ADR-0009: Separate Reserve Gains and Qualify Model-Loss Transitions

- Status: Proposed
- Date: 2026-09-26
- Decision owners: Lead Maintainer
- Related: [ADR-0007](0007-flux-map-current-compensation.md),
  [ADR-0008](0008-bounded-offline-pi-tuning.md), [Phase 3](../../ROADMAP.md)

## Context

ADR-0008 initially rejected tuned operation with nominal fallback because the
same configuration would otherwise reuse tuned gains in the degraded path.
The requested follow-up is separate reserve gains, transition evidence and
broader operating-point/timing evaluation. No hardware qualification is implied.

## Decision

- Extend the experimental model-current configuration with an optional borrowed,
  immutable `mapped_gains` pointer. Increment its version to 2; rebuild callers.
  NULL retains the original shared fixed-gain behavior. No stable ABI is promised.
- Keep `nominal` as the independently supplied reserve gains, magnetic priors,
  sample time, antiwindup and hard limits. Validate this complete configuration
  even while using mapped gains. Override only four gains in a bounded local copy
  for mapped operation; tuning never overwrites the reserve configuration.
- Preserve disable-on-model-failure as default. Explicit nominal fallback clears
  integrals once, uses reserve gains immediately and stays latched until reset.
  Do not retry a repaired map or read mapped gains while latched. Invalid sensors,
  numerical state, configuration or hard-limit violations still disable/latch.
- Retain the reset transition, not an unreviewed bumpless-transfer algorithm.
  Measure the resulting command jump and tracking transient. Qualification is
  limited to tested scenarios meeting stated criteria, not the whole map domain.
- Add host-only model-handle loss injection and initial mechanical speed to the
  native simulator. Keep the hidden plant unchanged. Add native sample-level
  overshoot, settling, peak-error and fallback diagnostics independently of trace
  decimation. Include failures and unmet criteria in the finite campaign.
- Make host timing opt-in and separate from deterministic physical metrics.
  Instrument controller dispatch only, not plant integration or JSON I/O.
  Report each observed path: nominal, mapped, first fallback and latched fallback.
  Use Windows performance counters or POSIX monotonic timestamps only in the
  simulator. No clock, OS dependency or allocation enters the portable core.

## Alternatives and consequences

Reusing tuned gains is not an independently designed reserve. Automatically
designing another fallback from the failing map would undermine that boundary.
Copying the old integral into a new controller is not necessarily bumpless:
gains and feedforward change. A future transfer design needs separate evidence.

The reserve PI is a degraded baseline, not a silent replacement for nonlinear
physics. Independent limits can disable dangerous inputs but do not guarantee
transient stability. Low bus/high back-EMF cases may be infeasible; reporting
them is preferable to relaxing current limits or hiding stopped runs.

Finite-window tests are not a stability theorem. Host maximum elapsed time is
not target WCET; preemption, timer/dispatch overhead and instrumentation remain.
Timing must not influence the numerical integration step or controller inputs.

## Validation and provenance

Native tests isolate each gain set and prove first-sample equivalence to a
fresh nominal controller, subsequent integral accumulation, latch/reset and
hard-limit authority. Python orchestrates native runs and independently checks
metrics from dense traces, replay, decimation independence and timer counts.
The campaign retains full input maps, scenario inputs, observed plant parameters,
build provenance, all results and rejection messages. See the
[qualification guide](../development/control-qualification.md).

Timer API contracts: [Microsoft performance counter documentation](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter)
and [POSIX clock functions](https://pubs.opengroup.org/onlinepubs/9799919799/functions/clock_gettime.html).
Only OS APIs are used; no vendor implementation or motor dataset was copied.
Human review/acceptance remains pending. This extends only ADR-0008's initial
fallback restriction, not its tuning equations or startup-only activation.
