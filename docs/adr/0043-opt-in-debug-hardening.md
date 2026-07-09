# ADR-0043: Opt-in debug hardening — poisoning, a guard word, and free-list safe-linking

- **Status:** Accepted
- **Date:** 2026-07-08
- **Deciders:** Daniel Polo (maintainer / project architect)
- **Related:** [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) (the intrusive free-list layout, the `block_size >= sizeof(void*)` + `alignof(std::max_align_t)` constraints, and the strict-aliasing-safe access idiom this ADR extends), [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) (the "defined, loud failure" stance and the accepted no-default-double-free-detection trade-off this hardens), [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) (the metadata-overhead budget kept intact when the gate is off), [ADR-0020](0020-thread-safety-strategy-and-compile-time-knob.md) (the three synchronization policies the hardening threads through), [ADR-0022](0022-dynamic-growth-policy-and-chunk-linking.md) (the growth chunks the stride change must respect), [ADR-0025](0025-decorator-for-instrumented-pool.md) (the `InstrumentedPool` decorator that composes over the hardened pool), [ADR-0037](0037-new-feature-roadmap-placement.md) (roadmap Milestone 9, which this item continues), [ADR-0004](0004-versioning-and-release-policy.md) (the SemVer **MINOR** this lands under), issue #109, origin issue #105, spec [§4.1](../specs/01_spec_cpp_memory_pool.md#41-constraints--guarantees) / [§5.3](../specs/01_spec_cpp_memory_pool.md#53-error-semantics)

## Context

The pool's free list is **intrusive** ([ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §1): a free block stores the address of the next free block *inside its own first `sizeof(void*)` bytes*. That is what makes the metadata overhead of a free block zero (spec §3.2) — and it is also a classic memory-corruption exploitation primitive. A use-after-free write into a freed block silently overwrites a live next-link; a linear buffer overflow past `block_size` corrupts the following slot; a leaked free-list pointer is a ready-made write target. The default build detects none of this: it deliberately trades detection for speed and a zero-overhead free block, and it does **not** detect a double-free of an in-range block ([ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md), spec §5.3).

