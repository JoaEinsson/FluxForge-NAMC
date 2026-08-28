# Repository Setup

This checklist records GitHub settings that cannot be delivered through a
local commit. The Lead Maintainer should apply and verify them after the initial
commit is reviewed and pushed.

## Repository identity

- Set the description to `Adaptive Nonlinear Motor Control & Identification
  Platform`.
- Set the default branch to `main`.
- Add appropriate topics such as `motor-control`, `electric-motors`,
  `nonlinear-control`, `system-identification`, `embedded-c`, and `simulation`.
- Keep the repository marked pre-alpha in its README and releases.

## Security

- Enable private vulnerability reporting.
- Enable secret scanning and push protection when available for the repository.
- Keep the default workflow token read-only unless a specific workflow has a
  reviewed need for additional permissions.
- Do not add repository or environment secrets until a concrete workflow needs
  them.

## Branch protection

Protect `main` after the first CI run establishes the check names:

- require a pull request before merging;
- require the `Repository policy` status check;
- require conversation resolution;
- block force pushes and branch deletion;
- require linear history if squash merge is the only merge strategy;
- apply rules to administrators when practical;
- increase required reviewer count when additional maintainers join.

The initial founder-led stage may use one approving reviewer when an independent
reviewer is available. This must not be interpreted as permission for an AI
agent to approve its own work.

## Contribution infrastructure

- Create the label taxonomy in `project-management.md` as real issues require
  it.
- Create milestones that mirror `ROADMAP.md` phases.
- Enable squash merge and disable merge strategies the project does not use.
- Configure a DCO sign-off check before accepting external contributions.
- Enable GitHub Discussions only when maintainers are ready to support it.

## Governance growth

When the Lead Maintainer reports material contributor growth, follow the
transition trigger in `GOVERNANCE.md` before distributing informal repository
permissions. Update CODEOWNERS, reviewer requirements, and security ownership
as part of the reviewed governance transition.

## Verification record

Remote settings should be verified periodically and after ownership,
maintainer, or GitHub plan changes. Changes with governance or security impact
should be summarized in an issue or pull request even when GitHub stores an
administrative audit log.
