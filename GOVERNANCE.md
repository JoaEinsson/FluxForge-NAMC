# FluxForge-NAMC Governance

## 1. Purpose

This document defines how the FluxForge-NAMC project makes decisions, assigns
responsibility, and evolves its governance. The initial model is intentionally
lightweight so that the project can move quickly while decisions remain
traceable and contributions remain reviewable.

## 2. Current governance model

FluxForge-NAMC begins as a founder-led project.

- **Lead Maintainer:** [@JoaEinsson](https://github.com/JoaEinsson)
- **Maintainers:** people granted merge and project-management authority by
  the Lead Maintainer.
- **Contributors:** anyone who submits accepted code, documentation, tests,
  data, analysis, or other project work.

The Lead Maintainer has final authority over project scope, releases,
maintainer appointments, governance changes, and unresolved technical
decisions. That authority is exercised in the open through issues, pull
requests, architecture decision records, and release notes whenever practical.

## 3. Responsibilities

### Lead Maintainer

The Lead Maintainer:

- protects the project mission and safety boundary;
- appoints and removes maintainers;
- resolves decisions that do not reach consensus;
- approves licensing, governance, security-policy, and release changes;
- decides when the project is ready to claim a new capability;
- initiates a governance transition when contributor growth requires it.

### Maintainers

Maintainers:

- review contributions for technical quality, licensing, safety, and scope;
- keep issue, roadmap, and release state accurate;
- enforce the Code of Conduct and security process;
- document significant technical decisions;
- avoid merging their own material changes without independent review when
  another maintainer is reasonably available.

### Contributors

Contributors:

- follow `CONTRIBUTING.md`, the Code of Conduct, and the DCO;
- provide enough evidence to review and reproduce their work;
- disclose material AI assistance;
- distinguish verified results from assumptions or simulated evidence;
- accept that maintainers may request changes or decline out-of-scope work.

## 4. Decision process

Routine decisions are made through pull-request review. Significant decisions
that affect architecture, public APIs, numerical conventions, safety
boundaries, compatibility, or long-term maintenance require an Architecture
Decision Record (ADR).

The project aims for reasoned consensus. A proposal should state the problem,
constraints, alternatives, consequences, and validation evidence. When
consensus is not available within a reasonable time, the Lead Maintainer makes
and records the decision.

The following changes always require explicit Lead Maintainer approval:

- project governance or licensing;
- security disclosure and Code of Conduct enforcement policy;
- claims of hardware readiness, vehicle readiness, or safety compliance;
- the first stable release or a change to compatibility guarantees;
- transfer of repository ownership or trademark control.

## 5. Maintainer selection

Maintainers are selected based on sustained, high-quality participation rather
than contribution count. Relevant signals include sound technical judgment,
constructive review, respect for project boundaries, reliability, and the
ability to separate experimental results from verified claims.

Appointments and changes to maintainer status are recorded in the repository.

## 6. Governance transition trigger

The project is expected to move from founder-led governance toward a small
Technical Steering Committee (TSC) when collaboration makes unilateral
coordination a bottleneck or creates a material concentration-of-authority
risk.

The transition review is explicitly triggered when the Lead Maintainer states,
to a human collaborator or an AI development agent, that a substantial number
of contributors are joining, that maintainer workload is becoming difficult to
centralize, or that a multi-maintainer governance model should be prepared.
Development agents must treat that statement as an instruction to surface and
prepare the governance transition; they must not silently defer it.

A transition proposal should normally consider:

- at least three active and trusted maintainers;
- documented nomination, voting, quorum, recusal, and removal rules;
- ownership of releases, security response, and Code of Conduct enforcement;
- time-bounded terms or periodic confirmation of TSC membership;
- a reserved role or defined tie-breaking mechanism for the founder during a
  documented transition period;
- an amendment process approved before the TSC takes authority.

Contributor count alone does not automatically change governance. The Lead
Maintainer initiates the review, and the resulting proposal is adopted through
an explicit pull request and ADR.

## 7. Conflicts of interest

Decision-makers must disclose material personal, employment, or financial
interests that could affect a project decision. They should recuse themselves
when impartial review would reasonably be questioned. The Lead Maintainer
appoints an alternate reviewer when possible.

## 8. Conduct and enforcement

All project spaces follow `CODE_OF_CONDUCT.md`. Conduct reports are handled
privately and separately from ordinary technical disagreement. Security
reports follow `SECURITY.md`.

## 9. Amendments

Governance amendments require a public pull request, a rationale, a reasonable
review period, and explicit approval from the current governing authority.
Material changes must be summarized in the changelog.
