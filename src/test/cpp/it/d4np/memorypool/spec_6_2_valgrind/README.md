# Spec §6.2 — Valgrind verification

This directory carries the **literal demonstrative test** mandated by [`ROADMAP.md`](../../../../../../../ROADMAP.md) item 2.8 and named in the functional specification at [`docs/specs/01_spec_cpp_memory_pool.md`](../../../../../../../docs/specs/01_spec_cpp_memory_pool.md) §6.2.

## The literal spec command

```bash
gcc -g -O0 test_pool.c memory_pool.c -o test_pool
valgrind --leak-check=full --show-leak-kinds=all ./test_pool
```

**Success criterion** (verbatim from the spec): `ERROR SUMMARY: 0 errors from 0 contexts`.

## How CI reproduces it

The implementation is C++17 not C (ADR-0009 §1 — backing acquired via `::operator new(size, std::align_val_t)`), so the literal command needs two structural substitutions:

| Spec command          | Repo command                      | Why                                                                                   |
|-----------------------|-----------------------------------|---------------------------------------------------------------------------------------|
| `gcc` (on the impl)   | `g++`                             | `memory_pool.cpp` is C++17; the link needs `g++` to bring in `libstdc++`              |
| `memory_pool.c`       | `memory_pool.cpp`                 | the impl file name reflects the chosen language (ADR-0009 §1)                         |
| (no `-std`)           | `-std=c89` on `gcc`, `-std=c++17` on `g++` | pin the standards explicitly so a future toolchain default change cannot drift the test |

The exact lines run by the `valgrind` job in [`.github/workflows/ci.yml`](../../../../../../../.github/workflows/ci.yml) are:

```bash
gcc -std=c89 -pedantic -g -O0 -Isrc/main/cpp \
    -c src/test/cpp/it/d4np/memorypool/spec_6_2_valgrind/test_pool.c \
    -o build/valgrind/test_pool.o
g++ -std=c++17 -g -O0 -Isrc/main/cpp \
    -c src/main/cpp/it/d4np/memorypool/memory_pool.cpp \
    -o build/valgrind/memory_pool.o
g++ -g -O0 \
    build/valgrind/test_pool.o build/valgrind/memory_pool.o \
    -o build/valgrind/test_pool
valgrind --leak-check=full --show-leak-kinds=all \
    --errors-for-leak-kinds=definite,indirect \
    --error-exitcode=1 \
    build/valgrind/test_pool
```

The CI job additionally greps the Valgrind output for the literal spec success criterion `ERROR SUMMARY: 0 errors from 0 contexts`, so a green build proves both the structural exit-code gate and the spec-named sentence.

The `--errors-for-leak-kinds=definite,indirect` flag is the only non-spec addition. Without it, leaks are reported but do not affect the exit code, so a regression that leaked the backing buffer would pass the `--error-exitcode=1` gate while still violating spec §3.1. The selection covers the two leak categories that always indicate a bug (`definite` — fully unreachable, `indirect` — reachable only through a definitely-leaked block); `still reachable` and `possible` are left informational so global libstdc++ state does not trip the gate.

## What the test exercises

| Spec scenario                              | How the test exercises it                                                                  |
|--------------------------------------------|--------------------------------------------------------------------------------------------|
| §6.1 happy path                            | `memory_pool_create(BLOCK_SIZE, BLOCK_COUNT)` returns non-NULL                             |
| §6.1 null inputs                           | `memory_pool_free(NULL, NULL)`, `memory_pool_free(pool, NULL)`, `memory_pool_alloc(NULL)`  |
| §6.1 exhaustion                            | every `BLOCK_COUNT`-th alloc succeeds; the next returns NULL                               |
| §6.1 foreign / out-of-range pointer        | `memory_pool_free(pool, malloc'd_block)` and `memory_pool_free(pool, &stack_local)` no-op  |
| §3.1 destroy releases all memory to the OS | full alloc / free cycle twice, then `memory_pool_destroy` — Valgrind audits the final HEAP |

## CMake / CTest integration

The `test_pool` binary is also registered as a CTest target so it can be exercised locally without invoking `gcc` / `g++` directly. The CMake-driven path uses the project's standard configuration (warnings-as-errors, project-wide C99 with C90 override on this target, full static linkage against `pbr::memory_pool`):

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure -R spec_6_2_valgrind
```

For the leak audit locally, wrap the CMake-built binary in Valgrind directly:

```bash
valgrind --leak-check=full --show-leak-kinds=all \
    build/debug/src/test/cpp/it/d4np/memorypool/spec_6_2_valgrind/spec_6_2_valgrind_test_pool
```

The CMake target name (`spec_6_2_valgrind_test_pool`) is prefixed to avoid collision with any future top-level `test_pool` target; the on-disk source file keeps the spec-named `test_pool.c` so the spec command stays readable.

## Scope

This file is a *verification artifact*, not a unit-test surface. The granular doctest cases for each correctness scenario live in [`../pool_smoke_test.cpp`](../pool_smoke_test.cpp); this directory exists only to make the spec §6.2 invocation reproducible 1:1 (modulo the documented C → C++ structural substitution) and CI-gated.
