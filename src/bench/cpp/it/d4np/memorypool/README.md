# Benchmarks — `it::d4np::memorypool`

Benchmark sources for `pbr-cpp-memory-pool`. The contract is fixed by [ADR-0014](../../../../../../docs/adr/0014-microbenchmark-methodology-pool-vs-malloc.md); this README is the operational how-to.

## Targets

| Target                  | Source                                             | Implements           |
|-------------------------|----------------------------------------------------|----------------------|
| `pool_vs_malloc_bench`  | [`pool_vs_malloc_bench.cpp`](pool_vs_malloc_bench.cpp) | spec §6.3, ROADMAP §2.9 |

The full taxonomy (block sizes, growth modes, threading variants) lands incrementally — M4.5 will extend `pool_vs_malloc_bench` with a `--variant {fixed,locked,lock-free}` flag, M5.x with `{fixed,growable}`. ADR-0014 §6 fixes the TSV column shape so the format absorbs those rows without re-parsing.

## Build

Benchmarks are **off by default**. Either invoke the dedicated `bench` preset or pass the option explicitly.

```bash
# Recommended — the bench preset is Release + benchmarks ON + tests OFF.
cmake --preset bench
cmake --build --preset bench
```

```bash
# Equivalent manual invocation, if a different preset is needed.
cmake -S . -B build/manual-bench -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPBR_MEMORY_POOL_BUILD_TESTS=OFF \
    -DPBR_MEMORY_POOL_BUILD_BENCHMARKS=ON
cmake --build build/manual-bench
```

The binary lands at `build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench` (`.exe` on Windows).

## Run

```text
usage: pool_vs_malloc_bench [--iterations N] [--repeats N] [--block-size N]
                             [--scenario {bulk|interleaved|both}]
```

Defaults match the spec §6.3 contract — 1,000,000 iterations, 10 repeats (first discarded as warm-up, nine measured), 64-byte block size, both scenarios.

```bash
# Full canonical run — what the committed bench reports use.
./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench

# Quick smoke run (matches the CI `bench-smoke` cell).
./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench \
    --iterations 10000 --repeats 3

# Single scenario, default iteration count.
./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench \
    --scenario interleaved
```

The binary writes everything to stdout. Redirect to a file when capturing a release report:

```bash
./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench > raw-bench.txt
```

## Output format

ADR-0014 §6 fixes the three-section layout: a `#`-prefixed header block, a TSV body, and a `#`-prefixed headline ratio summary. Example abbreviated output:

```text
# pool-vs-malloc benchmark (M2.9 / spec §6.3)
# methodology: ADR-0014
# compiler: msvc 195136247
# hardware_concurrency: 4
# max_align_t: 8 bytes
# config: iterations=1000000 repeats=10 block_size=64
# (the human-runner appends host / cpu / os details when committing the report)

scenario   allocator  region      min_ns/op  median_ns/op  mean_ns/op  max_ns/op  stddev_ns/op
bulk       pool       alloc       4.290      4.390         4.340       4.390      0.050
bulk       pool       free        7.740      7.960         7.850       7.960      0.110
bulk       malloc     alloc       63.380     65.110        64.245      65.110     0.865
bulk       malloc     free        40.530     41.830        41.180      41.830     0.650
interleaved pool      alloc+free  8.550      8.990         8.770       8.990      0.220
interleaved malloc    alloc+free  47.600     51.360        49.480      51.360     1.880

# headline: bulk-alloc: malloc / pool = 14.831x
# headline: bulk-free: malloc / pool = 5.255x
# headline: interleaved: malloc / pool = 5.713x
```

All times are in **nanoseconds per single operation** (`ns/op`), computed as `(elapsed_ns_for_repeat / iterations)`. Min / median / mean / max / stddev are across the measured repeats only (the warm-up at index 0 is dropped before statistics).

## Reporting

Every release that closes a milestone ships a bench report under [`docs/bench/`](../../../../../../docs/bench/). One file per release × host, named `v<X.Y.Z>-<os>-<compiler>-<arch>.md`:

```text
docs/bench/v0.2.0-windows-msvc-x64.md
docs/bench/v0.2.0-linux-gcc-x64.md       (optional, contributor-driven)
docs/bench/v0.2.0-macos-apple-clang-arm64.md   (optional, contributor-driven)
```

The file wraps the raw benchmark output in a Markdown report disclosing the full host (CPU model + clock, OS version, RAM, compiler version, build flags) and adding an *Observations* section. ADR-0014 §7 fixes the file shape.

## CI

The `bench-smoke` job in [`.github/workflows/ci.yml`](../../../../../../.github/workflows/ci.yml) builds the bench binary with the `bench` preset and runs it with `--iterations 10000 --repeats 3` — proves the binary still compiles, links, and runs to completion. It deliberately does **not** assert numeric thresholds; shared-runner noise makes that gate flaky without adding signal. ADR-0014 §8 documents the rationale.

## Methodology snapshot

The full methodology — scenarios, repeat handling, anti-optimization barriers, statistics, output format, reporting cadence, CI integration, six rejected alternatives — lives in [ADR-0014](../../../../../../docs/adr/0014-microbenchmark-methodology-pool-vs-malloc.md). This README is the operational quick-start; ADR-0014 is the contract.
