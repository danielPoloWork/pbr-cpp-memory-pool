# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Daniel Polo
#
# Test targets for pbr-cpp-memory-pool. Framework choice and CTest integration
# governed by ADR-0007. The list of test executables grows as milestones land;
# each one self-registers with CTest below.

add_executable(pool_smoke_test
    pool_smoke_test.cpp)

target_link_libraries(pool_smoke_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(pool_smoke_test PRIVATE cxx_std_17)

add_test(
    NAME    pool_smoke
    COMMAND pool_smoke_test)

set_tests_properties(pool_smoke PROPERTIES
    LABELS "smoke;milestone-1")

# ROADMAP item 3.2 — TypedPool<T> type-safe surface (ADR-0017). The
# template is header-only, so this binary is also the compile-time proof
# that typed_pool.hpp instantiates cleanly under the full warning set.
add_executable(typed_pool_test
    typed_pool_test.cpp)

target_link_libraries(typed_pool_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(typed_pool_test PRIVATE cxx_std_17)

add_test(
    NAME    typed_pool
    COMMAND typed_pool_test)

set_tests_properties(typed_pool PROPERTIES
    LABELS "smoke;milestone-3")

# ROADMAP item 3.3 — PoolAllocator<T> STL adapter (ADR-0018). The adapter is
# header-only, so this binary is also the compile-time proof that
# pool_allocator.hpp instantiates cleanly (including through std::list and
# std::vector) under the full warning set.
add_executable(pool_allocator_test
    pool_allocator_test.cpp)

target_link_libraries(pool_allocator_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(pool_allocator_test PRIVATE cxx_std_17)

add_test(
    NAME    pool_allocator
    COMMAND pool_allocator_test)

set_tests_properties(pool_allocator PROPERTIES
    LABELS "smoke;milestone-3")

# ROADMAP item 3.4 — read-only free-list diagnostic Iterator (ADR-0019).
# The iterator is header-only and gated behind PBR_MEMORY_POOL_DIAGNOSTICS;
# the test TU compiles in both states (a placeholder case when the surface
# is gated out), so this binary builds in every configuration.
add_executable(free_list_iterator_test
    free_list_iterator_test.cpp)

target_link_libraries(free_list_iterator_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(free_list_iterator_test PRIVATE cxx_std_17)

add_test(
    NAME    free_list_iterator
    COMMAND free_list_iterator_test)

set_tests_properties(free_list_iterator PROPERTIES
    LABELS "smoke;milestone-3")

# ROADMAP item 3.5 — end-to-end integration of PoolAllocator<T> (ADR-0018)
# with std::list, std::vector, and a hand-written allocator-aware container.
add_executable(container_integration_test
    container_integration_test.cpp)

target_link_libraries(container_integration_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(container_integration_test PRIVATE cxx_std_17)

add_test(
    NAME    container_integration
    COMMAND container_integration_test)

set_tests_properties(container_integration PROPERTIES
    LABELS "integration;milestone-3")

# ROADMAP item 4.4 — concurrent stress tests for the thread-safe policies
# (ADR-0020). The cases are gated behind PBR_MEMORY_POOL_THREAD_SAFETY != NONE
# (a placeholder runs under the default single-threaded build). The library's
# thread-safety macro is PRIVATE (ADR-0020), so mirror the top-level cache
# variable onto this target so the gate sees the mode the library was built
# with. Links Threads for std::thread.
find_package(Threads REQUIRED)
add_executable(concurrency_stress_test
    concurrency_stress_test.cpp)

target_link_libraries(concurrency_stress_test PRIVATE
    pbr::memory_pool
    doctest::doctest
    Threads::Threads)

target_compile_features(concurrency_stress_test PRIVATE cxx_std_17)

if(PBR_MEMORY_POOL_THREAD_SAFETY STREQUAL "MUTEX")
    target_compile_definitions(concurrency_stress_test PRIVATE
        PBR_MEMORY_POOL_THREAD_SAFETY=PBR_MEMORY_POOL_THREAD_SAFETY_MUTEX)
elseif(PBR_MEMORY_POOL_THREAD_SAFETY STREQUAL "LOCKFREE")
    target_compile_definitions(concurrency_stress_test PRIVATE
        PBR_MEMORY_POOL_THREAD_SAFETY=PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE)
endif()

add_test(
    NAME    concurrency_stress
    COMMAND concurrency_stress_test)

set_tests_properties(concurrency_stress PROPERTIES
    LABELS "stress;milestone-4")

# ROADMAP item 5.4 — exhaustion-and-grow tests for dynamic growth (ADR-0022 /
# ADR-0023 / ADR-0024). Mirrors the library's PRIVATE thread-safety macro onto
# the target so the TU knows whether it is a lock-free build (which rejects
# dynamic mode) or a NONE/MUTEX build (which exercises the full growth matrix).
add_executable(dynamic_growth_test
    dynamic_growth_test.cpp)

target_link_libraries(dynamic_growth_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(dynamic_growth_test PRIVATE cxx_std_17)

if(PBR_MEMORY_POOL_THREAD_SAFETY STREQUAL "MUTEX")
    target_compile_definitions(dynamic_growth_test PRIVATE
        PBR_MEMORY_POOL_THREAD_SAFETY=PBR_MEMORY_POOL_THREAD_SAFETY_MUTEX)
elseif(PBR_MEMORY_POOL_THREAD_SAFETY STREQUAL "LOCKFREE")
    target_compile_definitions(dynamic_growth_test PRIVATE
        PBR_MEMORY_POOL_THREAD_SAFETY=PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE)
endif()

add_test(
    NAME    dynamic_growth
    COMMAND dynamic_growth_test)

set_tests_properties(dynamic_growth PROPERTIES
    LABELS "growth;milestone-5")

# ROADMAP item 6.1 — the InstrumentedPool Decorator (ADR-0025). Header-only;
# the test handles make_dynamic's nullopt under a lock-free build, so no macro
# mirror is needed.
add_executable(instrumented_pool_test
    instrumented_pool_test.cpp)

target_link_libraries(instrumented_pool_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(instrumented_pool_test PRIVATE cxx_std_17)

add_test(
    NAME    instrumented_pool
    COMMAND instrumented_pool_test)

set_tests_properties(instrumented_pool PROPERTIES
    LABELS "smoke;milestone-6")

# ROADMAP item 6.3 — zero-overhead-when-disabled verification (ADR-0025 §5).
# The proof is structural (static_asserts on type footprint + opt-in-by-type
# surface) plus a runtime behavioural-equivalence check, so the binary builds
# and passes in every configuration — including the Release matrix cells, which
# is the "in release builds" coverage the roadmap item asks for. Header-only;
# no thread-safety macro mirror is needed.
add_executable(zero_overhead_test
    zero_overhead_test.cpp)

target_link_libraries(zero_overhead_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(zero_overhead_test PRIVATE cxx_std_17)

add_test(
    NAME    zero_overhead
    COMMAND zero_overhead_test)

set_tests_properties(zero_overhead PROPERTIES
    LABELS "smoke;milestone-6")

# ROADMAP item 9.1 — the std::pmr::memory_resource Adapter (ADR-0042). The
# adapter is header-only and gated behind PBR_MEMORY_POOL_HAS_PMR (availability
# of <memory_resource>); the test TU compiles in both states (a placeholder
# case when the surface is gated out), so this binary builds in every
# configuration and is the compile-time proof that pool_memory_resource.hpp
# instantiates cleanly (including through std::pmr::list / std::pmr::vector)
# under the full warning set.
add_executable(pool_memory_resource_test
    pool_memory_resource_test.cpp)

target_link_libraries(pool_memory_resource_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(pool_memory_resource_test PRIVATE cxx_std_17)

add_test(
    NAME    pool_memory_resource
    COMMAND pool_memory_resource_test)

set_tests_properties(pool_memory_resource PROPERTIES
    LABELS "smoke;milestone-9")

# ROADMAP item 9.2 — opt-in debug hardening (ADR-0043). The hardening surface
# is gated behind PBR_MEMORY_POOL_HARDENING, which the library defines PUBLIC
# (unlike the PRIVATE thread-safety macro), so linking pbr::memory_pool
# propagates the gate to this target automatically — no cache-variable mirror is
# needed. The test TU compiles in both states (a placeholder case when the
# surface is gated out, mirroring free_list_iterator_test), so this binary
# builds in every configuration and, under the `harden` preset, proves that a
# use-after-free, a buffer overflow past block_size, and a double-free are each
# detected deterministically.
add_executable(pool_hardening_test
    pool_hardening_test.cpp)

target_link_libraries(pool_hardening_test PRIVATE
    pbr::memory_pool
    doctest::doctest)

target_compile_features(pool_hardening_test PRIVATE cxx_std_17)

add_test(
    NAME    pool_hardening
    COMMAND pool_hardening_test)

set_tests_properties(pool_hardening PROPERTIES
    LABELS "smoke;milestone-9")

# ROADMAP item 9.3 — coverage-guided fuzzing harness (ADR-0044). The harness in
# pool_fuzz.cpp exposes LLVMFuzzerTestOneInput plus a standalone replay main.
#
# pool_fuzz_replay is built with the normal test suite (no fuzzing engine): its
# main replays each seed-corpus file, so the corpus is a portable regression
# gate that runs on every platform — including MSVC, where libFuzzer is
# unavailable. Being a normal test target it is also covered by the clang-tidy
# gate. The corpus README is excluded from the replayed file list.
set(_pool_fuzz_corpus_dir "${CMAKE_CURRENT_SOURCE_DIR}/pool_fuzz_corpus")
file(GLOB _pool_fuzz_corpus_files CONFIGURE_DEPENDS "${_pool_fuzz_corpus_dir}/*")
list(FILTER _pool_fuzz_corpus_files EXCLUDE REGEX "/README\\.md$")

add_executable(pool_fuzz_replay
    pool_fuzz.cpp)

target_link_libraries(pool_fuzz_replay PRIVATE
    pbr::memory_pool)

target_compile_features(pool_fuzz_replay PRIVATE cxx_std_17)

add_test(
    NAME    pool_fuzz_replay
    COMMAND pool_fuzz_replay ${_pool_fuzz_corpus_files})

set_tests_properties(pool_fuzz_replay PROPERTIES
    LABELS "smoke;milestone-9")

# The true coverage-guided libFuzzer target is opt-in (PBR_MEMORY_POOL_BUILD_FUZZERS,
# default OFF — declared at the top level) and Clang-only, since libFuzzer is a
# Clang runtime. A dedicated CI job builds and runs it under a bounded time
# budget; it never perturbs the release build or the ADR-0014 benchmark numbers.
if(PBR_MEMORY_POOL_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "PBR_MEMORY_POOL_BUILD_FUZZERS requires Clang (libFuzzer is a Clang "
            "runtime); got '${CMAKE_CXX_COMPILER_ID}'. The portable pool_fuzz_replay "
            "target already gates the seed corpus on every platform.")
    endif()
    add_executable(pool_fuzz
        pool_fuzz.cpp)

    target_link_libraries(pool_fuzz PRIVATE
        pbr::memory_pool)

    target_compile_features(pool_fuzz PRIVATE cxx_std_17)
    target_compile_definitions(pool_fuzz PRIVATE PBR_MEMORY_POOL_LIBFUZZER=1)
    # The `fuzz` preset compiles every TU (library + harness) with
    # -fsanitize=fuzzer-no-link for edge coverage; this target links the
    # libFuzzer runtime + entry point. Splitting compile-coverage from
    # link-engine is the standard libFuzzer wiring and keeps the coverage
    # feedback spanning the library under test, not just the harness.
    target_link_options(pool_fuzz PRIVATE -fsanitize=fuzzer)

    # libFuzzer replays file / directory arguments and exits 0, so this gates on
    # the seed corpus as a fast regression check; the CI job adds a bounded run.
    add_test(
        NAME    pool_fuzz_corpus
        COMMAND pool_fuzz ${_pool_fuzz_corpus_dir})

    set_tests_properties(pool_fuzz_corpus PROPERTIES
        LABELS "fuzz;milestone-9")
endif()

# ROADMAP item 2.8 — spec §6.2 Valgrind demonstrative test. The directory
# carries its own CMakeLists.txt because the target consumes a strict ANSI
# C89 .c source and overrides the project-wide C99 floor on its own scope.
add_subdirectory(spec_6_2_valgrind)
