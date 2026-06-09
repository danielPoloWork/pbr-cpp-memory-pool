# ADR-NNNN: <Short, descriptive title in imperative or noun form>

- **Status:** Proposed | Accepted | Superseded by ADR-XXXX | Deprecated
- **Date:** YYYY-MM-DD
- **Deciders:** <names or roles>
- **Related:** <ADR-XXXX, spec section, issue, PR>

## Context

What is the situation that motivates this decision? Describe the forces at play: functional requirements, non-functional constraints (performance, ABI, portability, thread safety), prior art in the codebase, and any external pressures. Be specific enough that a future reader who never saw the original discussion can reconstruct the problem.

## Decision

State the decision in a single declarative paragraph. Use active voice: *"We use an implicit free list stored inside free blocks…"*. The decision is the contract — everything else in this document is supporting evidence.

## Alternatives Considered

For each rejected option, give a one-paragraph summary and the specific reason it lost. Showing the runners-up makes the decision auditable.

- **Alternative A** — <description>. Rejected because <reason>.
- **Alternative B** — <description>. Rejected because <reason>.

## Consequences

Describe the outcomes — both the wins this decision unlocks and the costs it imposes. Include:

- API or ABI implications
- Performance trade-offs (with rough magnitudes if known)
- Testing and tooling impact
- Documentation that must be added or updated
- Risks and known limitations

## References

- Spec sections, papers, blog posts, related ADRs, benchmark results, prior incidents.
