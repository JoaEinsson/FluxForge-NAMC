# AI-Assisted Development Policy

## Purpose

FluxForge-NAMC permits responsible use of large language models and other
generative tools. These tools can accelerate implementation, documentation,
review, and analysis, but they do not hold project authority and do not reduce
the evidence required for safety-relevant or mathematically sensitive work.

This policy is vendor-neutral. Repository-specific execution instructions are
also provided in the root `AGENTS.md` so compatible development agents begin
with consistent project context.

## Human accountability

Every contribution requires an accountable human submitter. That person is
responsible for:

- understanding the contribution well enough to defend its design;
- verifying technical, physical, numerical, and security correctness;
- confirming provenance and license compatibility;
- running and interpreting applicable tests;
- accurately describing limitations and evidence level;
- satisfying the DCO and responding to review.

An AI system cannot be an author, DCO signatory, maintainer, approver, or final
decision-maker.

## Permitted uses

Examples of acceptable assistance include:

- exploring design alternatives and drafting ADRs;
- generating an initial implementation that receives human review;
- producing test ideas, analytical checks, and failure scenarios;
- improving documentation, examples, and issue summaries;
- reviewing diffs for numerical, portability, safety, or consistency risks;
- translating a human-approved requirement into English project artifacts;
- analyzing public, appropriately licensed technical sources with attribution.

## Prohibited or restricted uses

Contributors and agents must not:

- provide secrets, credentials, personal data, confidential information, or
  proprietary source code to an AI service;
- ask a model to reproduce, imitate, decompile, or reconstruct proprietary
  controller firmware or restricted vendor material;
- merge generated code that the accountable contributor cannot explain;
- use generated output as proof of mathematical correctness or physical
  validity;
- fabricate citations, benchmark results, experiments, reviews, or test runs;
- let an AI system approve its own contribution or satisfy required independent
  review;
- use AI to bypass licensing, DCO, security, Code of Conduct, or safety rules;
- allow an agent to push, publish, release, or change external systems without
  explicit authorization.

## Required review

AI-assisted output is untrusted input. Review must be proportional to risk:

- Documentation must be checked against implemented behavior and primary
  sources.
- C and Python changes must pass formatting, warnings, unit, integration, and
  numerical checks applicable to the touched area.
- Mathematical derivatives must be checked against analytical fixtures or
  independent numerical test oracles outside runtime code.
- Safety and limit changes require explicit failure-path and fallback review.
- Imported constants, equations, datasets, and algorithms require provenance.
- Performance claims require reproducible measured output.

For high-consequence changes, use a reviewer or validation method independent
of the model interaction that produced the change.

## Disclosure

Material AI assistance must be disclosed in the pull-request template. The
disclosure should identify:

- the tool or model family when known;
- the parts of the change materially assisted;
- the human verification performed;
- known limitations or uncertain provenance that reviewers should inspect.

Incidental spelling completion or ordinary editor autocomplete does not need a
detailed disclosure.

Do not commit full prompts, private conversations, hidden reasoning, or raw
transcripts by default. Record design rationale in issues, ADRs, tests, and
documentation instead. A minimal prompt excerpt may be included only when it is
itself a necessary, non-sensitive project artifact.

## Source and provenance discipline

- Prefer primary sources: standards, official documentation, original papers,
  and openly licensed upstream repositories.
- Verify claims against the actual source rather than a search snippet or model
  summary.
- Record licenses for copied or adapted material.
- Do not assume generated text or code is novel merely because no source was
  cited.
- Reimplement concepts from equations and public knowledge when license or
  provenance is uncertain.

## Agent operating boundaries

Development agents should inspect the repository before editing, preserve
unrelated work, make scoped changes, run relevant validation, and hand off a
reviewable diff. They must distinguish actions performed from actions merely
recommended.

Agents may make ordinary reversible implementation decisions within an
accepted task. They require explicit human approval for governance, licensing,
security-policy, release, publication, external communication, new production
dependencies, and material safety-boundary changes.

## Governance growth signal

The project begins with founder-led governance. If the Lead Maintainer states
that contributor volume has materially increased or asks for a multi-maintainer
structure, AI development agents must identify that statement as the formal
transition trigger described in `GOVERNANCE.md`. The next governance work must
prepare a reviewable TSC proposal and ADR rather than silently continuing the
founder-led structure.

## Policy evolution

This policy should be reviewed when:

- a new category of AI tool gains write, release, or infrastructure access;
- AI-generated datasets or learned models enter the repository;
- the contributor or maintainer structure changes materially;
- regulation, licensing practice, or project risk changes;
- repeated review failures reveal a missing control.

Policy changes require explicit Lead Maintainer approval and a documented
rationale.
