# 高性能内存池管理器（C++）

[![ci](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/ci.yml)
[![docs](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml/badge.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml)
[![docs-site](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs-site.yml/badge.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs-site.yml)
[![API reference](https://img.shields.io/badge/API%20reference-Doxygen-1f6feb.svg)](https://danielpolowork.github.io/pbr-cpp-memory-pool/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../../../LICENSE)
[![Standard: C++17 / ANSI C](https://img.shields.io/badge/Standard-C%2B%2B17%20%2F%20ANSI%20C-blue.svg)](docs/specs/01_spec_cpp_memory_pool.md)
[![Status: v1.1.0 stable](https://img.shields.io/badge/Status-v1.1.0%20stable-brightgreen.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v1.1.0)

> 🌐 本页面是项目 [`README.md`](../../../README.md) 的简体中文翻译（基于提交 `90c6fb3`）。
> **英文版本为唯一权威来源** —— 如本译文与原文不一致，以英文版为准。
> 用其他语言阅读：[English](../../../README.md) · [日本語](../ja/README.md)。
> 本目录其他简体中文页面：[规范](docs/specs/01_spec_cpp_memory_pool.md) · [设计模式目录（概览）](docs/patterns/README.md)。未翻译的页面回退到英文原文。
>
> 隶属于 **Purpose-Built References (PBR)** 系列 —— 一组小而精、具教学性、达生产质量的 C/C++ 高性能构建块参考实现。

许多高性能系统 —— 图形引擎、金融交易服务器、数据库 —— 都受到内存碎片化，以及频繁调用 `malloc`/`free`（或 `new`/`delete`）开销的困扰。本组件提供一个**自定义内存池（Memory Pool）**，它预先分配一块连续的内存，并提供**常数时间、固定大小、零外部碎片的分配**。它是一个单一、聚焦的 C/C++ 库 —— 一个由头文件支撑的静态库，带有四函数的 C ABI 和一个地道的 C++17 包装器 —— 以企业级质量构建（警告即错误、消毒器、Valgrind、Doxygen），并在其 [ADR](../../../docs/adr/) 中逐项决策地记录。

## 概览

- **分配：** O(1)，固定块大小，连续的后备存储。
- **空闲链表策略：** 隐式 —— 空闲块在其自身前 `sizeof(void*)` 个字节中存储指向下一个空闲块的指针，因此存活块零元数据开销。
- **元数据开销：** 每块 0 字节（空闲链表链接复用未使用的块存储）+ 每池固定约 40 字节 —— 与 `block_count` 无关。由 CI 限制在 ≤ 128 字节（见 [ADR-0015](../../../docs/adr/0015-metadata-overhead-budget-and-introspection.md)）。
- **标准：** ANSI C 公共接口，C++17 内部实现与包装器。无外部依赖。
- **线程安全：** 可选，编译期配置（里程碑 4）。
- **动态增长：** 可选的连续溢出 chunk（里程碑 5）。
- **可观测性：** 可选的 `InstrumentedPool` Decorator（统计、占用）+ 用于生命周期事件的 `PoolObserver` —— 对未装饰的池零开销（里程碑 6）。
- **质量门禁：** `clang-tidy` 干净，ASan + UBSan +（线程化落地后）TSan 通过，Valgrind 干净，公共接口有 Doxygen 文档。
- **基准目标：** 在 1,000,000 次迭代上与 `malloc`/`free` 对比测量。

## 公共 C API

完整的公共接口是四个函数。完整契约见[规范](docs/specs/01_spec_cpp_memory_pool.md)：

```c
typedef struct memory_pool memory_pool_t;

memory_pool_t* memory_pool_create(size_t block_size, size_t block_count);
void*          memory_pool_alloc(memory_pool_t* pool);
void           memory_pool_free(memory_pool_t* pool, void* block);
void           memory_pool_destroy(memory_pool_t* pool);
```

一个 C++17 RAII 包装器（`it::d4np::memorypool::Pool`）和一个类型化模板（`TypedPool<T>`）在此接口之上分层 —— 见 [`ROADMAP.md`](../../../ROADMAP.md) 中的里程碑 2.5 与 3.2。

完整的、相互链接的 **API 参考** —— 每个公共符号的参数 / 返回值 / 抛出契约 —— 由头文件内的 Doxygen 注释生成，并在每次推送到 `master` 时发布到 [GitHub Pages](https://danielpolowork.github.io/pbr-cpp-memory-pool/)（[ADR-0027](../../../docs/adr/0027-doxygen-html-site-and-publication-pipeline.md)）。同一构建在每个 PR 上作为“警告即错误”门禁运行。

## 用法

公共包含根目录是 `src/main/cpp`，因此头文件以 `<it/d4np/memorypool/…>` 形式包含；链接静态库目标 `pbr_memory_pool`（见下文「构建与测试」一节）。下面每个片段都在每次发布前由维护者原样编译并运行。C++ 接口位于命名空间 `it::d4np::memorypool`（下文为简洁省略）。

### C API —— 四函数核心

```c
#include <it/d4np/memorypool/memory_pool.h>

memory_pool_t* pool = memory_pool_create(64, 1024);  /* 1024 个 64 字节的块 */
if (pool != NULL) {
    void* block = memory_pool_alloc(pool);           /* O(1)；耗尽时返回 NULL */
    if (block != NULL) {
        memory_pool_free(pool, block);               /* O(1)；归还空闲链表 */
    }
    memory_pool_destroy(pool);                       /* 释放全部后备存储 */
}
```

### C++ RAII 包装器 —— `Pool`

`Pool` 拥有句柄（构造即创建，析构即销毁），且仅可移动。异常策略为双动词式（[ADR-0016](../../../docs/adr/0016-exception-policy-at-the-c-cpp-boundary.md)）：`allocate()` 在耗尽时抛出 `std::bad_alloc`，`try_allocate()` 为 `noexcept` 并返回 `nullptr`。

```cpp
#include <it/d4np/memorypool/memory_pool.hpp>
using namespace it::d4np::memorypool;

Pool pool(64, 1024);                       // 配置非法时抛出 std::bad_alloc
if (void* block = pool.try_allocate()) {   // 不抛异常的动词
    pool.deallocate(block);
}

// 以返回值（而非异常）表示失败 —— Factory Method / Builder（ADR-0011）：
if (std::optional<Pool> p = Pool::make(64, 1024)) {
    void* b = p->allocate();               // 抛异常的动词 —— 绝不返回 nullptr
    p->deallocate(b);
}

auto built = PoolBuilder{}.with_block_size(64).with_block_count(1024).build();
```

### 类型安全的池 —— `TypedPool<T>`

在编译期从 `T` 推导出符合规范的 `block_size`，并添加对象生命周期动词（`construct` 执行 placement-new，`destroy` 运行析构函数）。

```cpp
#include <it/d4np/memorypool/typed_pool.hpp>

TypedPool<Widget> pool(1024);
Widget* w = pool.construct(arg1, arg2);    // 分配 + placement-new（强保证）
// ... 使用 w ...
pool.destroy(w);                           // ~Widget() + 将槽位归还给池
```

### STL 容器 —— `PoolAllocator<T>`

一个 *Cpp17Allocator* Adapter（[ADR-0018](../../../docs/adr/0018-stl-allocator-adapter.md)）。基于节点的容器（`std::list`、`std::map`、`std::set`）走 O(1) 的池快速路径；超大 / 多元素请求透明回退到 `::operator new`。池必须比容器存活更久。

```cpp
#include <it/d4np/memorypool/pool_allocator.hpp>
#include <list>

Pool pool(64, 1024);                       // 64 字节的块可容纳一个 list<int> 节点
std::list<int, PoolAllocator<int>> values{PoolAllocator<int>{pool}};
for (int i = 0; i < 100; ++i) {
    values.push_back(i);                   // 每个节点都由池提供
}
```

### 动态增长

可选、按池启用（[ADR-0022](../../../docs/adr/0022-dynamic-growth-policy-and-chunk-linking.md)）：耗尽时池获取一块新的连续 chunk 并按几何方式增长，而非失败。默认的池保持固定大小。

```cpp
if (std::optional<Pool> pool = Pool::make_dynamic(64, 256, /*growth_factor=*/2)) {
    for (int i = 0; i < 100000; ++i) {
        (void)pool->try_allocate();        // 按 256 → 512 → … 增长，而非返回 nullptr
    }
}
```

### 可观测性 —— `InstrumentedPool` + `PoolObserver`

一个可选的 Decorator（[ADR-0025](../../../docs/adr/0025-decorator-for-instrumented-pool.md)），统计分配活动并发出生命周期事件（[ADR-0026](../../../docs/adr/0026-observer-for-pool-lifecycle-events.md)）。直接使用 `Pool` 的程序不付出任何代价。

```cpp
#include <it/d4np/memorypool/instrumented_pool.hpp>

struct LoggingObserver : PoolObserver {
    void on_pool_event(PoolEvent event, const PoolStats& stats) noexcept override {
        if (event == PoolEvent::exhausted) {
            std::cerr << "pool exhausted at " << stats.live_ << " live blocks\n";
        }
    }
};

if (auto pool = InstrumentedPool::make(64, 1024)) {
    LoggingObserver observer;
    pool->add_observer(observer);          // 观察者必须比池存活更久
    void* b = pool->try_allocate();
    pool->deallocate(b);
    PoolStats s = pool->stats();           // allocations_、deallocations_、live_、peak_live_……
    pool->write_summary(std::cout);
}
```

## 架构

池使用一个**内嵌于空闲块自身的空闲链表**来管理空闲内存：当一个块空闲时，它的前 `sizeof(void*)` 个字节保存下一个空闲块的地址。存活块完全不带任何元数据。

```text
+-------------------------------------------------------------------+
|                        内存池 (Memory Pool)                        |
+-------------------------------------------------------------------+
| [块 1（空闲）]   -> 指向「块 2」的 next-free 指针
| [块 2（使用中）] -> 用户数据
| [块 3（空闲）]   -> 指向「块 4」的 next-free 指针
| [块 4（空闲）]   -> NULL（空闲链表结束）
+-------------------------------------------------------------------+
```

空闲链表布局、`block_size ≥ sizeof(void*)` 约束以及对齐保证锁定于 [ADR-0009](../../../docs/adr/0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md)；周边决策记录于 [ADR-0002](../../../docs/adr/0002-adopt-cross-language-source-layout.md) 和 [ADR-0003](../../../docs/adr/0003-design-patterns-policy.md)。

## 性能

**摘要。** 相对 `malloc`/`free`，预先按需大小分配的固定池在单线程下快 **4–11×**；*正在增长*的池即使在获取 chunk 期间也保持快 **约 2×**；在线程争用下 lock-free 策略胜过互斥锁，但单一共享头部的池无法超越 `malloc` 的每线程 arena 扩展性。下面所有数字来自维护者的 **Intel i5-6600K (Skylake) × Windows 10 × MSVC 19.51 Release** 工作站，64 字节块，1,000,000 次迭代 × 10 轮（首轮作为预热丢弃）。方法学契约：[ADR-0014](../../../docs/adr/0014-microbenchmark-methodology-pool-vs-malloc.md)。

**固定、单线程**（M2.9 / 规范 §6.3 —— 完整报告：[`docs/bench/v0.2.0-windows-msvc-x64.md`](../../../docs/bench/v0.2.0-windows-msvc-x64.md)）：

| 场景         | `malloc` 中位数 (ns/op) | `pool` 中位数 (ns/op) | `malloc` / `pool` |
|--------------|------------------------:|----------------------:|------------------:|
| bulk-alloc   | 75.5                    | 6.9                   | **11.02 ×**       |
| bulk-free    | 44.5                    | 8.3                   | **5.35 ×**        |
| interleaved  | 49.9                    | 11.2                  | **4.45 ×**        |

**动态增长**（M5.4 —— 完整报告：[`docs/bench/v0.5.0-windows-msvc-x64-growth.md`](../../../docs/bench/v0.5.0-windows-msvc-x64-growth.md)）：一个在运行中从 256 块增长到 1,000,000 块的池，其摊销批量分配为 **55 ns/op，而 `malloc` 为 108 —— 快 1.96×**，代价是约 12 次几何 chunk 获取。已知工作集大小时请预先按需大小分配（约 11× 的路径）；未知时使用增长模式。

**线程化**（M4.5 —— 完整报告：[`docs/bench/v0.4.0-windows-msvc-x64-threading.md`](../../../docs/bench/v0.4.0-windows-msvc-x64-threading.md)）：`--scenario concurrent` 用 `T` 个线程对一个共享池操作，按线程安全策略构建。单线程快速路径得以保留（`NONE` 交错 ≈ 9 ns/op，不变）；在 4 线程争用下 `LOCKFREE`（41.8 ns/op）胜过 `MUTEX`（69.5 ns/op）。

基准二进制默认不构建；`bench` 预设（Release + 启用基准 + 关闭测试）可启用它：

```bash
cmake --preset bench
cmake --build --preset bench
./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench
```

欢迎提供其他 主机 × 编译器 组合（Linux / GCC、Linux / Clang、macOS / Apple Clang）的报告 —— 贡献方法见 [`docs/bench/README.md`](../../../docs/bench/README.md)。

## 状态

`v1.1.0` —— **国际化与发布后治理**（里程碑 8），首个 1.0 之后的 MINOR。纯**附加**性 —— 库的二进制与 `v1.0.x` 相同。文档现已提供**简体中文（`zh-Hans`）与日语（`ja`）**版本（英文为权威来源 —— [`docs/i18n/`](../../../docs/i18n/)、[ADR-0032](../../../docs/adr/0032-documentation-i18n-architecture.md)）；规范以英文为权威（[ADR-0033](../../../docs/adr/0033-english-as-the-spec-normative-language.md)）；一份[发布后维护协议](../../../docs/workflow/maintenance.md)（[ADR-0034](../../../docs/adr/0034-post-release-maintenance-protocol.md)）治理维护期；一个可由代理运行的[一致性 lint](../../../tools/consistency_lint.py)（[ADR-0035](../../../docs/adr/0035-agent-runnable-consistency-lint.md)）在 CI 与代理契约中对跨产物一致性进行门禁。四个新 ADR（0032–0035）使总数达到 35。发布说明：[`docs/releases/v1.1.0.md`](../../../docs/releases/v1.1.0.md)。更早的版本：

`v1.0.1` —— 在已冻结的 `v1.0.0` API 之上的**打包补丁**：新增 vcpkg port 和 Conan 2.x recipe（第二阶段分发，ADR [0030](../../../docs/adr/0030-vcpkg-port.md) / [0031](../../../docs/adr/0031-conan-recipe.md)）。所发布的库与 `v1.0.0` 逐字节相同 —— 这是一个 `PATCH`，无 API/ABI/行为变化。发布说明：[`docs/releases/v1.0.1.md`](../../../docs/releases/v1.0.1.md)。它所补丁的稳定基线：

`v1.0.0` —— **首个稳定发布。** 公共 C ABI（`memory_pool_create` / `_alloc` / `_free` / `_destroy` 加上 O(1) 自省访问器）和 C++ 接口（`Pool`、`TypedPool<T>`、`PoolAllocator<T>`、`InstrumentedPool`、`PoolObserver`）在 SemVer 1.0 承诺下冻结 —— 不经 `2.0.0` 不做破坏性变更。`v1.0.0` 封存了里程碑 0–6 构建的特性集 —— O(1) 隐式空闲链表定长块池（每块零元数据）、RAII / 类型化 / STL 分配器 C++ 包装器、编译期可配置的线程安全、可选的几何动态增长以及可选的可观测性 —— 并加入里程碑 7 的打磨：已发布的 Doxygen API 站点（M7.1）、扩展的用法 / 性能 / 兼容性 README（M7.2）、`find_package` 安装 + pkg-config 打包（M7.4），以及设计模式目录（M7.5）和规范符合性（M7.6，[ADR-0029](../../../docs/adr/0029-spec-compliance-acceptance-audit.md)）验收审计。全部十五行 Spec Coverage Map 均为 ✅，并经端到端复核。二十九个 ADR（0001–0029）记录了每一个决策；全部十一个已采用的设计模式均为 Implemented。`v1.0.0` 的发布说明见 [`docs/releases/v1.0.0.md`](../../../docs/releases/v1.0.0.md)。

| 里程碑 | 标题 | 状态 |
|--------|------|------|
| 0 | 代理与工作流脚手架 | ✅ 完成 |
| 1 | 构建系统与项目骨架 | ✅ 完成 |
| 2 | 核心内存池（单线程） | ✅ 完成 |
| 3 | C++ 包装器与类型安全 | ✅ 完成 |
| 4 | 线程安全变体 | ✅ 完成 |
| 5 | 动态增长模式 | ✅ 完成 |
| 6 | 可观测性与 Decorator | ✅ 完成 |
| 7 | 发布与打磨 | ✅ 完成 |
| 8 | 国际化与发布后治理 | ✅ 完成 |

逐任务的分解以及底部的 Spec Coverage Map（从规范章节到路线图条目的可追溯性）见 [`ROADMAP.md`](../../../ROADMAP.md)。

## 兼容性

本库的实现面向 **C++17**（严格，无编译器扩展），公共 C 头文件面向 **ANSI C (C89) + C99** —— 二者均经 CI 验证。**无外部依赖**：仅依赖 C 与 C++ 标准库。完整理由与最低版本推理见 [ADR-0005](../../../docs/adr/0005-toolchain-matrix-and-supported-platforms.md)。

**第一梯队 —— 每个 PR 都门禁**（Linux × {GCC, Clang} + Windows × MSVC + macOS × Apple Clang，覆盖 Debug / Release，并在适用处加 ASan / UBSan / TSan）：

| 操作系统 | 架构 | 编译器（最低版本） |
|----------|------|---------------------|
| Linux   | x86_64 | GCC ≥ 11, Clang ≥ 14 |
| Windows | x86_64 | MSVC ≥ 19.30 (VS 2022 17.0)；clang-cl ≥ 14 可选 |
| macOS   | arm64  | Apple Clang ≥ 14 (Xcode 14) |

**第二梯队 —— 尽力而为，不门禁：** Linux aarch64、macOS x86_64（Apple Silicon 之前）、FreeBSD x86_64、Windows 上的 MinGW-w64。

**语言标准：**

| 接口 | 标准 | 构建标志 |
|------|------|----------|
| C++ 实现 | C++17（严格） | `-std=c++17` / `/std:c++17`，`-Wall -Wextra -Wpedantic -Werror` / `/W4 /WX` |
| C 公共头文件 | ANSI C (C89) **与** C99 | 专用 CI 作业：`-std=c89 -pedantic -Werror`、`-std=c99 -pedantic -Werror` |

**线程安全**在构建期通过 `PBR_MEMORY_POOL_THREAD_SAFETY` CMake 选项选择 —— `NONE`（默认，单线程快速路径）、`MUTEX` 或 `LOCKFREE`（[ADR-0020](../../../docs/adr/0020-thread-safety-strategy-and-compile-time-knob.md)）。动态增长在 `NONE` 和 `MUTEX` 下受支持（`LOCKFREE` 下不支持 —— [ADR-0024](../../../docs/adr/0024-dynamic-growth-synchronization-and-creation-surface.md) §2）。

## 技术栈

除 C 与 C++ 标准库外，本库**无任何运行时/构建依赖**（规范 §3.3）；下表中的每一项要么是语言标准，要么是构建/测试/文档工具，要么是打包集成。面向使用者的编译器与平台矩阵见上文「兼容性」一节。

| 层 | 技术 | 版本 |
|----|------|------|
| 语言（实现） | C++ | C++17（严格，无扩展） |
| 语言（C 公共头文件） | ANSI C | C89 **与** C99 |
| 构建系统 | CMake | ≥ 3.21 |
| 构建生成器 | Ninja（CI 与预设） | 任意较新版本 |
| 编译器（最低） | GCC / Clang / MSVC / Apple Clang | 11 / 14 / 19.30 (VS 2022 17.0) / 14（[ADR-0005](../../../docs/adr/0005-toolchain-matrix-and-supported-platforms.md)） |
| 单元测试 | [doctest](https://github.com/doctest/doctest)（经 CMake `FetchContent`，仅测试） | v2.4.11 |
| 运行时消毒器 | ASan · UBSan · TSan | 编译器自带 |
| 内存检查器 | Valgrind | 发行版（CI：Ubuntu 24.04） |
| 静态分析 / 风格 | clang-tidy · clang-format | LLVM 14+ |
| API 文档 | Doxygen → GitHub Pages | 1.10.x（[ADR-0027](../../../docs/adr/0027-doxygen-html-site-and-publication-pipeline.md)） |
| 项目工具 | Python（一致性 lint，仅标准库） | 3.x（[ADR-0035](../../../docs/adr/0035-agent-runnable-consistency-lint.md)） |
| 打包 | CMake `find_package` + pkg-config · vcpkg port · Conan recipe | [ADR-0028](../../../docs/adr/0028-install-and-packaging-layout.md) / [0030](../../../docs/adr/0030-vcpkg-port.md) / [0031](../../../docs/adr/0031-conan-recipe.md) |
| CI | GitHub Actions | — |
| 运行时依赖 | **无** | — |

## 验证与质量门禁

每个 PR 至少必须通过：

| 门禁 | 要求 |
|------|------|
| 编译器矩阵 | MSVC、GCC、Clang —— Debug 与 Release 构建 |
| 警告 | `-Wall -Wextra -Wpedantic -Werror`（GCC/Clang）或 `/W4 /WX`（MSVC）—— 零警告 |
| `clang-tidy` | 在 diff 上干净；不做大范围禁用 |
| 单元测试 | 覆盖新增/变更的行为；在每个编译器上通过 |
| 消毒器 | ASan + UBSan 通过；一旦触及线程化则加 TSan |
| Valgrind | 演示性测试上 `ERROR SUMMARY: 0 errors from 0 contexts` |
| 公共 API 文档 | Doxygen 兼容，构建无警告 |
| 性能声明 | 由 `src/bench/` 下可复现的基准支撑 |

完整质量契约：[`AGENTS.md`](../../../AGENTS.md) §10。C++ 构建矩阵、消毒器、`clang-format`、`clang-tidy` diff 门禁、ANSI C / C99 验证以及零外部依赖审计在每个 PR 上经 [`ci.yml`](../../../.github/workflows/ci.yml) 运行；一个仅文档的工作流（[`docs.yml`](../../../.github/workflows/docs.yml)）覆盖 markdownlint、内部链接检查与 ADR 编号合理性；一个 docs-site 工作流（[`docs-site.yml`](../../../.github/workflows/docs-site.yml)）在每个 PR 上作为“警告即错误”门禁构建 Doxygen API 参考，并在推送到 `master` 时发布到 GitHub Pages（[ADR-0027](../../../docs/adr/0027-doxygen-html-site-and-publication-pipeline.md)）。上方的徽章对 `master` 门禁。

## 构建与测试

工具链安装完成后，规范的三步流程在每个受支持平台上都相同 —— 仅 shell 层面的链接方式不同。

```bash
# Linux、macOS、MinGW/MSYS2、WSL —— 任意 POSIX shell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

```powershell
# Windows —— Developer PowerShell for VS 2022（PowerShell 5.1 没有 &&）
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

两种调用都在每次推送到 `master` 时由 [CI 矩阵](../../../.github/workflows/ci.yml)端到端演练：Linux × {GCC, Clang}、Windows × MSVC、macOS × Apple Clang —— 覆盖 `debug`、`release`、`asan`、`ubsan` 预设（消毒器预设仅限 POSIX，见 [ADR-0005](../../../docs/adr/0005-toolchain-matrix-and-supported-platforms.md) §3）。上方绿色的 `ci` 徽章是“快速上手在 Windows 与 Linux 上有效”的权威信号。

全新克隆的首次设置 —— 按平台安装 CMake、Ninja 与受支持的编译器，外加排障与完整的质量门禁流程 —— 见 **[本地构建指南](../../../docs/development/local-build.md)**。

## 安装与使用

安装到某个前缀，并用 CMake 的 `find_package` 使用（[ADR-0028](../../../docs/adr/0028-install-and-packaging-layout.md) —— 第一阶段分发）：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPBR_MEMORY_POOL_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
# 在消费者的 CMakeLists.txt 中 —— 无论是安装该包还是
# 经 add_subdirectory / FetchContent 内联引入，导入目标名都相同。
find_package(pbr_memory_pool CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE pbr::memory_pool)
```

安装树包含全部公共头文件（`include/it/d4np/memorypool/`）、静态库、CMake 包配置（`SameMajorVersion` 兼容性）以及供非 CMake（Make / autotools / Meson）构建使用的 pkg-config `pbr-memory-pool.pc`。每个 GitHub Release 的 tar 包内含的正是同一个 `cmake --install` 树。不安装而内联引入也可行：

```cmake
add_subdirectory(path/to/pbr-cpp-memory-pool)   # 或 FetchContent
target_link_libraries(my_app PRIVATE pbr::memory_pool)
```

**vcpkg**（第二阶段 —— [ADR-0030](../../../docs/adr/0030-vcpkg-port.md)）：一个固定到 `v1.0.0` 的 port 位于 [`ports/`](../../../ports/)，今天即可作为 overlay port 使用（同样的 `pbr::memory_pool` 目标）：

```bash
vcpkg install pbr-memory-pool --overlay-ports=ports
```

**Conan**（第二阶段 —— [ADR-0031](../../../docs/adr/0031-conan-recipe.md)）：一个固定到 `v1.0.0` 的 Conan 2.x recipe 位于 [`conan/`](../../../conan/)，今天即可创建（经 `CMakeDeps` 得到同样的 `pbr::memory_pool` 目标）：

```bash
conan create conan/        # 然后依赖 pbr-memory-pool/1.0.0
```

注册表发布（microsoft/vcpkg、ConanCenter / 自托管）已延后 —— 见 [`ports/README.md`](../../../ports/README.md) 和 [`conan/README.md`](../../../conan/README.md)。

## 仓库布局

| 路径 | 内容 |
|------|------|
| [`AGENTS.md`](../../../AGENTS.md) | 面向 AI 编码代理（Codex、Claude、Gemini）的跨工具契约。 |
| [`CLAUDE.md`](../../../CLAUDE.md) | Claude Code 适配器 —— 遵从 `AGENTS.md`。 |
| [`GEMINI.md`](../../../GEMINI.md) | Gemini Antigravity 适配器 —— 遵从 `AGENTS.md`。 |
| [`ROADMAP.md`](../../../ROADMAP.md) | 编号的复选框计划 + Spec Coverage Map。 |
| [`CHANGELOG.md`](../../../CHANGELOG.md) | Keep a Changelog 1.1.0 历史；每次发布的用户可见变更（见 [ADR-0004](../../../docs/adr/0004-versioning-and-release-policy.md)）。 |
| [`src/`](../../../src/) | 全部源代码，Maven 风格布局 —— `src/{main,test,bench}/cpp/it/d4np/memorypool/`。 |
| [`docs/specs/`](../../../docs/specs/) | 功能与技术规范。 |
| [`docs/adr/`](../../../docs/adr/) | 架构决策记录。 |
| [`docs/patterns/`](../../../docs/patterns/) | 设计模式目录 + 规范企业级分类法。 |
| [`docs/workflow/`](../../../docs/workflow/) | Git 与文档约定。 |
| [`docs/development/`](../../../docs/development/) | 本地开发的操作指南（构建、调试、性能分析）。 |
| [`docs/i18n/`](../../../docs/i18n/) | 文档翻译（`zh-Hans`、`ja`）；英文为权威来源（[ADR-0032](../../../docs/adr/0032-documentation-i18n-architecture.md)）。 |

## 致人类贡献者

请按以下顺序开始阅读：

1. [`docs/specs/01_spec_cpp_memory_pool.md`](../../../docs/specs/01_spec_cpp_memory_pool.md) —— 我们在构建什么。
2. [`docs/development/local-build.md`](../../../docs/development/local-build.md) —— 如何在你的机器上构建与测试。
3. [`docs/adr/`](../../../docs/adr/) —— 项目为何如此组织。
4. [`docs/patterns/README.md`](../../../docs/patterns/README.md) —— 我们演练了哪些设计模式以及为什么。
5. [`docs/workflow/git-workflow.md`](../../../docs/workflow/git-workflow.md) —— 分支、提交与 PR 约定。
6. [`ROADMAP.md`](../../../ROADMAP.md) —— 已完成什么、下一步是什么。

## 致 AI 编码代理

本仓库配置为可与 **Claude Code**、**Gemini Antigravity** 和 **ChatGPT Codex** 协作。代理契约位于 [`AGENTS.md`](../../../AGENTS.md) —— 做任何事之前先阅读它。简版：

- 每件产物（代码、文档、提交、分支、PR）都用**英文**。
- 你在功能分支上提交、推送并**起草**拉取请求。维护者手动**开启并合并** PR。
- 非平凡的设计决策在同一个 PR 中记录为 ADR。
- 每个已采用的设计模式都在某个 ADR 中得到论证，并列于 [`docs/patterns/README.md`](../../../docs/patterns/README.md)。
- 企业级质量门禁：警告即错误、`clang-tidy` 干净、ASan/UBSan/TSan 通过、Valgrind 干净、有 Doxygen 文档。
- `README.md` 与 `ROADMAP.md` 与其所描述的工作在同一个 PR 中保持同步。

## 许可证

[MIT](../../../LICENSE) © 2026 Daniel Polo。
