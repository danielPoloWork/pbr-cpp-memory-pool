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
 *
 * The deliberate `std::malloc` / `std::free` calls in this TU are the WHOLE
 * POINT of the comparison and carry narrow NOLINTs against the
 * `cppcoreguidelines-no-malloc` and `cppcoreguidelines-owning-memory`
 * checks. The argv array carries a NOLINT against the C-arrays checks
 * because the standard main signature is non-negotiable.
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
// Configuration carried on the stack through every scenario runner. Member
// suffix follows the project's MemberSuffix: '_' identifier-naming rule
// (.clang-tidy). Keeping the struct small + trivially copyable is intentional
// — every runner takes it by const-ref.
// ---------------------------------------------------------------------------
struct Config {
    std::size_t iterations_ = 1'000'000U;
    std::size_t repeats_ = 10U;
    std::size_t block_size_ = 64U;
    bool run_bulk_ = true;
    bool run_interleaved_ = true;
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
// elide stores to it.
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
    *static_cast<volatile unsigned char*>(ptr) = static_cast<unsigned char>(loop_index & BYTE_MASK);
}

// ---------------------------------------------------------------------------
// Stats over a sample of per-iteration nanosecond costs. The first repeat is
// the warm-up and is dropped before this function is called.
// ---------------------------------------------------------------------------
struct Stats {
    double min_ns_;
    double median_ns_;
    double mean_ns_;
    double max_ns_;
    double stddev_ns_;
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
    const double stddev = std::sqrt(variance);
    return Stats{samples.front(), samples.at(n / 2U), mean, samples.back(), stddev};
}

// ---------------------------------------------------------------------------
// Timed runners. Each returns the elapsed nanoseconds per iteration for ONE
// repeat. The outer loop (run_* runners below) takes care of warm-up + stats.
// All malloc helpers take const Config& so the bugprone-easily-swappable
// check does not see two adjacent std::size_t parameters.
// ---------------------------------------------------------------------------
using clock = std::chrono::steady_clock;

inline double ns_per_iter(clock::time_point t0, clock::time_point t1, std::size_t n) {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return static_cast<double>(ns) / static_cast<double>(n);
}

