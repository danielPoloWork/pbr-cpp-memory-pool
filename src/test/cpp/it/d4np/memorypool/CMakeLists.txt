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

# ROADMAP item 2.8 — spec §6.2 Valgrind demonstrative test. The directory
# carries its own CMakeLists.txt because the target consumes a strict ANSI
# C89 .c source and overrides the project-wide C99 floor on its own scope.
add_subdirectory(spec_6_2_valgrind)
