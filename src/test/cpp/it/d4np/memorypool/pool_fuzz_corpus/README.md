# Fuzzing seed corpus — `pool_fuzz`

Seed inputs for the coverage-guided fuzzing harness
[`pool_fuzz.cpp`](../pool_fuzz.cpp) (ROADMAP item 9.3, decided in
[ADR-0044](../../../../../../../docs/adr/0044-coverage-guided-fuzzing-harness.md)).

The harness drives the pool's public surface — `create` →
`alloc`/`free` → `destroy`, plus dynamic growth — through randomized
operation sequences and asserts the no-alias, canary-intact,
foreign-pointer-no-op, and instrumented-accounting invariants. A violation
aborts, so the fuzzer saves the offending input as a reproducer.

## Input format

Each input byte is interpreted as a tiny program:

- **byte 0** — block-size selector: `block_size = alignof(max_align_t) * (1 + (b & 3))`.
- **byte 1** — block-count selector: `block_count = 1 + (b & 7)`.
- **byte 2** — mode: bit 0 selects a dynamic-growth pool; bit 1 selects a
  growth factor of 2 or 3.
- **remaining bytes** — opcode stream, `op & 3`:
  - `0` allocate one block;
  - `1` free a live block (the next byte selects which, modulo the live count);
  - `2` free `NULL` (a no-op);
  - `3` free a foreign pointer (a defined no-op — ADR-0012).

A short or empty input is valid; it simply yields a short program.

## Running locally

The standalone replay binary (built by default with the tests, no fuzzing
engine required) replays every file passed on the command line — this is the
regression gate that runs on every platform, including MSVC:

```sh
cmake --preset debug
cmake --build build/debug --target pool_fuzz_replay
ctest --preset debug -R pool_fuzz_replay
```

A true coverage-guided run needs Clang's libFuzzer (POSIX). The `fuzz` preset
builds the `pool_fuzz` target with `-fsanitize=fuzzer,address,undefined`:

```sh
CC=clang CXX=clang++ cmake --preset fuzz
cmake --build build/fuzz --target pool_fuzz
# replay the seed corpus, then fuzz for a bounded time
./build/fuzz/src/test/cpp/it/d4np/memorypool/pool_fuzz \
    src/test/cpp/it/d4np/memorypool/pool_fuzz_corpus
./build/fuzz/src/test/cpp/it/d4np/memorypool/pool_fuzz \
    -max_total_time=60 src/test/cpp/it/d4np/memorypool/pool_fuzz_corpus
```

Any crash found is filed in the bug ledger (`docs/bugs/`,
[ADR-0039](../../../../../../../docs/adr/0039-bug-ledger-and-triage-protocol.md))
with the crashing input as the reproducer.
