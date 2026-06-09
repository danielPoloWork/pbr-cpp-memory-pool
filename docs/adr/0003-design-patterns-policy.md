# ADR-0003: Design Patterns Policy

- **Status:** Accepted
- **Date:** 2026-06-09
- **Deciders:** Daniel Polo (maintainer)
- **Related:** [`AGENTS.md`](../../AGENTS.md) §8, [`docs/patterns/README.md`](../patterns/README.md)

## Context

`pbr-cpp-memory-pool` is a **reference implementation**. Its value lies as much in being a teaching artifact as in being executable code. Among the competences a reader should be able to recognise in this repository is **deliberate, well-justified use of classical design patterns** — the GoF catalogue, RAII, Pimpl, Object Pool, and the broader enterprise toolbox.

Two failure modes are equally easy to fall into:

1. **Under-applying patterns.** Reaching only for ad-hoc structures and missing the opportunity to demonstrate how a recognised pattern would model the same problem more clearly.
2. **Over-applying patterns ("pattern soup").** Forcing patterns onto problems that do not need them, dressing simple code in vocabulary that adds ceremony without value. This *demonstrates the opposite* of mastery.

Both failures are mitigated only by an explicit policy: actively look for natural pattern fits, but require every adoption to pass an articulated justification, and record the choice durably.

## Decision

We adopt the following policy, normative across the repository:

1. **Patterns are exercised broadly where they are natural fits.** When a recognised pattern models the problem more clearly than an ad-hoc structure would, we use it. The didactic value is intentional.
2. **Every adoption is recorded and justified.** Each non-trivial pattern enters the codebase through:
   - an **ADR** (own ADR preferred; shared ADR allowed when multiple patterns are co-introduced and interdependent) capturing the problem, the alternatives considered, and the specific reasons this pattern was picked, and
   - an entry in [`docs/patterns/README.md`](../patterns/README.md) (the patterns catalogue) recording status, location in code, and the link back to the ADR.
3. **Rejected patterns are recorded too.** If a pattern was considered for a given problem and ruled out, the rejection — with reason — is captured in the catalogue under *Rejected*. This prevents the same debate from recurring in later PRs.
4. **No force-fitting.** Where the simpler answer is "no pattern needed", that decision is itself recorded (catalogue entry under *Considered → Not adopted*) and is treated as a valid outcome.
5. **PR template enforces visibility.** The pull-request template's *Design Patterns* section is mandatory for every PR; PRs that introduce no pattern write "None — straightforward implementation."

## Alternatives Considered

- **No formal policy — patterns at the author's discretion.** Rejected because it leaves the didactic dimension to chance and produces inconsistent coverage across PRs.
- **Mandate a fixed list of patterns to demonstrate.** Rejected as artificial. It would push contributors toward forced applications of inappropriate patterns purely to tick boxes, producing exactly the "pattern soup" failure mode we want to avoid.
- **Track patterns in code comments only.** Rejected because comments age silently with the code they annotate and are not discoverable as a whole. A central catalogue is needed for the artifact to be navigable as a reference.

## Consequences

**Positive**

- Patterns become a first-class, auditable dimension of the codebase rather than an implicit by-product.
- A reader can open the patterns catalogue and immediately survey what techniques the project demonstrates, with traceability into the code and the rationale.
- The "no pattern" decision is documented when it is the right call, so the policy does not collapse into over-application.

**Negative**

- Documentation tax per PR: each pattern-touching change requires both an ADR and a catalogue entry. Mitigated by the per-PR documentation rules already in force (everything ships in the same PR; no follow-ups).
- The patterns catalogue must be maintained by hand. Mitigated by listing it explicitly in the PR template's *Documentation Impact* checklist.

## References

- Gamma, Helm, Johnson, Vlissides — *Design Patterns: Elements of Reusable Object-Oriented Software* (1994).
- Sutter, Alexandrescu — *C++ Coding Standards*, items on Pimpl and RAII.
- [`AGENTS.md`](../../AGENTS.md) §8 — normative statement of the policy.
- [`docs/patterns/README.md`](../patterns/README.md) — living catalogue.