double time_pool_bulk_alloc(mem::Pool& pool, const Config& cfg, std::vector<void*>& out) {
    out.clear();
    out.reserve(cfg.iterations_);
    const auto t0 = clock::now();
    for (std::size_t i = 0; i < cfg.iterations_; ++i) {
        // try_allocate is the apples-to-apples verb vs std::malloc — both
        // report failure in-band with NULL (ADR-0016 §4). It is also the
        // exact code path that produced the committed v0.2.0 numbers.
        void* const p = pool.try_allocate();
        touch_byte(p, i);
        do_not_optimize(p);
        out.push_back(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, cfg.iterations_);
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

double time_malloc_bulk_alloc(const Config& cfg, std::vector<void*>& out) {
    out.clear();
    out.reserve(cfg.iterations_);
    const auto t0 = clock::now();
    for (std::size_t i = 0; i < cfg.iterations_; ++i) {
        // The comparison against std::malloc is the entire purpose of this
        // benchmark; the cppcoreguidelines-no-malloc / -owning-memory checks
        // are explicitly suppressed at the call sites that exist for that
        // comparison.
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
        void* const p = std::malloc(cfg.block_size_);
        touch_byte(p, i);
        do_not_optimize(p);
        out.push_back(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, cfg.iterations_);
}

double time_malloc_bulk_free(std::vector<void*>& blocks) {
    const auto t0 = clock::now();
    for (void* p : blocks) {
        do_not_optimize(p);
        // Paired with the suppressed std::malloc above.
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
        std::free(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, blocks.size());
}

double time_pool_interleaved(mem::Pool& pool, const Config& cfg) {
    const auto t0 = clock::now();
    for (std::size_t i = 0; i < cfg.iterations_; ++i) {
        // Same in-band-failure verb as the bulk scenario (ADR-0016 §4).
        void* const p = pool.try_allocate();
        touch_byte(p, i);
        do_not_optimize(p);
        pool.deallocate(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, cfg.iterations_);
}

double time_malloc_interleaved(const Config& cfg) {
    const auto t0 = clock::now();
    for (std::size_t i = 0; i < cfg.iterations_; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
        void* const p = std::malloc(cfg.block_size_);
        touch_byte(p, i);
        do_not_optimize(p);
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
        std::free(p);
    }
    const auto t1 = clock::now();
    return ns_per_iter(t0, t1, cfg.iterations_);
}

// ---------------------------------------------------------------------------
// Per-repeat orchestration: build a fresh pool, run alloc + free, collect.
// The first repeat (index 0) is the warm-up and is dropped before statistics.
// ---------------------------------------------------------------------------
struct BulkMeasurement {
    std::vector<double> alloc_samples_;
    std::vector<double> free_samples_;
};

BulkMeasurement run_pool_bulk(const Config& cfg) {
    BulkMeasurement out;
    out.alloc_samples_.reserve(cfg.repeats_);
    out.free_samples_.reserve(cfg.repeats_);
    std::vector<void*> slots;
    for (std::size_t r = 0; r < cfg.repeats_; ++r) {
        std::optional<mem::Pool> opt = mem::Pool::make(cfg.block_size_, cfg.iterations_);
        if (!opt.has_value()) {
            std::cerr << "pool-bulk: Pool::make failed\n";
            std::exit(EXIT_FAILURE);
        }
        mem::Pool& pool = *opt;
        if (r != WARMUP_REPEAT_INDEX) {
            out.alloc_samples_.push_back(time_pool_bulk_alloc(pool, cfg, slots));
            out.free_samples_.push_back(time_pool_bulk_free(pool, slots));
        } else {
            (void)time_pool_bulk_alloc(pool, cfg, slots);
            (void)time_pool_bulk_free(pool, slots);
        }
    }
    return out;
}

BulkMeasurement run_malloc_bulk(const Config& cfg) {
    BulkMeasurement out;
    out.alloc_samples_.reserve(cfg.repeats_);
    out.free_samples_.reserve(cfg.repeats_);
    std::vector<void*> slots;
    for (std::size_t r = 0; r < cfg.repeats_; ++r) {
        if (r != WARMUP_REPEAT_INDEX) {
            out.alloc_samples_.push_back(time_malloc_bulk_alloc(cfg, slots));
            out.free_samples_.push_back(time_malloc_bulk_free(slots));
        } else {
            (void)time_malloc_bulk_alloc(cfg, slots);
            (void)time_malloc_bulk_free(slots);
        }
    }
    return out;
}

std::vector<double> run_pool_interleaved(const Config& cfg) {
    std::vector<double> samples;
    samples.reserve(cfg.repeats_);
    std::optional<mem::Pool> opt = mem::Pool::make(cfg.block_size_, INTERLEAVED_POOL_CAPACITY);
    if (!opt.has_value()) {
        std::cerr << "pool-interleaved: Pool::make failed\n";
        std::exit(EXIT_FAILURE);
    }
    mem::Pool& pool = *opt;
    for (std::size_t r = 0; r < cfg.repeats_; ++r) {
        const double s = time_pool_interleaved(pool, cfg);
        if (r != WARMUP_REPEAT_INDEX) {
            samples.push_back(s);
        }
    }
    return samples;
}

std::vector<double> run_malloc_interleaved(const Config& cfg) {
    std::vector<double> samples;
    samples.reserve(cfg.repeats_);
    for (std::size_t r = 0; r < cfg.repeats_; ++r) {
        const double s = time_malloc_interleaved(cfg);
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
//
// argv0 + flag travel together to every diagnostic site so they are folded
// into a single `ParseLoc` struct rather than passed as two adjacent
// string_view parameters (which would trip bugprone-easily-swappable).
// ---------------------------------------------------------------------------
struct ParseLoc {
    std::string_view argv0_;
    std::string_view flag_;
};

[[noreturn]] void die_with_usage(std::string_view argv0, std::string_view msg) {
    std::cerr << argv0 << ": " << msg << "\n";
    std::cerr << "usage: " << argv0 << " [--iterations N] [--repeats N]";
    std::cerr << " [--block-size N] [--scenario {bulk|interleaved|both}]\n";
    std::exit(EXIT_FAILURE);
}

std::size_t parse_size(ParseLoc loc, std::string_view value) {
    try {
        const auto n = std::stoull(std::string{value});
        return static_cast<std::size_t>(n);
    } catch (const std::exception&) {
        std::ostringstream oss;
        oss << loc.flag_ << " expects a positive integer, got '" << value << "'";
        die_with_usage(loc.argv0_, oss.str());
    }
}

// The standard C++ main signature is `int main(int argc, char* argv[])`; the
// argv parameter is unavoidable as a C-style array per the language spec.
// parse_args mirrors that signature, so the same NOLINT applies here.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,hicpp-avoid-c-arrays)
Config parse_args(int argc, char* argv[]) {
    Config cfg;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const std::string_view argv0{argv[0]};
    for (int i = 1; i < argc; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const std::string_view a{argv[i]};
        if (a == "--iterations" || a == "--repeats" || a == "--block-size" || a == "--scenario") {
            if (i + 1 >= argc) {
                die_with_usage(argv0, std::string{a} + " requires a value");
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const std::string_view v{argv[i + 1]};
            const ParseLoc loc{argv0, a};
            if (a == "--iterations") {
                cfg.iterations_ = parse_size(loc, v);
            } else if (a == "--repeats") {
                cfg.repeats_ = parse_size(loc, v);
            } else if (a == "--block-size") {
                cfg.block_size_ = parse_size(loc, v);
            } else {
                if (v == "bulk") {
                    cfg.run_interleaved_ = false;
                } else if (v == "interleaved") {
                    cfg.run_bulk_ = false;
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
    if (cfg.iterations_ == 0U) {
        die_with_usage(argv0, "--iterations must be > 0");
    }
    if (cfg.repeats_ < MIN_REPEATS) {
        die_with_usage(argv0, "--repeats must be >= 2 (one warm-up + at least one measured)");
    }
    return cfg;
}

// ---------------------------------------------------------------------------
// Output: header block, TSV body, headline summary. Everything goes through
// std::ostream so the helpers can target std::cout for the body and a local
// std::ostringstream for the headline tail.
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
    std::ostringstream s;
#if defined(__clang__)
    s << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
#elif defined(__GNUC__)
    s << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
#elif defined(_MSC_VER)
    s << _MSC_FULL_VER;
#else
    s << "unknown";
#endif
    return s.str();
}

void print_header(std::ostream& os, const Config& cfg) {
    os << "# pool-vs-malloc benchmark (M2.9 / spec §6.3)\n";
    os << "# methodology: ADR-0014\n";
    os << "# compiler: " << compiler_name() << " " << compiler_version() << "\n";
    os << "# hardware_concurrency: " << std::thread::hardware_concurrency() << "\n";
    os << "# max_align_t: " << alignof(std::max_align_t) << " bytes\n";
    os << "# config: iterations=" << cfg.iterations_;
    os << " repeats=" << cfg.repeats_;
    os << " block_size=" << cfg.block_size_ << "\n";
    os << "# (the human-runner appends host / cpu / os details when committing the report)\n";
    os << "\n";
}

// Row labels for the TSV body. The struct folds the three label string_views
// into a single parameter, sidestepping bugprone-easily-swappable that would
// fire on three adjacent string_view params, and keeps the print_row
// signature short enough for clang-format to leave on one line.
struct RowKey {
    std::string_view scenario_;
    std::string_view allocator_;
    std::string_view region_;
};

void print_row(std::ostream& os, RowKey key, const Stats& s) {
    os << key.scenario_ << "\t" << key.allocator_ << "\t" << key.region_ << "\t";
    os << s.min_ns_ << "\t" << s.median_ns_ << "\t" << s.mean_ns_ << "\t";
    os << s.max_ns_ << "\t" << s.stddev_ns_ << "\n";
}

void print_table_header(std::ostream& os) {
    os << "scenario\tallocator\tregion\tmin_ns/op\tmedian_ns/op\tmean_ns/op";
    os << "\tmax_ns/op\tstddev_ns/op\n";
}

// `ratio` is malloc_median / pool_median, precomputed by the caller to a
// single double — avoids two adjacent same-typed parameters and gives the
// caller one canonical place to handle the pool_median == 0 invalid case
// (ratio == 0.0 sentinel).
void print_headline(std::ostream& os, std::string_view scenario, double ratio) {
    if (ratio == 0.0) {
        os << "# headline: " << scenario << ": pool median is 0 — measurement invalid\n";
        return;
    }
    os << "# headline: " << scenario << ": malloc / pool = " << ratio << "x\n";
}

inline double safe_ratio(double numerator, double denominator) {
    return denominator <= 0.0 ? 0.0 : numerator / denominator;
}

// Returns the headline-summary string that the caller appends to the tail of
// the report. Avoids passing both `body` and `tail` ostreams as adjacent
// parameters (which would trip bugprone-easily-swappable-parameters).
std::string run_and_report_bulk(const Config& cfg, std::ostream& body) {
    const auto pm = run_pool_bulk(cfg);
    const auto mm = run_malloc_bulk(cfg);
    const Stats pa = summarise(pm.alloc_samples_);
    const Stats pf = summarise(pm.free_samples_);
    const Stats ma = summarise(mm.alloc_samples_);
    const Stats mf = summarise(mm.free_samples_);
    print_row(body, RowKey{"bulk", "pool", "alloc"}, pa);
    print_row(body, RowKey{"bulk", "pool", "free"}, pf);
    print_row(body, RowKey{"bulk", "malloc", "alloc"}, ma);
    print_row(body, RowKey{"bulk", "malloc", "free"}, mf);
    std::ostringstream tail;
    tail << std::fixed << std::setprecision(3);
    print_headline(tail, "bulk-alloc", safe_ratio(ma.median_ns_, pa.median_ns_));
    print_headline(tail, "bulk-free", safe_ratio(mf.median_ns_, pf.median_ns_));
    return tail.str();
}

std::string run_and_report_interleaved(const Config& cfg, std::ostream& body) {
    const Stats ip = summarise(run_pool_interleaved(cfg));
    const Stats im = summarise(run_malloc_interleaved(cfg));
    print_row(body, RowKey{"interleaved", "pool", "alloc+free"}, ip);
    print_row(body, RowKey{"interleaved", "malloc", "alloc+free"}, im);
    std::ostringstream tail;
    tail << std::fixed << std::setprecision(3);
    print_headline(tail, "interleaved", safe_ratio(im.median_ns_, ip.median_ns_));
    return tail.str();
}

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,hicpp-avoid-c-arrays)
int main(int argc, char* argv[]) {
    const Config cfg = parse_args(argc, argv);
    // Three-decimal fixed precision so the TSV columns line up across runs;
    // the same precision is used for the headline ratios. The committed
    // bench report keeps the raw output verbatim, so consistency here
    // directly drives readability in docs/bench/.
    std::cout << std::fixed << std::setprecision(3);
    print_header(std::cout, cfg);
    print_table_header(std::cout);

    std::string tail;
    if (cfg.run_bulk_) {
        tail += run_and_report_bulk(cfg, std::cout);
    }
    if (cfg.run_interleaved_) {
        tail += run_and_report_interleaved(cfg, std::cout);
    }
    std::cout << "\n" << tail;
    return 0;
}
