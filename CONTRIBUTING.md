# Contributing to FluxForge-NAMC

Thank you for helping build FluxForge-NAMC. This is a motor-control engineering
project with safety-relevant long-term ambitions, so reproducibility,
physical correctness, numerical evidence, and honest capability claims matter
as much as code quality.

## Before contributing

Read:

1. `README.md` and `docs/project-charter.md` for project intent;
2. `ROADMAP.md` for current priorities;
3. `docs/safety-scope.md` for the present safety boundary;
4. `docs/development/workflow.md` for the development process;
5. `docs/development/ai-assisted-development.md` if any AI tool materially
   assists the contribution;
6. `CODE_OF_CONDUCT.md` and `DCO`.

## Language

Repository artifacts must be written in English. This includes source code,
comments, documentation, commit messages, issues, pull requests, ADRs, test
names, configuration, and generated reports intended for version control.

## Proposing work

- Use an issue template for bugs, features, or research proposals.
- Discuss large, safety-relevant, architecture-changing, or dependency-adding
  work before implementation.
- Keep each pull request focused on one coherent outcome.
- Do not present simulated results as hardware validation.

Early-stage implementation choices should follow the roadmap and be recorded
in an ADR when they constrain public APIs, physics conventions, file formats,
or future embedded portability.

## Development workflow

1. Create a short-lived branch from `main`.
2. Make the smallest coherent change that solves the accepted problem.
3. Add or update tests and documentation in the same pull request.
4. Run all relevant validation locally.
5. Review the diff for accidental files, secrets, generated outputs, and
   unsupported claims.
6. Open a pull request and complete every applicable checklist item.

Direct pushes to `main` are discouraged. The intended merge strategy is squash
merge after required checks and review pass.

## Technical expectations

When implementation begins:

- firmware-relevant reference algorithms belong in portable C11;
- public C symbols use the `namc_` prefix;
- critical Python code calls the C implementation instead of creating an
  independent algorithmic fork;
- the real-time core does not depend on operating-system, filesystem, network,
  Python, or MATLAB facilities;
- fast paths avoid heap allocation, blocking I/O, recursion, and unbounded
  iteration;
- SI units and coordinate-transform conventions are explicit;
- plant truth remains structurally inaccessible to the controller and
  identifier;
- coupled flux maps retain saturation and cross-saturation; incremental
  matrices are derived only where needed by an algorithm;
- NaN, infinity, singular matrices, and invalid models cannot propagate
  silently to modulation outputs.

Formatting tools and exact build commands will be documented when the first
source targets are added. Until then, documentation changes must at minimum be
reviewed for correct links, consistent terminology, and clean Git diffs.

## Commits

Use [Conventional Commits](https://www.conventionalcommits.org/) with concise,
imperative summaries. Examples:

```text
docs: define founder-led governance
feat(magnetic): add coupled flux-map lookup
test(transforms): verify power-invariant convention
```

### Developer Certificate of Origin

Every commit must be signed off to certify the Developer Certificate of Origin
1.1 in `DCO`:

```text
git commit --signoff
```

The resulting commit message must contain:

```text
Signed-off-by: Your Name <your.email@example.com>
```

The sign-off is a legal certification, not a cryptographic signature. Use an
identity you are authorized to submit publicly.

## AI-assisted contributions

AI assistance is permitted, but the human contributor remains responsible for
the entire contribution. Material assistance must be disclosed in the pull
request. Do not commit chat transcripts or prompts by default, and never submit
secrets, confidential information, or third-party code of uncertain provenance
to an AI service or to this repository.

AI-generated output must receive the same or greater review, testing,
licensing, and provenance checks as human-authored output. See the full
AI-assisted development policy for requirements.

## Dependencies and third-party material

- Prefer small, well-maintained, permissively licensed dependencies.
- Request approval before adding a production dependency.
- Record source, version, license, and modifications for imported material.
- Do not copy code from proprietary motor-control firmware, vendor SDKs,
  publications, or websites unless its license clearly permits the intended
  use and attribution requirements are satisfied.
- Future datasets, FEA maps, and calibration files require explicit provenance
  and licensing metadata.

## Pull-request acceptance

A contribution may be merged when it is in scope, reviewable, licensed,
appropriately tested, documented, and free of unresolved safety or provenance
concerns. Maintainers may require an ADR or additional evidence before merging
changes with long-term consequences.
