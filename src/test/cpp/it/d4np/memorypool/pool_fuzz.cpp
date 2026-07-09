// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file pool_fuzz.cpp
 * @brief Coverage-guided fuzzing harness for the pool surface (M9.3 / ADR-0044).
 *
 * `LLVMFuzzerTestOneInput` interprets the fuzzer's byte buffer as a small
 * program: the first bytes configure a pool (block size / count / fixed-vs-
 * dynamic), and the remaining bytes drive a state machine of `alloc`,
 * `free(valid)`, `free(NULL)`, and `free(foreign)` operations. A shadow model
 * of the live blocks lets the harness assert the invariants the pool promises
 * (ADR-0009 / ADR-0012 / ADR-0022), so a violation surfaces as a crash the
 * fuzzer can minimise and save as a reproducer:
 *
 *  - every successful allocation returns a pointer that is not already live
 *    (no two live blocks alias — a corrupted free list vending a block twice
 *    is caught here);
 *  - a per-block canary written on allocation is still intact at free time
 *    (nothing wrote through a block while the harness owned it);
 *  - freeing `NULL` or a foreign / out-of-range pointer is a no-op that never
 *    corrupts the free list (ADR-0012);
 *  - the `InstrumentedPool` live count tracks the shadow set exactly, and after
 *    draining every block the pool reports zero live and balanced counters
 *    (ADR-0025).
 *
 * Both fixed and dynamic-growth pools are exercised; a dynamic pool grows
 * implicitly as allocation runs past its initial capacity (ADR-0022 / ADR-0024).
 *
 * The harness is deliberately engine-agnostic. Built with Clang's
 * `-fsanitize=fuzzer` it is a libFuzzer target (`PBR_MEMORY_POOL_LIBFUZZER`);
 * built without it, the standalone `main` below replays the files named on the
 * command line (the OSS-Fuzz "StandaloneFuzzTargetMain" shape), so the seed
 * corpus runs as a portable regression gate everywhere the project builds —
 * including MSVC, where libFuzzer is unavailable. See ADR-0044.
 */

#include <it/d4np/memorypool/instrumented_pool.hpp>
#include <it/d4np/memorypool/memory_pool.h>
#include <it/d4np/memorypool/memory_pool.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <vector>

