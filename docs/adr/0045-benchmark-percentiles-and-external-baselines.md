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

### 2. Optional external baselines — feature-detected, never a hard dependency

jemalloc and tcmalloc are added as **optional** baselines. CMake feature-detects each (`find_library` + `find_path` for the header); only when both are found does it define `PBR_BENCH_HAVE_JEMALLOC` / `PBR_BENCH_HAVE_TCMALLOC` and link it. When neither is present — the default, and every MSVC build — the guarded code is compiled out and the benchmark's output is byte-for-byte what ADR-0014 produced (plus one `# baselines: malloc` disclosure line). jemalloc is driven through its prefix-independent extended API (`mallocx`/`dallocx`), tcmalloc through gperftools' explicit `tc_malloc`/`tc_free`, so neither has to override the system `malloc` and all three appear as **distinct rows**.

A small `RawAllocator` (name + alloc/free function pointers) unifies the system `malloc`, jemalloc, and tcmalloc behind one interface for the *added* baseline rows and the percentile recorder. The dedicated `malloc` runners that produce the committed ADR-0014 aggregate numbers are left untouched.

### 3. Output contract

Additive per ADR-0014 §6: the aggregate 8-column table is unchanged; baseline rows reuse that schema (extra rows, tagged with the allocator name); percentile data is a new, separate table with its own header; a `# baselines:` header line discloses which allocators are compiled in. No existing row or column changes, so the M7.x report-diffing tooling still parses old and new reports.

### 4. CI

A `bench-baselines` job (Linux) installs `libjemalloc-dev` + `libgoogle-perftools-dev`, asserts both baselines were feature-detected, builds, and runs `--scenario all --percentiles`, asserting the baseline rows and the percentile table are present. Like every bench cell it gates on **exit code 0, not numbers** (ADR-0014 §8 — shared runners are too noisy for numeric thresholds). It is Linux-only, never touching the MSVC leg (ADR-0005 §3), consistent with the sanitizer-preset split.

## Alternatives Considered

- **Always-on percentiles (fold p99 into the existing per-repeat `Stats`).** Rejected. p99 over ~9 per-repeat aggregates is meaningless (it is essentially the max), and per-op timing on the default path would perturb the committed ADR-0014 ns/op numbers. Per-op sampling behind an opt-in flag is the only way to get a meaningful p99 without disturbing the frozen numbers.
- **HdrHistogram (or another bucketed recorder).** Rejected for now: it is an external dependency, and for this benchmark's needs a `std::vector<double>` of per-op samples with a nearest-rank query is sufficient and keeps the methodology inspectable (the ADR-0014 §1 pedagogy argument). Revisit if memory for the sample vector becomes a constraint at very large iteration counts.
- **`LD_PRELOAD` / link the whole binary against jemalloc.** Rejected: that *replaces* the `malloc` row with jemalloc rather than adding a distinct baseline, so pool / malloc / jemalloc cannot be compared side by side in one run. Calling the allocators' explicit APIs keeps them as separate rows.
- **A hard dependency on jemalloc/tcmalloc.** Rejected — it would break spec §3.3 and the MSVC leg. Feature-detection with a silent skip keeps the default build dependency-free.
- **CI numeric thresholds on the baselines/percentiles.** Rejected for the ADR-0014 §8 runner-noise reasons; the new cell is a build/run gate, not a performance gate.

## Consequences

**Positive**

- p99/p999 tail latency is reportable, and the dynamic-pool growth row makes the amortized-growth spike visible where the median hid it — the #105 §6.3 critique is closed.
- Modern baselines (jemalloc/tcmalloc) are available for comparison wherever they are installed, feature-detected and validated in CI.
- The default build and the committed ADR-0014 numbers are unchanged; zero external dependencies preserved.

**Negative**

- Per-op percentiles are resolution-bound: on Windows's ~100 ns `steady_clock` tick they quantize, so they are meaningful there only for microsecond-scale tail events, not for sub-tick medians (documented; the aggregate table stays authoritative).
- The jemalloc/tcmalloc code paths build and run only where those libraries exist, so they are exercised on the Linux CI cell, not on the maintainer's MSVC box.

**Tooling / documentation (same PR)**

- [`pool_vs_malloc_bench.cpp`](../../src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench.cpp) — the `--percentiles` mode, the `RawAllocator` abstraction, and the guarded baselines.
- [bench `CMakeLists.txt`](../../src/bench/cpp/it/d4np/memorypool/CMakeLists.txt) — the jemalloc/tcmalloc feature-detect.
- [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) — the `bench-baselines` job.
- [bench `README.md`](../../src/bench/cpp/it/d4np/memorypool/README.md) — the new columns/baselines and the percentile caveat.
- [`ROADMAP.md`](../../ROADMAP.md) item 9.4, spec §7.1, [`CHANGELOG.md`](../../CHANGELOG.md) `Unreleased`.

## References

- [ADR-0014](0014-microbenchmark-methodology-pool-vs-malloc.md) — the methodology this extends.
- jemalloc `mallocx`/`dallocx` extended API; gperftools tcmalloc `tc_malloc`/`tc_free`.
- Gil Tene, *How NOT to Measure Latency* — the case for percentiles/tail over averages in latency reporting.
