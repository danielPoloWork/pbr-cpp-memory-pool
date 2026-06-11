# Benchmark reports

Committed performance measurements for `pbr-cpp-memory-pool`. One file per release × host combination, with the spelling `v<X.Y.Z>-<os>-<compiler>-<arch>.md` per [ADR-0014](../adr/0014-microbenchmark-methodology-pool-vs-malloc.md) §7.

## Index

| Release | Host                     | Report                                                                  |
|---------|--------------------------|-------------------------------------------------------------------------|
| v0.2.0  | Windows / MSVC / x64     | [`v0.2.0-windows-msvc-x64.md`](v0.2.0-windows-msvc-x64.md)              |

Contributors who run the benchmark on other hosts (Linux / GCC or Clang, macOS / Apple Clang) can append a row to this table and commit the corresponding report in the same PR. The methodology (scenarios, iterations, repeats, anti-optimization barriers, statistical summary) is binding across hosts; only the host disclosure block and the numbers themselves vary.

## How to produce a new report

1. Build with the `bench` preset on a quiet host (no other interactive workloads):

   ```bash
   cmake --preset bench
   cmake --build --preset bench
   ```

2. Run the canonical benchmark and capture the output:

   ```bash
   ./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench > raw.txt
   ```

3. Wrap `raw.txt` in a Markdown report matching the shape of the existing reports — Host / Run configuration / Raw output / Results / Observations / How to reproduce sections. The maintainer's [`v0.2.0-windows-msvc-x64.md`](v0.2.0-windows-msvc-x64.md) is the reference template.

4. Add a row to the *Index* table above pointing at the new file. In the same PR, refresh the README *Performance* headline if the new report represents the canonical host for the current release.

## Contract

The contract that binds every file in this directory — what to measure, how to time it, what to report — is fixed by [ADR-0014](../adr/0014-microbenchmark-methodology-pool-vs-malloc.md). The operational quick-start lives in [`src/bench/cpp/it/d4np/memorypool/README.md`](../../src/bench/cpp/it/d4np/memorypool/README.md).
