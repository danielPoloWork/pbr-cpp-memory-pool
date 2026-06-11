// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Polo

/**
 * @file pool_vs_malloc_bench.cpp
 * @brief Microbenchmark comparing `memory_pool_alloc` / `_free` against
 *        `malloc` / `free`, satisfying spec §6.3 and ROADMAP §2.9.
 *
 * Methodology fixed by ADR-0014 (`docs/adr/0014-...`); the summary below is
 * binding:
 *   - two scenarios: **bulk** (alloc N back-to-back, then free N
 *     back-to-back) and **interleaved** (alloc + immediate free, repeated N);
 *   - 1,000,000 iterations per scenario by default (overridable via
 *     `--iterations`); 10 repeats by default, the first treated as warm-up
 *     and discarded;
 *   - 64-byte block size by default (cache-line-shaped, ADR-0009 §2 clean);
 *   - per-iteration `volatile` byte write + `do_not_optimize` barrier
 *     defeat the optimiser eliding empty alloc/free pairs;
 *   - output is a parseable header + TSV table + headline ratio, ready for
 *     redirection into `docs/bench/v<X.Y.Z>-<host>.md`;
 *   - the binary deliberately performs no numeric self-assertions — the
 *     `bench-smoke` CI cell only verifies exit code 0. Committed numbers
 *     come from a controlled host disclosed in the bench report.
 */

#include <it/d4np/memorypool/memory_pool.h>
#include <it/d4np/memorypool/memory_pool.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace mem = it::d4np::memorypool;

