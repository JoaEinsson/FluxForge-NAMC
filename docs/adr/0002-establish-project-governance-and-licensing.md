# ADR-0002: Establish Project Governance and Licensing

- Status: Accepted
- Date: 2026-08-27
- Decision owners: Lead Maintainer
- Related: `GOVERNANCE.md`, `LICENSE`, `DCO`

## Context

The project needs clear ownership, contribution provenance, patent-aware
open-source terms, and a governance model that works for its current single
lead while remaining able to evolve as participation grows.

## Decision

FluxForge-NAMC will:

- license code and project documentation under Apache License 2.0;
- use `FluxForge-NAMC contributors` in the project copyright notice;
- require Developer Certificate of Origin 1.1 sign-off on every commit;
- begin with founder-led governance under `@JoaEinsson` as Lead Maintainer;
- explicitly review transition to a Technical Steering Committee when the Lead
  Maintainer reports material contributor growth or coordination pressure;
- keep future third-party datasets and imported maps under explicit,
  separately reviewed provenance and licensing metadata.

## Alternatives considered

- **MIT license:** simpler text, but lacks Apache-2.0's explicit patent grant.
- **GPL family license:** provides stronger copyleft, but may inhibit intended
  research and industrial reuse.
- **Contributor License Agreement:** can centralize relicensing authority, but
  adds process and legal friction that is not justified at the current scale.
- **Immediate committee governance:** adds ceremony before a stable maintainer
  group exists.

## Consequences

The project is permissively reusable under defined copyright and patent terms.
Contributors publicly certify their right to submit work. The Lead Maintainer
retains final authority initially, while the repository contains an explicit
signal and process for governance growth.

Apache-2.0 does not provide legal advice, guarantee freedom to operate, or
license third-party trademarks and patents outside its terms.

## Validation

Governance and licensing will be reviewed when contributor growth triggers the
transition process, before accepting material differently licensed assets, or
before any transfer to an organization or foundation.
