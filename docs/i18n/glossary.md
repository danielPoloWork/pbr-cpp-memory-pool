# Terminology glossary

Canonical term map for translators, per [ADR-0032](../adr/0032-documentation-i18n-architecture.md) §5. Use these renderings so terminology stays consistent across pages and languages. Terms in the **Keep in English** section are *not* translated — leave them verbatim in every language.

## Keep in English (do not translate)

Terms of art, notation, GoF pattern names, and public identifiers stay in English in every translation:

| Term | Why |
|------|-----|
| `free list` | Established term of art (spec §4); translating it loses the connection to the literature. |
| `RAII` | Standard C++ acronym. |
| `Pimpl` | Standard C++ idiom name. |
| `O(1)`, `O(log N)`, `O(N)` | Big-O notation. |
| Factory Method, Builder, Adapter, Iterator, Strategy, Template Method, Composite, Decorator, Observer | GoF pattern names — the catalogue uses the canonical English names ([ADR-0003](../adr/0003-design-patterns-policy.md)). |
| `memory_pool_t`, `memory_pool_create`/`_alloc`/`_free`/`_destroy`, `Pool`, `TypedPool<T>`, `PoolAllocator<T>`, `InstrumentedPool`, `PoolObserver` | Public identifiers — code, never prose. |
| `vcpkg`, `Conan`, `CMake`, `find_package`, `pkg-config`, `Doxygen`, `Valgrind`, `ASan`/`UBSan`/`TSan` | Tool and API names. |

## Translatable terms

| English | `zh-Hans` (简体中文) | `ja` (日本語) |
|---------|----------------------|----------------|
| memory pool | 内存池 | メモリプール |
| pool | 池 | プール |
| block | 块 | ブロック |
| block size | 块大小 | ブロックサイズ |
| block count | 块数量 | ブロック数 |
| allocation | 分配 | 確保（アロケーション） |
| deallocation | 释放 | 解放 |
| allocator | 分配器 | アロケータ |
| backing storage | 后备存储 | バッキングストレージ |
| contiguous | 连续的 | 連続した |
| alignment | 对齐 | アライメント |
| metadata | 元数据 | メタデータ |
| overhead | 开销 | オーバーヘッド |
| (external) fragmentation | （外部）碎片化 | （外部）断片化 |
| constant time | 常数时间 | 定数時間 |
| thread | 线程 | スレッド |
| thread safety | 线程安全 | スレッドセーフ |
| lock-free | 无锁 | ロックフリー |
| mutex | 互斥锁 | ミューテックス |
| dynamic growth | 动态增长 | 動的拡張 |
| exhaustion | 耗尽 | 枯渇 |
| benchmark | 基准测试 | ベンチマーク |
| specification | 规范 | 仕様 |
| reference implementation | 参考实现 | リファレンス実装 |

> The translatable rows are filled with the agreed renderings used by the
> `zh-Hans` (§8.3) and `ja` (§8.4) translations; extend the table as new terms
> arise during translation.
