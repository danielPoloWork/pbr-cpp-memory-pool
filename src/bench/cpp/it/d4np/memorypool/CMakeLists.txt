# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Daniel Polo
#
# Benchmark targets for pbr-cpp-memory-pool — ROADMAP §2.9 (M2.9, spec §6.3).
# Methodology contract: ADR-0014.
#
# Gated by the PBR_MEMORY_POOL_BUILD_BENCHMARKS option in the top-level
# CMakeLists.txt; only entered when the `bench` preset (or an equivalent
# manual configure with -DPBR_MEMORY_POOL_BUILD_BENCHMARKS=ON) is in use.

add_executable(pool_vs_malloc_bench
    pool_vs_malloc_bench.cpp)

# Threads for the M4.5 concurrent scenario.
find_package(Threads REQUIRED)
target_link_libraries(pool_vs_malloc_bench PRIVATE
    pbr::memory_pool
    Threads::Threads)

target_compile_features(pool_vs_malloc_bench PRIVATE cxx_std_17)

# The library's thread-safety macro is PRIVATE (ADR-0020); mirror the
# top-level cache variable onto the bench target so it can report which
# policy it was built against and clamp the concurrent scenario under NONE.
if(PBR_MEMORY_POOL_THREAD_SAFETY STREQUAL "MUTEX")
    target_compile_definitions(pool_vs_malloc_bench PRIVATE
        PBR_MEMORY_POOL_THREAD_SAFETY=PBR_MEMORY_POOL_THREAD_SAFETY_MUTEX)
elseif(PBR_MEMORY_POOL_THREAD_SAFETY STREQUAL "LOCKFREE")
    target_compile_definitions(pool_vs_malloc_bench PRIVATE
        PBR_MEMORY_POOL_THREAD_SAFETY=PBR_MEMORY_POOL_THREAD_SAFETY_LOCKFREE)
endif()

# ---------------------------------------------------------------------------
# Optional external-allocator baselines (ADR-0045). Feature-detected: when the
# library AND its header are both found, the bench compiles in an extra baseline
# row (and percentile row) for that allocator. NEVER a hard dependency — the
# default build stays zero-external-dependency (spec §3.3), and a missing
# allocator is a silent skip, not an error. jemalloc is used via its extended
# mallocx/dallocx API; tcmalloc via gperftools' explicit tc_malloc/tc_free.
# ---------------------------------------------------------------------------
find_library(PBR_JEMALLOC_LIB jemalloc)
find_path(PBR_JEMALLOC_INCLUDE_DIR jemalloc/jemalloc.h)
if(PBR_JEMALLOC_LIB AND PBR_JEMALLOC_INCLUDE_DIR)
    target_compile_definitions(pool_vs_malloc_bench PRIVATE PBR_BENCH_HAVE_JEMALLOC=1)
    target_include_directories(pool_vs_malloc_bench PRIVATE "${PBR_JEMALLOC_INCLUDE_DIR}")
    target_link_libraries(pool_vs_malloc_bench PRIVATE "${PBR_JEMALLOC_LIB}")
    message(STATUS "  bench baseline: jemalloc (${PBR_JEMALLOC_LIB})")
endif()

find_library(PBR_TCMALLOC_LIB tcmalloc)
find_path(PBR_TCMALLOC_INCLUDE_DIR gperftools/tcmalloc.h)
if(PBR_TCMALLOC_LIB AND PBR_TCMALLOC_INCLUDE_DIR)
    target_compile_definitions(pool_vs_malloc_bench PRIVATE PBR_BENCH_HAVE_TCMALLOC=1)
    target_include_directories(pool_vs_malloc_bench PRIVATE "${PBR_TCMALLOC_INCLUDE_DIR}")
    target_link_libraries(pool_vs_malloc_bench PRIVATE "${PBR_TCMALLOC_LIB}")
    message(STATUS "  bench baseline: tcmalloc (${PBR_TCMALLOC_LIB})")
endif()

# Property name used by IDEs that group targets; matches the layout under
# src/bench/cpp/.
set_target_properties(pool_vs_malloc_bench PROPERTIES
    FOLDER "bench")