namespace {

// ---------------------------------------------------------------------------
// Configuration carried on the stack through every scenario runner. Keeping
// the struct small + trivially copyable is intentional — every runner takes
// it by value so any future parallel runner can shard configurations across
// threads without aliasing concerns.
// ---------------------------------------------------------------------------
struct Config {
    std::size_t iterations = 1'000'000U;
    std::size_t repeats = 10U;
    std::size_t block_size = 64U;
    bool run_bulk = true;
    bool run_interleaved = true;
};

constexpr std::size_t MIN_REPEATS = 2U;
constexpr std::size_t INTERLEAVED_POOL_CAPACITY = 16U;
constexpr std::size_t WARMUP_REPEAT_INDEX = 0U;
constexpr unsigned BYTE_MASK = 0xFFU;

// ---------------------------------------------------------------------------
// do_not_optimize — portable optimisation barrier. The GCC/Clang form uses
// inline assembly with a "memory" clobber so the compiler must materialise
// the value and discard any reordering assumptions across the barrier. The
// MSVC form routes the value through a volatile sink so the optimiser cannot
// elide stores to it; the cl.exe optimiser respects volatile lvalues.
// ---------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
template <typename T>
inline void do_not_optimize(const T& value) {
    asm volatile("" : : "g"(value) : "memory");
}
#elif defined(_MSC_VER)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile const void* g_sink = nullptr;
template <typename T>
inline void do_not_optimize(const T& value) {
    g_sink = static_cast<const void*>(&value);
}
#else
template <typename T>
inline void do_not_optimize(const T& value) {
    (void)value;
}
#endif

// One-byte write through a volatile lvalue. Forces the compiler to keep the
// allocation alive and to actually fault in the page on first touch.
inline void touch_byte(void* ptr, std::size_t loop_index) {
    *static_cast<volatile unsigned char*>(ptr) =
        static_cast<unsigned char>(loop_index & BYTE_MASK);
}

// ---------------------------------------------------------------------------
// Stats over a sample of per-iteration nanosecond costs. The first repeat is
// the warm-up and is dropped before this function is called.
// ---------------------------------------------------------------------------
struct Stats {
    double min_ns;
    double median_ns;
    double mean_ns;
    double max_ns;
    double stddev_ns;
};

Stats summarise(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const auto n = samples.size();
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    const double mean = sum / static_cast<double>(n);
    double sq = 0.0;
    for (const auto v : samples) {
        const double d = v - mean;
        sq += d * d;
    }
    const double variance = sq / static_cast<double>(n);
    return Stats{samples.front(),
                 samples.at(n / 2U),
                 mean,
                 samples.back(),
                 std::sqrt(variance)};
}

// ---------------------------------------------------------------------------
// Timed runners. Each returns the elapsed nanoseconds per iteration for ONE
// repeat. The outer loop (run_repeats) takes care of warm-up + statistics.
// ---------------------------------------------------------------------------
using clock = std::chrono::steady_clock;

inline double ns_per_iter(clock::time_point t0, clock::time_point t1, std::size_t n) {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return static_cast<double>(ns) / static_cast<double>(n);
}

double time_pool_bulk_alloc(mem::Pool& pool, std::size_t n, std::vector<void*>& out) {
    out.clear();
    out.reserve(n);
    const auto t0 = clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        void* const p = pool.allocate();
        touch_byte(p, i);
        do_not_optimize(p);
        out.push_back(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, n);
}

double time_pool_bulk_free(mem::Pool& pool, std::vector<void*>& blocks) {
    const auto t0 = clock::now();
    for (void* p : blocks) {
        do_not_optimize(p);
        pool.deallocate(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, blocks.size());
}

double time_malloc_bulk_alloc(std::size_t n, std::size_t block_size, std::vector<void*>& out) {
    out.clear();
    out.reserve(n);
    const auto t0 = clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        void* const p = std::malloc(block_size);
        touch_byte(p, i);
        do_not_optimize(p);
        out.push_back(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, n);
}

double time_malloc_bulk_free(std::vector<void*>& blocks) {
    const auto t0 = clock::now();
    for (void* p : blocks) {
        do_not_optimize(p);
        std::free(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, blocks.size());
}

double time_pool_interleaved(mem::Pool& pool, std::size_t n) {
    const auto t0 = clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        void* const p = pool.allocate();
        touch_byte(p, i);
        do_not_optimize(p);
        pool.deallocate(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, n);
}

double time_malloc_interleaved(std::size_t n, std::size_t block_size) {
    const auto t0 = clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        void* const p = std::malloc(block_size);
        touch_byte(p, i);
        do_not_optimize(p);
        std::free(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, n);
}

// ---------------------------------------------------------------------------
// Per-repeat orchestration: build a fresh pool, run alloc + free, collect.
// The first repeat (index 0) is the warm-up and is dropped before statistics
// are computed.
// ---------------------------------------------------------------------------
struct BulkMeasurement {
    std::vector<double> alloc_samples;
    std::vector<double> free_samples;
};

BulkMeasurement run_pool_bulk(const Config& cfg) {
    BulkMeasurement out;
    out.alloc_samples.reserve(cfg.repeats);
    out.free_samples.reserve(cfg.repeats);
    std::vector<void*> slots;
    for (std::size_t r = 0; r < cfg.repeats; ++r) {
        std::optional<mem::Pool> opt = mem::Pool::make(cfg.block_size, cfg.iterations);
        if (!opt.has_value()) {
            std::cerr << "pool-bulk: Pool::make failed (block_size="
                      << cfg.block_size << " iterations=" << cfg.iterations << ")\n";
            std::exit(EXIT_FAILURE);
        }
        mem::Pool& pool = *opt;
        if (r != WARMUP_REPEAT_INDEX) {
            out.alloc_samples.push_back(time_pool_bulk_alloc(pool, cfg.iterations, slots));
            out.free_samples.push_back(time_pool_bulk_free(pool, slots));
        } else {
            (void)time_pool_bulk_alloc(pool, cfg.iterations, slots);
            (void)time_pool_bulk_free(pool, slots);
        }
    }
    return out;
}

BulkMeasurement run_malloc_bulk(const Config& cfg) {
    BulkMeasurement out;
    out.alloc_samples.reserve(cfg.repeats);
    out.free_samples.reserve(cfg.repeats);
    std::vector<void*> slots;
    for (std::size_t r = 0; r < cfg.repeats; ++r) {
        if (r != WARMUP_REPEAT_INDEX) {
            out.alloc_samples.push_back(time_malloc_bulk_alloc(cfg.iterations, cfg.block_size, slots));
            out.free_samples.push_back(time_malloc_bulk_free(slots));
        } else {
            (void)time_malloc_bulk_alloc(cfg.iterations, cfg.block_size, slots);
            (void)time_malloc_bulk_free(slots);
        }
    }
    return out;
}

std::vector<double> run_pool_interleaved(const Config& cfg) {
    std::vector<double> samples;
    samples.reserve(cfg.repeats);
    std::optional<mem::Pool> opt = mem::Pool::make(cfg.block_size, INTERLEAVED_POOL_CAPACITY);
    if (!opt.has_value()) {
        std::cerr << "pool-interleaved: Pool::make failed (block_size="
                  << cfg.block_size << ")\n";
        std::exit(EXIT_FAILURE);
    }
    mem::Pool& pool = *opt;
    for (std::size_t r = 0; r < cfg.repeats; ++r) {
        const double s = time_pool_interleaved(pool, cfg.iterations);
        if (r != WARMUP_REPEAT_INDEX) {
            samples.push_back(s);
        }
    }
    return samples;
}

std::vector<double> run_malloc_interleaved(const Config& cfg) {
    std::vector<double> samples;
    samples.reserve(cfg.repeats);
    for (std::size_t r = 0; r < cfg.repeats; ++r) {
        const double s = time_malloc_interleaved(cfg.iterations, cfg.block_size);
        if (r != WARMUP_REPEAT_INDEX) {
            samples.push_back(s);
        }
    }
    return samples;
}

// ---------------------------------------------------------------------------
// CLI parsing. Long-form flags only — keeps the surface tiny and the help
// readable. Unrecognised flags terminate with a usage hint rather than
// silently being ignored.
// ---------------------------------------------------------------------------
[[noreturn]] void die_with_usage(std::string_view argv0, std::string_view msg) {
    std::cerr << argv0 << ": " << msg << "\n"
              << "usage: " << argv0
              << " [--iterations N] [--repeats N] [--block-size N]"
              << " [--scenario {bulk|interleaved|both}]\n";
    std::exit(EXIT_FAILURE);
}

std::size_t parse_size(std::string_view argv0, std::string_view flag, std::string_view value) {
    try {
        const auto n = std::stoull(std::string{value});
        return static_cast<std::size_t>(n);
    } catch (const std::exception&) {
        die_with_usage(argv0, std::string{flag} + " expects a positive integer, got '" +
                                   std::string{value} + "'");
    }
}

Config parse_args(int argc, char* argv[]) {
    Config cfg;
    const std::string_view argv0{argv[0]};
    for (int i = 1; i < argc; ++i) {
        const std::string_view a{argv[i]};
        if (a == "--iterations" || a == "--repeats" || a == "--block-size" || a == "--scenario") {
            if (i + 1 >= argc) {
                die_with_usage(argv0, std::string{a} + " requires a value");
            }
            const std::string_view v{argv[i + 1]};
            if (a == "--iterations") {
                cfg.iterations = parse_size(argv0, a, v);
            } else if (a == "--repeats") {
                cfg.repeats = parse_size(argv0, a, v);
            } else if (a == "--block-size") {
                cfg.block_size = parse_size(argv0, a, v);
            } else {
                if (v == "bulk") {
                    cfg.run_interleaved = false;
                } else if (v == "interleaved") {
                    cfg.run_bulk = false;
                } else if (v != "both") {
                    die_with_usage(argv0, "--scenario must be bulk | interleaved | both");
                }
            }
            ++i;
        } else if (a == "-h" || a == "--help") {
            die_with_usage(argv0, "help requested");
        } else {
            die_with_usage(argv0, "unknown flag: " + std::string{a});
        }
    }
    if (cfg.iterations == 0U) {
        die_with_usage(argv0, "--iterations must be > 0");
    }
    if (cfg.repeats < MIN_REPEATS) {
        die_with_usage(argv0, "--repeats must be >= 2 (one warm-up + at least one measured)");
    }
    return cfg;
}

// ---------------------------------------------------------------------------
// Output: header block, TSV body, headline summary. Everything goes to
// stdout so the file can be redirected verbatim into the bench report.
// ---------------------------------------------------------------------------
std::string_view compiler_name() {
#if defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#elif defined(_MSC_VER)
    return "msvc";
#else
    return "unknown";
#endif
}

std::string compiler_version() {
#if defined(__clang__)
    return std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__) + "." +
           std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
    return std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." +
           std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    return std::to_string(_MSC_FULL_VER);
#else
    return "unknown";
#endif
}

void print_header(std::ostream& os, const Config& cfg) {
    os << "# pool-vs-malloc benchmark (M2.9 / spec §6.3)\n";
    os << "# methodology: ADR-0014\n";
    os << "# compiler: " << compiler_name() << " " << compiler_version() << "\n";
    os << "# hardware_concurrency: " << std::thread::hardware_concurrency() << "\n";
    os << "# max_align_t: " << alignof(std::max_align_t) << " bytes\n";
    os << "# config: iterations=" << cfg.iterations
       << " repeats=" << cfg.repeats
       << " block_size=" << cfg.block_size << "\n";
    os << "# (the human-runner appends host / cpu / os details when committing the report)\n";
    os << "\n";
}

void print_row(std::ostream& os, std::string_view scenario, std::string_view alloc,
               std::string_view region, const Stats& s) {
    os << scenario << "\t" << alloc << "\t" << region << "\t"
       << s.min_ns << "\t" << s.median_ns << "\t" << s.mean_ns << "\t"
       << s.max_ns << "\t" << s.stddev_ns << "\n";
}

void print_table_header(std::ostream& os) {
    os << "scenario\tallocator\tregion\tmin_ns/op\tmedian_ns/op\tmean_ns/op"
          "\tmax_ns/op\tstddev_ns/op\n";
}

void print_headline(std::ostream& os, std::string_view scenario,
                    double malloc_median, double pool_median) {
    if (pool_median <= 0.0) {
        os << "# headline: " << scenario << ": pool median is 0 — measurement invalid\n";
        return;
    }
    os << "# headline: " << scenario
       << ": malloc / pool = " << (malloc_median / pool_median) << "x\n";
}

// Inlining the headline emission into the same scope as the run avoids the
// std::optional<Stats> pattern that bugprone-unchecked-optional-access does
// not recognise as flow-guarded through assignment + later dereference.
void run_and_report_bulk(const Config& cfg, std::ostream& body, std::ostream& tail) {
    const auto pm = run_pool_bulk(cfg);
    const auto mm = run_malloc_bulk(cfg);
    const Stats pa = summarise(pm.alloc_samples);
    const Stats pf = summarise(pm.free_samples);
    const Stats ma = summarise(mm.alloc_samples);
    const Stats mf = summarise(mm.free_samples);
    print_row(body, "bulk", "pool", "alloc", pa);
    print_row(body, "bulk", "pool", "free", pf);
    print_row(body, "bulk", "malloc", "alloc", ma);
    print_row(body, "bulk", "malloc", "free", mf);
    print_headline(tail, "bulk-alloc", ma.median_ns, pa.median_ns);
    print_headline(tail, "bulk-free", mf.median_ns, pf.median_ns);
}

void run_and_report_interleaved(const Config& cfg, std::ostream& body, std::ostream& tail) {
    const Stats ip = summarise(run_pool_interleaved(cfg));
    const Stats im = summarise(run_malloc_interleaved(cfg));
    print_row(body, "interleaved", "pool", "alloc+free", ip);
    print_row(body, "interleaved", "malloc", "alloc+free", im);
    print_headline(tail, "interleaved", im.median_ns, ip.median_ns);
}

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    const Config cfg = parse_args(argc, argv);
    // Three-decimal fixed precision so the TSV columns line up across runs;
    // the same precision is used for the headline ratios. The committed
    // bench report keeps the raw output verbatim, so consistency here
    // directly drives readability in docs/bench/.
    std::cout << std::fixed << std::setprecision(3);
    print_header(std::cout, cfg);
    print_table_header(std::cout);

    // Buffer the headline summary so the body table prints first and the
    // tail block (blank line + headlines) prints last, regardless of which
    // scenarios run.
    std::ostringstream tail;
    tail << std::fixed << std::setprecision(3);
    if (cfg.run_bulk) {
        run_and_report_bulk(cfg, std::cout, tail);
    }
    if (cfg.run_interleaved) {
        run_and_report_interleaved(cfg, std::cout, tail);
    }
    std::cout << "\n" << tail.str();
    return 0;
}
