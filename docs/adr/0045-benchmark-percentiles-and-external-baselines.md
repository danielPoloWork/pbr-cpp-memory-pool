# ADR-0045: Benchmark tail-latency percentiles and optional external allocator baselines

- **Status:** Accepted
- **Date:** 2026-07-09
- **Deciders:** Daniel Polo (maintainer / project architect)
- **Extends:** [ADR-0014](0014-microbenchmark-methodology-pool-vs-malloc.md) (the microbenchmark methodology — this ADR adds to it without superseding; the §1–8 contract there stays in force)
- **Related:** spec [§6.3](../specs/01_spec_cpp_memory_pool.md#63-performance-benchmark), [ADR-0004](0004-versioning-and-release-policy.md) (SemVer-neutral tooling change), [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3 (the platform split the baseline cell inherits), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2 (block-size constraints), [ADR-0024](0024-dynamic-growth-synchronization-and-creation-surface.md) §2 (dynamic-growth support, whose events the p99 row surfaces), issue #111, origin issue #105

## Context

[ADR-0014](0014-microbenchmark-methodology-pool-vs-malloc.md) delivered a rigorous hand-rolled microbenchmark — warm-up, min/median/mean/max/stddev over repeats, anti-optimization barriers, disclosed host, per-release reports, a non-asserting CI smoke, and (later) concurrent and growth scenarios. The spec review (#105 §6.3) is otherwise satisfied; **two residual critiques** stand:

1. **No tail latency.** `median` is effectively p50; there is no p99/p999. For an allocator, the tail — e.g. the microsecond-scale spike when a dynamic pool grows — is exactly what a latency-sensitive consumer cares about, and the aggregate median averages it away.
2. **Only the system `malloc` baseline.** A reviewer compares an allocator against jemalloc / tcmalloc; without them the headline ratios are only vs glibc/MSVC `malloc`.

This ADR extends ADR-0014 to close both, under two hard constraints: the existing aggregate ns/op numbers (the ADR-0014 committed contract) must **not** be perturbed, and the default build must keep spec §3.3's **zero external dependencies**.

## Decision

### 1. Tail-latency percentiles — opt-in `--percentiles`, per-operation timing, separate table

Percentiles are only meaningful over a large sample set, so `--percentiles` switches on a **per-operation** timing path (one latency sample per op) — distinct from the aggregate path (one sample per repeat), which is left exactly as ADR-0014 froze it. It is **opt-in** so per-op timing overhead never perturbs the committed aggregate numbers. It emits a **separate TSV table** (`scenario, allocator, p50_ns/op, p90_ns/op, p99_ns/op, p999_ns/op, samples`) after the headline block, so the ADR-0014 aggregate table's 8-column schema is untouched.

Percentiles use the **nearest-rank** method over the sorted per-op samples. The table covers the interleaved scenario (pool / `malloc` / each compiled baseline) and a **dynamic-pool growth** row whose p99/p999 expose the growth events — the headline motivation.

**Documented caveat (honest methodology).** Per-op timing carries a fixed clock-read overhead common to every allocator, and its resolution is the platform `steady_clock` tick: on Linux/macOS (≈1 ns) the percentiles are fine-grained; on Windows (≈100 ns) they **quantize to the tick**, so p50–p99 of a single ~5–50 ns op collapse onto multiples of 100 ns. The percentile columns are therefore for **tail / relative comparison and for surfacing microsecond-scale events** (a growth spike shows as p999 ≫ p50 on every platform); for absolute per-op cost the aggregate ns/op table remains authoritative. This is stated in the bench README and the report template.

### 2. Optional external baselines — measured under `LD_PRELOAD`, never a hard dependency

jemalloc and tcmalloc are measured by **re-running the same bench binary under `LD_PRELOAD`**, which swaps the whole process allocator. This is deliberate and load-bearing. Those libraries are *designed to be the process allocator*: they take over global `malloc`/`operator new` — via strong symbols and, for tcmalloc, a library constructor — on load. So they cannot be linked, nor even `dlopen`'d, alongside the system allocator to produce side-by-side in-process rows: linking one silently turns the "malloc" row into that allocator, linking both crashes (their initializers fight over `malloc`), and `dlopen` does not help because the constructor still runs. Both were tried and both crashed (SIGSEGV).

`LD_PRELOAD` sidesteps all of that: one process, one allocator, no mixing. Under `LD_PRELOAD=libjemalloc.so.2` the bench's `malloc` rows — and the pool's own backing store — are served by jemalloc, so a preloaded run is a clean, complete `pool`-vs-*that-allocator* comparison across every scenario (including the percentile table). The bench therefore carries **no allocator-specific code at all**: the default build stays byte-for-byte what ADR-0014 produced and spec §3.3's zero-external-dependency posture holds trivially. A `# allocator:` header line (read from `LD_PRELOAD`, POSIX only; "system malloc" otherwise) discloses which allocator each run's numbers reflect, so the three reports are unambiguous.

### 3. Output contract

Additive per ADR-0014 §6: the aggregate 8-column table is unchanged; percentile data is a new, separate table with its own header; a `# allocator:` header line discloses which allocator this run measured (the `LD_PRELOAD` basename, or "system malloc"). No existing row or column changes, so the M7.x report-diffing tooling still parses old and new reports; a baseline comparison is a set of reports, one per allocator, distinguished by that header line.

### 4. CI

A `bench-baselines` job (Linux) installs the jemalloc + tcmalloc **runtime** shared objects (`libjemalloc2`, `libtcmalloc-minimal4t64`), builds once, and runs `--scenario all --percentiles` three times — plain, `LD_PRELOAD=libjemalloc.so.2`, and `LD_PRELOAD=libtcmalloc_minimal.so.4` — asserting each disclosed the expected allocator, emitted the percentile table, and exited cleanly. Like every bench cell it gates on **exit code 0, not numbers** (ADR-0014 §8 — shared runners are too noisy for numeric thresholds). It is Linux-only, never touching the MSVC leg (ADR-0005 §3), consistent with the sanitizer-preset split.

## Alternatives Considered

- **Always-on percentiles (fold p99 into the existing per-repeat `Stats`).** Rejected. p99 over ~9 per-repeat aggregates is meaningless (it is essentially the max), and per-op timing on the default path would perturb the committed ADR-0014 ns/op numbers. Per-op sampling behind an opt-in flag is the only way to get a meaningful p99 without disturbing the frozen numbers.
- **HdrHistogram (or another bucketed recorder).** Rejected for now: it is an external dependency, and for this benchmark's needs a `std::vector<double>` of per-op samples with a nearest-rank query is sufficient and keeps the methodology inspectable (the ADR-0014 §1 pedagogy argument). Revisit if memory for the sample vector becomes a constraint at very large iteration counts.
- **In-process side-by-side rows — link the allocators (`-ljemalloc`/`-ltcmalloc`) or `dlopen` them.** Rejected after *both* were implemented and *both crashed* (SIGSEGV). The libraries take over global `malloc` on load (strong symbols; tcmalloc also via a constructor), so co-linking makes their initializers fight over `malloc`, linking one silently rebinds the "malloc" row to that allocator, and `dlopen(RTLD_LOCAL)` still runs the constructor. There is no safe way to have two of these allocators plus the system allocator live in one process. `LD_PRELOAD` — one allocator per process, chosen from outside — is the standard, crash-free way to benchmark them, at the cost that a full comparison is N reports rather than N rows in one.
- **A hard dependency on jemalloc/tcmalloc.** Rejected — it would break spec §3.3 and the MSVC leg. Feature-detection with a silent skip keeps the default build dependency-free.
- **CI numeric thresholds on the baselines/percentiles.** Rejected for the ADR-0014 §8 runner-noise reasons; the new cell is a build/run gate, not a performance gate.

## Consequences

**Positive**

- p99/p999 tail latency is reportable, and the dynamic-pool growth row makes the amortized-growth spike visible where the median hid it — the #105 §6.3 critique is closed.
- Modern baselines (jemalloc/tcmalloc) are available for comparison wherever they are installed, via `LD_PRELOAD`, and validated in CI.
- The default build and the committed ADR-0014 numbers are unchanged; zero external dependencies preserved.

**Negative**

- Per-op percentiles are resolution-bound: on Windows's ~100 ns `steady_clock` tick they quantize, so they are meaningful there only for microsecond-scale tail events, not for sub-tick medians (documented; the aggregate table stays authoritative).
- A baseline comparison is **N reports, not N rows in one** — the user (or CI) re-runs the bench under each `LD_PRELOAD`. This is exercised on the Linux CI cell; the maintainer's MSVC box has no `LD_PRELOAD`, so it reports "system malloc" only.

**Tooling / documentation (same PR)**

- [`pool_vs_malloc_bench.cpp`](../../src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench.cpp) — the `--percentiles` mode and the `# allocator:` header disclosure.
- [bench `CMakeLists.txt`](../../src/bench/cpp/it/d4np/memorypool/CMakeLists.txt) — no allocator link; the `# allocator:` header disclosure is the only bench code touched for baselines.
- [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) — the `bench-baselines` job.
- [bench `README.md`](../../src/bench/cpp/it/d4np/memorypool/README.md) — the new columns/baselines and the percentile caveat.
- [`ROADMAP.md`](../../ROADMAP.md) item 9.4, spec §7.1, [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased`.

## References

- [ADR-0014](0014-microbenchmark-methodology-pool-vs-malloc.md) — the methodology this extends.
- jemalloc and gperftools tcmalloc — `LD_PRELOAD` drop-in allocator replacements; both take over global `malloc` on load, which is why they are measured out-of-process here.
- Gil Tene, *How NOT to Measure Latency* — the case for percentiles/tail over averages in latency reporting.
