# Project Management

## System of record

GitHub is the intended public system of record:

- Issues hold scoped work, defects, research proposals, and decisions needing
  action.
- Pull requests hold reviewable changes and validation evidence.
- ADRs hold durable, consequential technical decisions.
- Milestones mirror roadmap phases.
- A GitHub Project may visualize status without replacing issue history.

## Recommended issue states

Use project fields or labels to distinguish:

```text
Inbox -> Needs triage -> Ready -> In progress -> In review -> Done
                         |              |
                         v              v
                       Blocked        Rejected
```

An issue is `Ready` only when its outcome, boundaries, and acceptance evidence
are sufficiently clear for implementation.

## Label taxonomy

Create labels gradually as issues appear. Recommended groups are:

- `type: bug`, `type: feature`, `type: research`, `type: documentation`,
  `type: maintenance`;
- `area: core`, `area: plant`, `area: magnetic`, `area: identification`,
  `area: control`, `area: safety`, `area: simulation`, `area: bindings`,
  `area: tooling`, `area: governance`;
- `status: needs-triage`, `status: blocked`, `status: needs-decision`;
- `risk: safety`, `risk: numerical`, `risk: compatibility`,
  `risk: security`;
- `good first issue` and `help wanted` only after acceptance criteria and
  maintainer support are available.

Avoid label proliferation before it reflects real project work.

## Milestones

Milestones use the capability phases in `ROADMAP.md`. An issue may target a
future milestone, but completion requires the phase's documented exit evidence.
Do not use dates as a substitute for unresolved technical dependencies.

## Research proposals

A research proposal should identify:

- hypothesis or question;
- observable inputs and hidden truth boundaries;
- baseline and comparison methods;
- dataset or simulation provenance;
- metrics and failure criteria;
- numerical and safety risks;
- expected reusable artifact.

Exploratory work may be accepted without promising integration into runtime
code.

## Governance review

When the Lead Maintainer reports material contributor growth, create a
governance-transition issue and ADR as specified in `GOVERNANCE.md`. This work
takes priority over adding informal permissions that are not reflected in the
governance model.
