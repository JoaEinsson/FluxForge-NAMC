# Development Workflow

## Principles

The workflow is designed for small, auditable changes with reproducible
evidence. `main` represents the latest accepted development state and may be
unstable before `1.0.0`.

## Branches and pull requests

- Create a short-lived branch from current `main`.
- Use a descriptive branch name such as `docs/governance-foundation` or
  `feat/nonlinear-coenergy`.
- Keep a pull request focused on one coherent result.
- Draft pull requests are encouraged for early architecture or research review.
- Prefer squash merge after required review and checks pass.
- Delete merged branches when they are no longer needed.

Direct pushes to `main` should be prevented by branch protection once the
repository is published and the required GitHub checks exist.

## Commit convention

Use Conventional Commits:

```text
<type>(optional-scope): imperative summary
```

Common types are `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `build`,
`ci`, `chore`, and `revert`. Mark incompatible changes with `!` and a
`BREAKING CHANGE:` footer when versioning begins.

Every commit must include a DCO `Signed-off-by` trailer.

## Review depth

Review effort follows risk:

- editorial documentation: link, terminology, and claim checks;
- build or tooling: reproducibility and platform checks;
- mathematics: derivation, units, conventions, tolerances, and independent
  test oracle;
- runtime control: timing bounds, saturation, finite-state validation, and
  fallback behavior;
- safety or limits: failure injection, authority boundaries, and independent
  reviewer when available;
- serialization or public API: compatibility and ADR review.

## Architecture decisions

Create an ADR for a decision that is costly to reverse or constrains multiple
subsystems. The ADR should be accepted before or with the implementing pull
request. See `docs/adr/README.md`.

## Definition of done

A change is done when:

- acceptance criteria are met;
- relevant tests and analysis pass;
- public behavior and limitations are documented;
- generated evidence is reproducible;
- licensing and provenance are clear;
- AI assistance is disclosed when material;
- the diff contains no unrelated artifacts or secrets;
- changelog and roadmap state are updated when applicable.

## Release policy

The repository begins pre-alpha with no release. Semantic Versioning will be
used when releases begin:

- `0.y.z` may change APIs as documented in release notes;
- `1.0.0` requires separately defined stability and validation criteria;
- a version number never implies hardware readiness or safety certification.

The first `0.1.0` candidate is associated with Roadmap Phase 0 exit criteria,
not merely the governance commit.

Release creation, tags, and publication require explicit Lead Maintainer
approval.
