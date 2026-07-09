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
                             [--threads N]
                             [--scenario {bulk|interleaved|concurrent|growth|both|all}]
                             [--percentiles]
```

Defaults match the spec §6.3 contract — 1,000,000 iterations, 10 repeats (first discarded as warm-up, nine measured), 64-byte block size, both the `bulk` and `interleaved` scenarios (`concurrent` and `growth` are opt-in via `--scenario`).

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

## Tail-latency percentiles (`--percentiles`, ADR-0045)

`--percentiles` appends a **separate** TSV table timed **per operation** (one sample per op, rather than one per repeat), so p50/p90/p99/p999 are meaningful. It covers the interleaved scenario for every allocator plus a dynamic-pool **growth** row whose p99/p999 expose the microsecond-scale spike of a growth event — the tail a latency-sensitive consumer cares about, which the aggregate median averages away.

```text
# percentile table — per-op timing (ADR-0045); tail/relative, not absolute per-op cost
scenario     allocator  p50_ns/op  p90_ns/op  p99_ns/op  p999_ns/op  samples
interleaved  pool       3.000      4.000      6.000      31.000      1000000
growth       pool       3.000      4.000      6.000      7000.000    1000000
```

**Caveat:** per-op timing carries a fixed clock-read overhead, and its resolution is the platform `steady_clock` tick — ≈1 ns on Linux/macOS, but ≈100 ns on Windows, where the columns **quantize** to the tick. Treat the percentile columns as a **tail / relative** comparison and a way to surface microsecond-scale events (a growth spike shows as p999 ≫ p50 on every platform); the aggregate ns/op table above stays authoritative for absolute per-op cost. The `--percentiles` path is opt-in precisely so its overhead never perturbs those aggregate numbers.

## External allocator baselines (jemalloc / tcmalloc, ADR-0045)

jemalloc and tcmalloc are **optional** baselines, **loaded at run time via `dlopen`** (POSIX). They are deliberately *not* linked: linking either would make its `malloc` interpose the whole process (turning the "malloc" row into that allocator), and linking both crashes. Loading each with `RTLD_LOCAL` and calling only its explicit API (`mallocx`/`dallocx`, `tc_malloc`/`tc_free`) keeps the system `malloc` intact and lets all of pool / malloc / jemalloc / tcmalloc appear as distinct rows in one run. If an allocator's shared object is not installed, it is a silent skip; on non-POSIX hosts (MSVC) the mechanism is compiled out and the output is dependency-free (spec §3.3) but for a `# baselines: malloc` disclosure line. On Debian/Ubuntu just install the runtime libraries and run — no reconfigure needed:

```bash
sudo apt-get install -y libjemalloc2 libgoogle-perftools4t64
cmake --preset bench && cmake --build --preset bench
./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench --scenario all --percentiles
# the header line "# baselines: malloc jemalloc tcmalloc" confirms both loaded
```

## Reporting

Every release that closes a milestone ships a bench report under [`docs/bench/`](../../../../../../docs/bench/). One file per release × host, named `v<X.Y.Z>-<os>-<compiler>-<arch>.md`:

```text
docs/bench/v0.2.0-windows-msvc-x64.md
docs/bench/v0.2.0-linux-gcc-x64.md       (optional, contributor-driven)
docs/bench/v0.2.0-macos-apple-clang-arm64.md   (optional, contributor-driven)
```

The file wraps the raw benchmark output in a Markdown report disclosing the full host (CPU model + clock, OS version, RAM, compiler version, build flags) and adding an *Observations* section. ADR-0014 §7 fixes the file shape.

## CI

The `bench-smoke` job in [`.github/workflows/ci.yml`](../../../../../../.github/workflows/ci.yml) builds the bench binary with the `bench` preset and runs it with `--iterations 10000 --repeats 3` — proves the binary still compiles, links, and runs to completion. A companion `bench-baselines` job (Linux) installs jemalloc + tcmalloc and runs `--scenario all --percentiles`, asserting the feature-detected baseline rows and the percentile table are present (ADR-0045). Both deliberately **not** assert numeric thresholds; shared-runner noise makes that gate flaky without adding signal. ADR-0014 §8 documents the rationale.

## Methodology snapshot

The full methodology — scenarios, repeat handling, anti-optimization barriers, statistics, output format, reporting cadence, CI integration, six rejected alternatives — lives in [ADR-0014](../../../../../../docs/adr/0014-microbenchmark-methodology-pool-vs-malloc.md). This README is the operational quick-start; ADR-0014 is the contract.
