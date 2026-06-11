# ADR-0014: Microbenchmark methodology — pool vs. `malloc`

- **Status:** Accepted
- **Date:** 2026-06-11
- **Deciders:** Daniel Polo (maintainer), Claude (architect agent)
- **Related:** spec §6.3, ROADMAP §2.9, [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3 (compiler matrix), [ADR-0006](0006-code-style-and-static-analysis-baseline.md) (style + lint baseline), [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2 (block-size constraints the benchmark must respect), [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) (narrative goes in Markdown — bench reports too)

## Context

Spec §6.3 requires a benchmark comparing `memory_pool_alloc` / `memory_pool_free` against `malloc` / `free` over **a cycle of 1,000,000 iterations**, with numbers committed and a summary in the README. ROADMAP §2.9 locates the source under `src/bench/cpp/it/d4np/memorypool/` and §4.5 carries the contract forward into the concurrent variant once threading lands.

The seemingly innocuous "compare two allocators over 1M iterations" requirement hides four design decisions that materially affect whether the resulting numbers are meaningful:

1. **Framework.** Hand-rolled `std::chrono` versus a third-party microbenchmark library (Google Benchmark, Nanobench, doctest's `BENCHMARK`). The library route gives statistical machinery and process-level isolation for free; the hand-rolled route preserves the [spec §3.3](../specs/01_spec_cpp_memory_pool.md) zero-external-dependency posture even in the bench tree (where deps are technically allowed) and keeps the methodology legible end-to-end — important for a reference implementation.
2. **Scenarios.** Bulk-then-bulk (allocate 1M, then free 1M — measures throughput on cold-then-hot regions) versus interleaved (allocate-one / free-one × 1M — measures recycling efficiency). The two regimes stress different parts of an allocator; reporting only one paints an incomplete picture.
3. **Statistical rigor.** A single timed run of 1M iterations exhibits ~5–15 % noise on a workstation. A median over 10 repeats with min/max/stddev disclosure gives a number that survives a reviewer challenge; a single run does not.
4. **Anti-optimization.** A modern compiler in Release mode can prove that `void* p = malloc(64); free(p);` has no observable side effect and elide the entire loop. The benchmark must produce a write through every returned pointer to defeat that, and additionally use a portable `do_not_optimize` barrier on the pointer itself to defeat dead-store elimination on the byte write.

Skipping any one of these would produce numbers — and a senior reviewer would be right to throw them out.

A secondary forcing function: this is a **reference implementation** in the PBR series. The benchmark code is itself a teaching artifact. A reader who follows the bench source must learn *how to do this correctly*, not just *what numbers came out*. That argues against handing off the machinery to an external library that hides the methodology, and in favour of a hand-rolled implementation whose every line is part of the demonstrative contract.

## Decision

We adopt a **hand-rolled `std::chrono::steady_clock` microbenchmark** under `src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench.cpp` with the following methodology, frozen by this ADR:

### 1. Scenarios

Two scenarios run for every reported configuration:

- **Bulk** — allocate `iterations` blocks back-to-back into an array; then free all `iterations` blocks back-to-back. Two timing regions per run (`bulk-alloc`, `bulk-free`). Measures peak throughput on cold backing storage and on a fully-populated allocator. Pool capacity for this scenario equals `iterations`.

- **Interleaved** — loop `iterations` times performing one `alloc` immediately followed by one `free` of the same pointer. Single timing region per run (`interleaved`). Measures the steady-state cost of one alloc/free pair on an allocator that is recycling its working set. Pool capacity for this scenario is a small constant (`1` block per the implementation's contract, but configurable to a larger value to allow the compiler to vary slot residency).

Both scenarios run against both allocators (pool and `malloc`). The report therefore carries six measurements: `pool-bulk-alloc`, `pool-bulk-free`, `pool-interleaved`, `malloc-bulk-alloc`, `malloc-bulk-free`, `malloc-interleaved`.

### 2. Iteration count and repeats

- `iterations` defaults to **1,000,000** per spec §6.3, configurable via `--iterations N` from the command line for fast smoke runs.
- Each scenario is repeated **10 times** (configurable via `--repeats N`). The first repeat is treated as a warm-up and **discarded**; the remaining nine feed the statistical summary. Warm-up amortises the page-fault cost of the first touch on the backing storage and the I-cache cost of the first execution of the timed loop.

### 3. Block size

- `block_size` defaults to **64 bytes** (configurable via `--block-size N`). Sixty-four bytes is the most common cache line size on every Tier-1 platform (ADR-0005 §2) and is the natural representative of "a small struct or list node". The value satisfies ADR-0009 §2 on every supported host (`64 ≥ sizeof(void*)` and `64 % alignof(std::max_align_t) == 0`).

### 4. Statistical reporting

For each of the six measurements, after discarding the warm-up repeat, we report:

- **min** — best-case observed cost per iteration (ns/op).
- **median** — central tendency, robust against single-repeat outliers from OS scheduling.
- **mean** — arithmetic average, included for comparability with prior art.
- **max** — worst-case observed cost.
- **stddev** — standard deviation across the nine measured repeats.

All numbers are emitted as **nanoseconds per single allocation or deallocation** (`ns/op`), computed as `(elapsed_ns / iterations)` for each repeat. The headline comparison in the report and README is the **median ratio** `malloc_median / pool_median`.

### 5. Anti-optimization barriers

Every returned pointer is touched via `*static_cast<volatile unsigned char*>(p) = static_cast<unsigned char>(i & 0xFFu)` where `i` is the loop counter — a one-byte write through a `volatile` lvalue. This:

- forces the compiler to materialise the pointer and execute the allocation (the write must happen, so the pointer must be valid),
- faults the page in on first touch (so the bulk-alloc timing reflects the real cost of getting usable memory),
- carries enough variation in the written value (`i & 0xFFu`) to defeat any pattern-detection in the optimiser.

In addition, every pointer is passed through a portable `do_not_optimize(p)` helper before being freed — an inline function with `asm volatile("" : : "g"(p) : "memory")` on GCC/Clang and a `volatile` write through a sink on MSVC. This barrier guarantees the optimiser cannot reorder or elide operations across the timing-region boundary.

### 6. Output format

The binary prints to stdout. The output has three sections, all on `stdout` so the file can be redirected to disk verbatim:

1. **Header block** — host disclosure (one line each: hostname / OS / CPU / cores / RAM / compiler / compiler flags), then the run configuration (iterations / repeats / block size).
2. **Results table** — one row per measurement (six rows), tab-separated columns: scenario, allocator, region, min_ns/op, median_ns/op, mean_ns/op, max_ns/op, stddev_ns/op.
3. **Headline summary** — three lines stating the median ratio (`malloc_median / pool_median`) for each of bulk-alloc, bulk-free, interleaved.

The format is intentionally machine-parseable (TSV body, named sections) so future tooling (M7.x report-diffing, regression CI gates) can consume it without re-parsing.

### 7. Reporting cadence and storage

Numbers are committed to the repo under `docs/bench/`, one file per release × host combination, named `v<X.Y.Z>-<host-tag>.md`. The host-tag is `<os>-<compiler>-<arch>` (e.g. `windows-msvc-x64`, `linux-gcc-x64`, `macos-apple-clang-arm64`). The file body wraps the raw benchmark output in a Markdown report that:

- documents the host in human prose (CPU model + clock, OS version, compiler version, build flags, `cmake --preset` used),
- pastes the raw benchmark output verbatim inside a fenced block,
- adds an *Observations* section calling out any anomalies (e.g. macOS arm64 first-touch latency, glibc's `malloc` arena threshold).

The README's *Performance* section displays the canonical headline ratio from the most recently-committed report and links to the full file. When v0.2.0 ships, the bench report is `docs/bench/v0.2.0-<host>.md`. Subsequent releases add new files; old files stay for historical comparison.

### 8. CI integration

A `bench-smoke` job in [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) builds the bench binary with the `bench` preset (Release + `PBR_MEMORY_POOL_BUILD_BENCHMARKS=ON`) and runs it with `--iterations 10000 --repeats 3`. The job asserts the binary exits 0; it deliberately does **not** assert numeric thresholds.

GitHub Actions runners are shared, noisy, and vary across runs by ±30 % on memory-bound microbenchmarks. Committing a numeric assertion (e.g. "pool must be at least 2× faster") would result in regular spurious red, eroding the meaning of a green CI badge. The committed numbers in `docs/bench/v<X.Y.Z>-<host>.md` come from a controlled local host whose configuration is disclosed in the file header.

The bench-smoke job exists for one reason: to catch a regression where the bench binary no longer compiles, links, or runs to completion. The numbers themselves remain a human-verified deliverable.

## Alternatives Considered

- **Google Benchmark.** The industry-standard C++ microbenchmark library. Rejected because (a) it adds an external dependency to the bench tree that obscures the methodology from a reader (we want the timing loop to be inspectable end-to-end, not handed off to a `BENCHMARK_REGISTER_F` macro), (b) the spec §6.3 "1M iterations" requirement is a literal contract the hand-rolled loop reproduces 1:1, whereas Google Benchmark's adaptive iteration count would either drift from the spec or require disabling the adaptive logic, and (c) Google Benchmark is significantly heavier than the deliverable warrants (a 4 MB dependency for a single comparison binary). The library is excellent and would be the right call for a benchmark *suite* of dozens of measurements; for two scenarios and one comparison, it is overkill.

- **Nanobench.** Single-header, modern, MIT-licensed. Rejected on grounds (a) and (b) above — the dependency still hides the timing loop from the reader, even though it is single-header. Carries the same loss-of-pedagogy cost as Google Benchmark without the corresponding gain in scenario count.

- **doctest's `BENCHMARK` macro.** Already in the test tree via FetchContent. Rejected because the macro is a test-framework feature, not a benchmark framework — it lacks repeat handling, statistical summary, and the anti-optimization barriers. Using it would produce numbers but not honest ones, and would conflate the *correctness* and *performance* contracts (the test binary should exit on the first correctness failure; the bench binary should run to completion regardless of variance).

- **Single scenario** (bulk-only or interleaved-only). Rejected because the two scenarios stress different parts of an allocator: bulk measures throughput on cold backing storage, interleaved measures recycling cost on a single working slot. A pool's headline advantage over `malloc` is much larger in the interleaved regime (no heap walk, no coalescing) than in the bulk regime (where `malloc` can amortise its bookkeeping over many allocations); reporting only one would either flatter or under-sell the pool depending on which regime was chosen. Reporting both is the only honest option.

- **Single repeat (no statistics).** Rejected because workstation-grade noise (±5–15 % on a quiet system, much more on a CI runner) makes a single 1M-iteration number an opinion, not a measurement. Ten repeats with min/median/max/stddev disclosure is the minimum that survives reviewer challenge; it is also cheap (the whole benchmark binary runs in well under a minute).

- **No anti-optimization barriers.** Rejected because Release-mode optimisers will elide loops whose body has no observable side effect — Tier-1 compilers do this routinely on test microbenchmarks. The risk is not theoretical: clang at `-O2` will collapse `void* p = malloc(64); free(p);` into nothing if `p` is unused. Without barriers we would be timing the cost of an empty loop and reporting it as the allocator cost. The per-iteration `volatile` byte write plus the `do_not_optimize(p)` barrier across the timed region together cost a few ns/op of overhead common to both allocators, so the *ratio* between them stays meaningful.

- **CI numeric assertions.** Rejected for the runner-noise reasons documented in §8 above. The `bench-smoke` job is a build / run gate, not a performance gate. Performance assertions belong in a controlled environment; placing them on noisy shared runners produces flaky red and degrades the value of the CI signal.

## Consequences

- **Contract surface.** A new public-but-internal target: `pool_vs_malloc_bench` under `src/bench/cpp/it/d4np/memorypool/`, gated by `PBR_MEMORY_POOL_BUILD_BENCHMARKS=ON`. A new `bench` preset that turns benchmarks on, tests off, with `CMAKE_BUILD_TYPE=Release`. A new CI job (`bench-smoke`). A new docs tree at `docs/bench/`.

- **Build matrix impact.** Benchmarks remain **off by default**. The pre-existing 14-cell CI matrix is unchanged. The new `bench-smoke` cell adds one Ubuntu 24.04 row that builds the bench binary and runs it briefly — adds roughly 30 s to the CI wall-clock.

- **No new dependency.** The bench TU consumes only the C / C++ standard library and the project's own `pbr::memory_pool` target. Spec §3.3's zero-external-dependency posture is preserved into the bench tree.

- **Reporting cadence.** Every release that closes a milestone ships an updated bench report in `docs/bench/`. M4.5 will add concurrent-mode rows to the same scenario tables; M5.x will add a dynamic-growth row. The format is forward-compatible — new rows append, no existing column changes.

- **Limitation: single-host numbers per release.** The committed report describes performance on *one* host. Multi-host coverage is desirable but not required for v0.2.0; contributors who wish to run the bench on additional hosts can add files following the same template. M7.6's spec-compliance acceptance audit confirms a bench report exists; it does not require coverage across every Tier-1 host.

- **Limitation: no profiler integration.** The bench measures wall time only; it does not break down hits in L1 / L2 / L3, branch mispredictions, or instruction counts. A `perf stat` / VTune wrapper around the binary is an obvious M7.x extension if performance regressions need finer attribution. For v0.2.0 the headline ratio is sufficient evidence that the pool delivers the constant-time advantage the spec promises.

- **Future-proofing.** When ADR-0011's `Pool::make` Factory Method gains polymorphic implementations (M4.x thread-safe variants, M5.x growable variants), the bench binary acquires a fourth CLI flag (`--variant {fixed,growable,locked,lock-free}`) and the scenario rows multiply per variant. The TSV output format is designed to absorb that growth without re-parsing.

## References

- [Spec §6.3](../specs/01_spec_cpp_memory_pool.md) — the benchmark contract.
- [ROADMAP §2.9](../../ROADMAP.md) — the milestone item this ADR fulfils.
- [ROADMAP §4.5](../../ROADMAP.md) — the comparative concurrent re-run this ADR sets the format for.
- [ADR-0005](0005-toolchain-matrix-and-supported-platforms.md) §3 — the Tier-1 compilers the methodology must remain portable across.
- [ADR-0009](0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) §2 — the `block_size` constraints the chosen 64-byte default satisfies.
- [ADR-0013](0013-doxygen-for-api-markdown-for-narrative.md) §4 — the narrative-goes-in-Markdown rule that places the bench report at `docs/bench/...`, not in Doxygen.
- *Benchmarking Engineering Code* — Chandler Carruth, CppCon 2015 (anti-optimization barrier pattern).
- *Quantifying the performance of garbage collection vs. explicit memory management* — Hertz & Berger, OOPSLA 2005 (the foundational discussion of allocator microbenchmark design that this ADR draws on for the bulk + interleaved scenario split).
