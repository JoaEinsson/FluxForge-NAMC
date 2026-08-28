# ADR-0003: Adopt English and Stable Project Names

- Status: Accepted
- Date: 2026-08-27
- Decision owners: Lead Maintainer
- Related: `CONTRIBUTING.md`, `AGENTS.md`

## Context

The project is intended for open international collaboration and needs stable,
collision-resistant names before public APIs and packages are created.

## Decision

- All version-controlled project artifacts will be written in English.
- The canonical project name is `FluxForge-NAMC`.
- The canonical subtitle is `Adaptive Nonlinear Motor Control & Identification
  Platform`.
- Public C symbols will use the `namc_` prefix.
- The planned CMake core target is `namc_core`.
- The planned Python package name is `fluxforge_namc`.

Human collaborators may converse in another language outside project artifacts
when that improves communication.

## Alternatives considered

- **Generic `mc_` prefix:** concise, but collision-prone and difficult to
  identify outside this repository.
- **`fluxforge_` for every C symbol:** distinctive, but unnecessarily long for
  embedded APIs.
- **Mixed-language artifacts:** accessible to some early contributors, but
  creates maintenance and search inconsistency.

## Consequences

Naming becomes predictable before implementation begins. Existing local source
briefs in another language remain outside version control unless translated and
reviewed as English project documentation.

## Validation

The names will be checked for practical conflicts before the first published
package or stable ABI. A change after that point requires a superseding ADR and
migration plan.
