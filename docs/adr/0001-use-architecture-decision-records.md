# ADR-0001: Use Architecture Decision Records

- Status: Accepted
- Date: 2026-08-27
- Decision owners: Lead Maintainer
- Related: Initial governance foundation

## Context

FluxForge-NAMC will make choices about nonlinear physics, numerical methods,
embedded constraints, identification, safety boundaries, and public
interfaces. Many choices will have several defensible alternatives, and their
rationale may otherwise be lost as human and AI-assisted contributions
accumulate.

## Decision

The project will record consequential and costly-to-reverse decisions as
Markdown ADRs under `docs/adr/`. ADRs use sequential numbers, explicit status,
context, decision, alternatives, consequences, and validation sections.

## Alternatives considered

- **Only issues and pull requests:** useful for discussion, but rationale can
  become fragmented or difficult to discover.
- **A single design document:** initially simple, but creates merge contention
  and obscures the lifecycle of individual decisions.
- **No formal record:** lowest immediate effort, but inappropriate for a
  long-lived safety- and physics-sensitive project.

## Consequences

Contributors must spend modest effort documenting major decisions. Reviewers
gain a stable rationale and can identify when a new proposal supersedes an old
one. ADRs do not prove correctness; tests and analysis remain required.

## Validation

The ADR process will be reviewed after the first five technical ADRs or when
contributors report that it is too heavy or insufficiently informative.
