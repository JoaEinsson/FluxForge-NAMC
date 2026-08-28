# Repository Instructions for Development Agents

## Project identity

FluxForge-NAMC is an Adaptive Nonlinear Motor Control & Identification
Platform. Its defining feature is a physically coherent nonlinear magnetic
model whose local flux and full 2x2 incremental matrix are used by
identification, control, and optimization.

## Read before changing the repository

For every material task, read the files relevant to the task, starting with:

1. `README.md` and `docs/project-charter.md`;
2. `ROADMAP.md` and the issue or request in scope;
3. `CONTRIBUTING.md` and `docs/development/workflow.md`;
4. `docs/safety-scope.md` for control, plant, identification, limits, hardware,
   or claims;
5. `docs/development/ai-assisted-development.md`;
6. applicable ADRs under `docs/adr/`.

Do not treat planned roadmap text as implemented behavior.

## Working agreements

- Write all repository artifacts in English, including code, comments, docs,
  configuration, commit messages, issues, test names, and reports.
- Preserve user work and unrelated changes. Inspect before editing.
- Prefer a small, reviewable patch that completes the requested outcome.
- State assumptions and record consequential choices in an ADR.
- Add or update tests and documentation with behavioral changes.
- Run the narrowest relevant checks first, then the complete applicable suite.
- Report commands run, evidence observed, limitations, and work not performed.
- Never hard-code experiment results or claim validation that was not run.
- Do not add production dependencies without explicit approval.
- Do not push, publish, release, change repository settings, or contact third
  parties unless the task explicitly authorizes it.

## Technical invariants

- Firmware-relevant reference algorithms are portable C11, with reasonable C99
  compatibility where practical.
- Public C symbols use the `namc_` prefix.
- Python orchestrates or binds to C for critical algorithms; it does not become
  a silent independent reference implementation.
- The portable core must not depend on an OS, filesystem, networking, threads,
  Python, MATLAB, or a vendor HAL.
- Runtime-critical paths avoid heap allocation, blocking I/O, recursion, and
  unbounded iteration.
- Use explicit SI units and documented transform/sign conventions.
- Keep controller, plant truth, platform code, bindings, tests, and tools
  structurally separate.
- Never expose hidden plant parameters to controller or identifier code.
- Do not reduce the primary magnetic model to constant `Ld`/`Lq` or force
  `Ldq`/`Lqd` to zero. Linear models are baselines and fallbacks only.
- Validate determinants, domains, finite values, limits, and model versions.
  Invalid numerical state must not propagate silently to PWM or duty outputs.
- Adaptive estimation and optimization remain subordinate to independent hard
  safety constraints and validated fallback behavior.
- No hardware, road-use, certification, or performance claim may exceed the
  evidence documented in the repository.

## AI-assisted development

- A human contributor remains accountable for scope, correctness, licensing,
  safety, and final review of AI-assisted work.
- Do not submit secrets, personal data, confidential material, or proprietary
  source to a model or tool.
- Treat generated content and retrieved sources as untrusted until verified.
- Check provenance and licensing; do not imitate or reconstruct proprietary
  motor-control firmware.
- Disclose material AI assistance in the pull request. Do not commit prompts or
  transcripts by default.
- An AI agent may not approve its own work, waive tests, weaken safety, or make
  governance and licensing decisions.

## Governance transition trigger

The current model is founder-led. If the Lead Maintainer says that many
contributors are entering, maintainer coordination is becoming centralized, or
a multi-maintainer model is needed, treat that statement as the explicit
trigger in `GOVERNANCE.md`. Surface and prepare a governance-transition
proposal and ADR. Do not silently postpone the review, and do not change
governance without explicit Lead Maintainer approval.

## Changes requiring explicit approval

- licensing, governance, Code of Conduct, or security-policy changes;
- public API or serialization compatibility commitments;
- new production dependencies;
- claims of hardware readiness, vehicle readiness, or safety compliance;
- deletion of material project history or validation evidence;
- release creation, remote push, or repository-setting changes.

## Code review rules

Flag a change when it:

- lets controller or identifier code access plant truth;
- introduces an undocumented sign, transform, unit, or normalization choice;
- silently replaces nonlinear/coupled physics with a constant or decoupled
  approximation;
- performs numerical differentiation in a fast runtime path without an
  accepted design decision;
- permits NaN, infinity, singular matrices, invalid sensors, or invalid models
  to reach modulation output;
- allows adaptive logic to override hard limits;
- reports generated or simulated evidence as measured hardware evidence;
- adds copied or generated content without adequate provenance.

Prefer explaining the physical or safety consequence and the expected safe
path instead of leaving style-only feedback that automated tools can enforce.