namespace {

using it::d4np::memorypool::InstrumentedPool;

// A live allocation the harness is tracking: the block plus the canary byte it
// was filled with, so overlap / corruption is detectable on release.
struct LiveBlock {
    void* ptr_;
    unsigned char canary_;
};

// Bound the shadow set (and therefore any dynamic pool's growth) so a
// pathological all-allocate input cannot exhaust host memory or wall-clock.
constexpr std::size_t MAX_LIVE = 1024U;

// A forward byte cursor over the fuzzer input. A short or empty input is valid:
// once exhausted the cursor yields zero, which decodes to a short program.
class ByteReader {
public:
    ByteReader(const std::uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

    std::uint8_t next() noexcept {
        if (pos_ >= size_) {
            return 0U;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return data_[pos_++];
    }

    [[nodiscard]] bool done() const noexcept {
        return pos_ >= size_;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0U;
};

// A detected invariant violation is a real finding: report it and crash so the
// fuzzer (or the standalone corpus replay) saves the offending input. Not marked
// noexcept: the std::cerr insertion may in principle throw, and abort() follows
// on that path anyway, so leaving it potentially-throwing keeps it off the
// bugprone-exception-escape radar without changing the observable behaviour.
void expect(bool condition, const char* what) {
    if (!condition) {
        std::cerr << "pool_fuzz: invariant violated: " << what << '\n';
        std::abort();
    }
}

// Allocate one block, verify the no-alias invariant against the shadow set, and
// stamp it with a canary. Exhaustion (a fixed pool at capacity, or a failed
// growth) is a valid outcome, not a bug.
void do_alloc(InstrumentedPool& pool, std::vector<LiveBlock>& live, unsigned char canary) {
    if (live.size() >= MAX_LIVE) {
        return;
    }
    void* const block = pool.try_allocate();
    if (block == nullptr) {
        return;
    }
    for (const LiveBlock& held : live) {
        expect(held.ptr_ != block, "allocate returned an already-live block (free-list aliasing)");
    }
    const std::size_t bytes = pool.block_size();
    std::memset(block, canary, bytes);
    live.push_back(LiveBlock{block, canary});
}

// Free one live block chosen by @p index (modulo the live count, so any input
// byte selects a valid block — LIFO, FIFO, and interleaved orders all arise).
// Verifies the canary is intact before returning the block.
void do_free_valid(InstrumentedPool& pool, std::vector<LiveBlock>& live, std::size_t index) {
    if (live.empty()) {
        return;
    }
    const std::size_t at = index % live.size();
    const LiveBlock target = live.at(at);
    const std::size_t bytes = pool.block_size();
    const auto* const raw = static_cast<const unsigned char*>(target.ptr_);
    for (std::size_t i = 0; i < bytes; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        expect(raw[i] == target.canary_, "canary mismatch (overlap or write to a live block)");
    }
    pool.deallocate(target.ptr_);
    live.erase(live.begin() + static_cast<std::ptrdiff_t>(at));
}

// Drive the pool through the opcode stream and assert the accounting invariants
// after each step and after the final drain.
void run_program(const std::uint8_t* data, std::size_t size) {
    ByteReader reader{data, size};

    // Configure a valid pool from the leading bytes. block_size is a multiple of
    // max_align_t and >= sizeof(void*) by construction (ADR-0009 §2); block_count
    // is small so exhaustion and growth are reached quickly.
    constexpr std::size_t BASE_ALIGN = alignof(std::max_align_t);
    const std::size_t block_size = BASE_ALIGN * (1U + (reader.next() & 0x03U));
    const std::size_t block_count = 1U + (reader.next() & 0x07U);
    const std::uint8_t mode = reader.next();
    const bool dynamic = (mode & 0x01U) != 0U;
    const std::size_t growth_factor = 2U + ((mode >> 1U) & 0x01U);

    std::optional<InstrumentedPool> pool = dynamic
                                               ? InstrumentedPool::make_dynamic(block_size, block_count, growth_factor)
                                               : InstrumentedPool::make(block_size, block_count);
    // A dynamic pool is unsupported under the lock-free policy (ADR-0024 §2); the
    // fuzz build uses the default policy, but fall back to fixed so the harness
    // stays valid regardless of how the library was configured.
    if (!pool.has_value()) {
        pool = InstrumentedPool::make(block_size, block_count);
    }
    expect(pool.has_value(), "failed to create a pool from a valid configuration");
    if (!pool.has_value()) {
        return;  // unreachable after expect(); also proves the accesses below are guarded
    }
    InstrumentedPool& active = *pool;

    // A local object whose address is provably outside the pool backing — the
    // ADR-0012 foreign-pointer rejection path. Freed through the C core directly
    // so it does not perturb the instrumented counters.
    int foreign = 0;
    std::vector<LiveBlock> live;
    std::size_t peak = 0U;

    while (!reader.done()) {
        switch (reader.next() & 0x03U) {
        case 0U:
            do_alloc(active, live, static_cast<unsigned char>(0xA0U + (live.size() & 0x0FU)));
            break;
        case 1U:
            do_free_valid(active, live, reader.next());
            break;
        case 2U:
            // free(NULL) — a documented no-op (ADR-0012).
            active.deallocate(nullptr);
            break;
        default:
            // free(foreign) via the core — must be a defined no-op that
            // leaves the free list intact (ADR-0012).
            ::memory_pool_free(active.native_handle(), &foreign);
            break;
        }
        peak = (live.size() > peak) ? live.size() : peak;
        expect(active.stats().live_ == live.size(), "instrumented live count diverged from the shadow set");
    }

    // Drain everything and assert the pool is empty with balanced counters.
    while (!live.empty()) {
        do_free_valid(active, live, 0U);
    }
    const it::d4np::memorypool::PoolStats final_stats = active.stats();
    expect(final_stats.live_ == 0U, "blocks still live after draining");
    expect(final_stats.allocations_ == final_stats.deallocations_, "allocation / deallocation counts unbalanced");
    expect(final_stats.peak_live_ == peak, "instrumented peak-live disagrees with the observed high-water mark");
}

}  // namespace

// The libFuzzer entry point. Also called directly by the standalone replay main
// below when the target is built without a fuzzing engine. The name is fixed by
// the libFuzzer ABI, so it cannot follow the project's lower_case function style.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    run_program(data, size);
    return 0;
}

#ifndef PBR_MEMORY_POOL_LIBFUZZER

namespace {

// Read a whole file into a byte buffer. Returns false if it cannot be opened.
bool read_file(const char* path, std::vector<std::uint8_t>& out) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    return true;
}

}  // namespace

// Standalone replay entry point (OSS-Fuzz StandaloneFuzzTargetMain shape): each
// command-line argument is a corpus file replayed through the harness. Present
// only when the target is built without libFuzzer, so the seed corpus is a
// portable regression gate. The C-array argv parameter and its indexing are
// unavoidable per the language spec — same narrow NOLINTs the benchmark main uses.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,hicpp-avoid-c-arrays,bugprone-exception-escape)
int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::vector<std::uint8_t> buffer;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const char* const path = argv[i];
        if (!read_file(path, buffer)) {
            std::cerr << "pool_fuzz: cannot open corpus file: " << path << '\n';
            return 1;
        }
        static_cast<void>(LLVMFuzzerTestOneInput(buffer.data(), buffer.size()));
    }
    return 0;
}

#endif  // PBR_MEMORY_POOL_LIBFUZZER
