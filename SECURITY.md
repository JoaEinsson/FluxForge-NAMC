# Security Policy

## Supported versions

FluxForge-NAMC is pre-alpha and has no stable release. Security fixes are
applied only to the current `main` branch until a versioned support policy is
published.

| Version | Supported |
| --- | --- |
| `main` | Yes, best effort |
| Unreleased snapshots and forks | No |

## Reporting a vulnerability

Do not open a public issue for a suspected security vulnerability.

Once private vulnerability reporting is enabled in the published repository,
use GitHub's private vulnerability reporting form:

<https://github.com/JoaEinsson/FluxForge-NAMC/security/advisories/new>

If the form is unavailable, do not disclose vulnerability details in a public
issue. Use a private contact method listed on the Lead Maintainer's GitHub
profile to request a secure reporting channel.

Include, when available:

- affected revision or version;
- impact and realistic threat scenario;
- reproduction steps or a minimal proof of concept;
- relevant configuration and platform information;
- suggested mitigation;
- whether the issue has been disclosed elsewhere.

Do not include secrets, personal data, or dangerous high-energy hardware
instructions that are unnecessary to reproduce the software issue.

## Response process

Maintainers will acknowledge and investigate reports as project capacity
allows. A validated report will be handled through a private advisory until a
fix and coordinated disclosure are ready. Credit is offered when desired and
appropriate.

This project does not currently promise a response-time or remediation-time
service level. If the repository becomes operationally critical, this policy
must be revised before that claim is made.

## Security versus functional safety

A cybersecurity vulnerability and a motor-control safety defect may overlap,
but they are not identical. Report exploitable software or supply-chain issues
through the private security channel. Report ordinary simulation defects in a
bug issue, unless public disclosure would create a credible safety or security
risk.

The project is not functionally safe, hardware validated, or road approved.
See `docs/safety-scope.md` for the complete boundary.

## Safe harbor intent

Good-faith research that respects privacy, avoids data destruction, limits
testing to systems the researcher owns or is authorized to test, and reports
findings responsibly is welcome. This statement is an expression of project
intent and is not legal advice or a grant of authorization over third-party
systems.
