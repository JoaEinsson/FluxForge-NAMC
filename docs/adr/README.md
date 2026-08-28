# Architecture Decision Records

Architecture Decision Records (ADRs) capture consequential choices and their
tradeoffs. They explain why the project chose a path; they do not replace
implementation documentation or tests.

## When an ADR is required

Create an ADR when a decision materially affects more than one subsystem, is
costly to reverse, or establishes a durable convention. Examples include:

- coordinate transforms, signs, units, and power normalization;
- magnetic model families and derivative strategies;
- estimator or controller architecture;
- public C ABI, serialization, and compatibility policy;
- real-time memory and concurrency models;
- production dependencies;
- governance, licensing, or evidence policy.

Small local implementation choices do not need an ADR unless they accumulate
into a project constraint.

## Naming

Use sequential four-digit numbers and a concise kebab-case title:

```text
0003-select-magnetic-surface-backend.md
```

Numbers are never reused. A superseded ADR remains in history and links to its
replacement.

## Status

Use one of:

- `Proposed`
- `Accepted`
- `Rejected`
- `Deprecated`
- `Superseded by ADR-NNNN`

## Template

```markdown
# ADR-NNNN: Title

- Status: Proposed
- Date: YYYY-MM-DD
- Decision owners: role or handle
- Related: issue or pull-request links

## Context

What problem, constraints, and forces require a decision?

## Decision

What was decided?

## Alternatives considered

What credible options were considered and why were they not selected?

## Consequences

What becomes easier, harder, required, or intentionally deferred?

## Validation

How will the decision be tested or reconsidered?
```

An accepted ADR may be amended for clarity, but changing its decision requires
a new ADR that supersedes it.