The rest of the repository holds an enterprise/security posture — a [`SECURITY.md`](../../SECURITY.md), a sanitizer matrix ([ADR-0005](0005-toolchain-matrix-and-supported-platforms.md)), Valgrind-clean verification (spec §6.2). The specification review (#105) flagged the gap: "no mention of pointer obfuscation, freed-block poisoning, or debug canaries." AddressSanitizer covers much of this on the platforms where it exists — but it is unavailable on MSVC ([ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3), and it cannot instrument the pool's *own* sub-block boundaries: to ASan a pool is one large `operator new` region, so an overflow from one pool block into the next is invisible to its redzones.

We want a **self-contained, opt-in** hardening mode that works everywhere the library builds, catches the intrusive-free-list failure modes deterministically, demonstrates a real shipping allocator-hardening technique (glibc ≥ 2.32 safe-linking), and costs **nothing** — not a byte of footprint, not a cycle on the hot path — when it is off.

Four forces shape the design:

1. **Zero cost when off.** The default build and the [ADR-0014](0014-microbenchmark-methodology-pool-vs-malloc.md) benchmark numbers and the [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) overhead budget must be byte-for-byte and cycle-for-cycle unchanged. That rules out any runtime flag on the allocate/deallocate path; the knob must be **compile-time**.
2. **Don't disturb the frozen contract.** [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) fixes `block_size >= sizeof(void*)`, `block_size` a multiple of `alignof(std::max_align_t)`, and uniform `alignof(std::max_align_t)` alignment of every returned pointer. Hardening must preserve all three and must not shrink the user-visible `block_size`.
3. **Uniform application.** The transform must be applied at *every* free-list access site (init, push, pop, the debug walk) across all three thread-safety policies and across dynamic-growth chunks, or a single un-transformed access corrupts the list.
4. **Defined behaviour on detection.** A detected violation is a bug in the *consumer*; the response must be a defined, loud failure ([ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md)) — and it must be observable by a test without terminating the test process.

## Decision

Add an **opt-in, debug-only hardening mode** behind a single compile-time knob, `PBR_MEMORY_POOL_HARDENING` (a CMake `option`, **OFF** by default; a `harden` CMake preset turns it on). When it is off the entire mechanism is preprocessed out and this ADR's code is a no-op. The mode adds three protections, all layered onto the existing intrusive free list.

### 1. Physical slot stride, decoupled from `block_size`

Hardening reserves a trailing **guard word** (`std::uint64_t`) after each block's usable bytes. Rather than steal bytes from the user, the physical **slot stride** grows: `stride = roundup(block_size + sizeof(guard), alignof(std::max_align_t))`. Because `block_size` is already a multiple of `alignof(std::max_align_t)` (ADR-0009 §2), the stride becomes `block_size + alignof(std::max_align_t)` — the guard and its padding live in *added* storage, so:

- the user-visible `block_size` and the ADR-0009 §5 alignment guarantee are **unchanged** (every slot start remains `alignof(std::max_align_t)`-aligned);
- the cost is memory footprint **in the hardened build only** — one extra `alignof(std::max_align_t)` per slot.

A single `slot_stride(block_size)` helper is the identity (`== block_size`) when the knob is off, and *every* addressing site — `initialize_free_list`, `block_in_chunk` (range end + modulo), `grow_pool`, and `memory_pool_create` (including their `size_t`-overflow guards) — computes offsets from the stride, not from `block_size`.

### 2. Freed-block poisoning (use-after-free)

On free, after the next-link is threaded into the first `sizeof(void*)` bytes, the remainder of the block (`[sizeof(void*), block_size)`) is filled with a recognizable byte pattern (`0xDE`). On the next allocation of that slot, the pattern is verified intact before the block is handed out; a broken pattern is a **use-after-free** write and is reported. (The link bytes themselves are not poisoned — they are protected by safe-linking, below. When `block_size == sizeof(void*)` the block is entirely the link and has no payload to poison — see *Consequences*.)

### 3. Guard word (buffer overflow + double-free)

The trailing guard word is stamped `GUARD_ALLOCATED` when a block is vended and `GUARD_FREED` when it is returned. On free, the guard is validated first: still-`GUARD_FREED` ⇒ **double-free**; any value that is neither constant ⇒ a contiguous **buffer overflow** past `block_size` clobbered it. This is the "canary" the origin issue asked for — realized as one guard word living in the added stride (so, unlike a classic in-band canary, it costs no usable bytes and needs no separate sub-knob), and it makes double-free detection (the [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) gap) fall out for free.

### 4. Free-list next-pointer safe-linking

The in-band next-link is stored **XORed with a per-slot key** derived from the slot's own address (`ptr XOR (slot_addr >> 12)`) — glibc's `PROTECT_PTR`/`REVEAL_PTR` transform, which is symmetric (protect == reveal). Two consequences: a next-link leaked out of band is not a directly usable address, and a corrupted stored value reveals to a **misaligned** pointer, which an alignment check on reveal catches as **free-list corruption**. Access stays strict-aliasing-safe: the value is still read through `void* const*` and written through `void**` (ADR-0009 §3), only the bits stored differ.

### Detection policy — a swappable violation handler

On any detected violation the library calls an installed `HardeningViolationHandler` (declared in the new [`pool_hardening.hpp`](../../src/main/cpp/it/d4np/memorypool/pool_hardening.hpp), present only in hardened builds). The **default** handler prints a diagnostic to `std::cerr` and calls `std::abort()` — the [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) defined-loud-failure stance. `set_hardening_violation_handler` swaps it (thread-safe, via an atomic) and returns the previous one; passing `nullptr` restores the default. The handler is `noexcept` (it is called from the pool's `noexcept` (de)allocate path). A handler that **returns** — which the default never does — puts the pool on a best-effort path so a test can *assert* a violation without terminating the process; the tests install such a recording handler. Because the default aborts at the point of detection, the production path never continues past a violation.

### Compatibility

The knob is defined **PUBLIC** on the CMake target: a hardened build changes both the on-disk free-list encoding (safe-linking) *and* the physical slot stride, so the library and every consumer / test linking it must agree on the value. A hardened build is therefore deliberately **not** memory-layout-compatible with a non-hardened one — never mix the two configurations. The public C ABI and C++ types are otherwise untouched; the default build is byte-for-byte unchanged. This is purely additive → SemVer **MINOR** (a `v1.2.0` candidate, [ADR-0004](0004-versioning-and-release-policy.md); roadmap item 9.2 under [ADR-0037](0037-new-feature-roadmap-placement.md)).

## Alternatives Considered

- **A runtime flag instead of a compile-time knob.** Rejected. A per-operation branch on the hot path violates force 1 (the zero-cost-when-off guarantee and the ADR-0014/0015 numbers). A compile-time gate produces *identical* codegen when off.
- **Steal the guard/canary bytes from `block_size` (a distinct "canary" sub-mode with its own knob, as #109 sketched).** Rejected. Shrinking the usable block would change the ADR-0009 §2 block-size math and the user-visible contract, and it needed a second knob to manage the trade-off. Putting the guard in *added* stride keeps the user contract identical, costs no usable bytes, and folds overflow + double-free detection into one knob.
- **Head + tail canaries (two guard words).** Rejected as unnecessary for the fixed-block model: a *forward* linear overflow past `block_size` is the realistic case, one trailing guard catches it and doubles as the free/allocated state stamp, and a second word would only add footprint. (Underflow into the preceding slot is instead caught by *that* slot's poison/guard on its next cycle.)
- **Poison the whole block including the link bytes.** Rejected: the first `sizeof(void*)` bytes legitimately hold the (safe-linked) next-link while free, so they cannot also hold poison. Safe-linking guards those bytes instead; poison covers the rest.
- **Raw next-pointer with only poisoning (no safe-linking).** Rejected. Safe-linking is the didactic centrepiece (a real, shipping glibc technique; force in [ADR-0003](0003-design-patterns-policy.md)'s "exercise production-grade techniques and justify them" mandate), it makes a leaked link non-trivial to weaponize, and its alignment-on-reveal check gives cheap free-list-integrity detection that poisoning alone does not.
- **Lean on AddressSanitizer only.** Rejected as insufficient, not as competing: ASan is absent on MSVC (ADR-0005 §3) and cannot see *inside* the pool's own region (block-to-block overflow). This mode complements ASan and runs everywhere the library builds; the two are used together in CI.
- **A fixed abort with no handler hook.** Rejected. A swappable handler is what lets the test suite *prove* each violation is detected without killing the process, at zero cost to the production default (which still aborts).

## Consequences

**Positive**

- Deterministic detection of the three intrusive-free-list failure modes — use-after-free, forward buffer overflow past `block_size`, and double-free — everywhere the library builds, including MSVC where ASan is unavailable.
- The user-visible `block_size` and the ADR-0009 alignment guarantee are unchanged; the default build and its benchmark/overhead numbers are byte-for-byte and cycle-for-cycle unaffected (verified by the existing zero-overhead check and the CI `debug`/`release` cells).
- Demonstrates safe-linking end to end — the threat model, the `PROTECT_PTR`/`REVEAL_PTR` transform, and the alignment-on-reveal integrity check — in a shipping-quality reference, satisfying the #105 hardening ask and the ADR-0003 mandate.
- Works with fixed **and** dynamic pools and all three thread-safety policies; composes under the `InstrumentedPool` decorator (ADR-0025).

**Negative**

- A hardened build is a *separate configuration*: incompatible on-disk layout, and one extra `alignof(std::max_align_t)` of footprint per slot. This is stated explicitly and is the point (never mix configurations).
- The hardening runs validation work on every allocate/deallocate — it is a debug/bug-finding mode, not a production speed path. The compile-time gate is what keeps that cost strictly opt-in.
- Poisoning covers `[sizeof(void*), block_size)`; a use-after-free write that lands *only* in the first `sizeof(void*)` link bytes is caught by safe-linking's reveal check on the next pop rather than by poison. Where `block_size == sizeof(void*)` — reachable only where `alignof(std::max_align_t)` equals the pointer size (e.g. MSVC x64, both 8) — the block is *entirely* the link and has no payload to poison, so use-after-free of such a slot is covered by safe-linking (and the guard word / double-free check) rather than by poison. Larger blocks carry poisoned payload bytes. This is an inherent property of the layout, documented rather than papered over: rejecting such block sizes was considered and rejected because it would break `TypedPool<T>` / `PoolAllocator<T>` for pointer-sized `T` under hardening, and the [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) block-size contract stays identical in both configurations.

**Testing / tooling / documentation (landing in the same PR)**

- [`pool_hardening.hpp`](../../src/main/cpp/it/d4np/memorypool/pool_hardening.hpp) — the violation-handler surface (get/set + the `HARDENING_*` kind constants), present only in hardened builds.
- [`pool_hardening_test.cpp`](../../src/test/cpp/it/d4np/memorypool/pool_hardening_test.cpp) — a dedicated doctest binary (CTest `pool_hardening`), gated like `free_list_iterator_test` so it builds (with one placeholder case) in the default build, and under the `harden` preset asserts no-false-positive cycles plus deterministic detection of use-after-free, overflow, and double-free via a recording handler.
- [`CMakeLists.txt`](../../CMakeLists.txt) / [`CMakePresets.json`](../../CMakePresets.json) — the `PBR_MEMORY_POOL_HARDENING` option (PUBLIC) and a `harden` configure/build/test preset.
- [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) — a `harden` matrix cell on each Tier-1 platform so the hardened configuration is built and its tests run under the quality bar (the "clean in *both* configurations" acceptance criterion).
- [`docs/specs/01_spec_cpp_memory_pool.md`](../specs/01_spec_cpp_memory_pool.md) — §4.1 gains a hardening note, §5.3 records that opt-in detection now exists, and §7 maps it to this ADR / §7.1 no longer defers it. (The spec is a translated source but is already `stale` from the prior spec PRs, so this edit adds no new i18n debt.)
- [`docs/doxygen/Doxyfile`](../doxygen/Doxyfile) `PREDEFINED` gains `PBR_MEMORY_POOL_HARDENING=1` so the gated handler surface is documented on the API site.
- [`SECURITY.md`](../../SECURITY.md) — the scope note records the opt-in hardening build as a defense-in-depth / bug-finding aid.
- [`ROADMAP.md`](../../ROADMAP.md) — item 9.2 flips to done.
- [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased` — an *Added* entry.
- **Deferred to the `v1.2.0` release PR** to keep this feature PR off the *translated* docs surface (editing them trips the `i18n-freshness` gate, and their rows are currently `translated`): the [`README.md`](../../README.md) security-section refresh and Milestone-9 table row, plus any [`docs/patterns/README.md`](../patterns/README.md) touch. The release PR refreshes them and re-syncs the `zh-Hans` / `ja` translations in one pass — the same M8.8 / ADR-0042 sequencing.

## References

- glibc `malloc` safe-linking (`PROTECT_PTR` / `REVEAL_PTR`), introduced in glibc 2.32 — the XOR-with-`(addr >> 12)` transform this mirrors, and its alignment-on-reveal integrity check.
- [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) — the intrusive free-list layout, the block-size/alignment constraints, and the strict-aliasing-safe access idiom.
- [ADR-0012](0012-foreign-pointer-and-out-of-range-pointer-policy.md) — the defined-loud-failure stance and the default no-double-free-detection trade-off this hardens.
- [ADR-0015](0015-metadata-overhead-budget-and-introspection.md) — the metadata-overhead budget kept intact when the gate is off.
- [`SECURITY.md`](../../SECURITY.md) — the project's security policy and scope.
