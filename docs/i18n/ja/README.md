# 高性能メモリプールマネージャ（C++）

[![ci](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/ci.yml)
[![docs](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml/badge.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/actions/workflows/docs.yml)
[![API reference](https://img.shields.io/badge/API%20reference-Doxygen-1f6feb.svg)](https://danielpolowork.github.io/pbr-cpp-memory-pool/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../../../LICENSE)
[![Standard: C++17 / ANSI C](https://img.shields.io/badge/Standard-C%2B%2B17%20%2F%20ANSI%20C-blue.svg)](docs/specs/01_spec_cpp_memory_pool.md)
[![Status: v1.1.1 stable](https://img.shields.io/badge/Status-v1.1.1%20stable-brightgreen.svg)](https://github.com/danielPoloWork/pbr-cpp-memory-pool/releases/tag/v1.1.1)

> 🌐 本ページはプロジェクトの [`README.md`](../../../README.md) の日本語訳です（コミット `23fc6c4` 時点）。
> **英語版が唯一の正本です** —— 本訳文と原文に相違がある場合は英語版が優先します。
> 他の言語で読む：[English](../../../README.md) · [简体中文](../zh-Hans/README.md)。
> 本ディレクトリの他の日本語ページ：[仕様](docs/specs/01_spec_cpp_memory_pool.md) · [デザインパターンカタログ（概要）](docs/patterns/README.md)。未翻訳のページは英語原文にフォールバックします。
>
> **Purpose-Built References (PBR)** シリーズの一部 —— 小さく、教育的で、本番品質の、高性能な構築ブロックの C/C++ リファレンス実装群。

多くの高性能システム —— グラフィックスエンジン、金融取引サーバ、データベース —— は、メモリの断片化や、`malloc`/`free`（あるいは `new`/`delete`）の頻繁な呼び出しのオーバーヘッドに悩まされます。本コンポーネントは、連続したメモリブロックを事前確保し、**定数時間・固定サイズ・外部断片化ゼロの確保**を提供する**カスタムメモリプール（Memory Pool）**を提供します。これは単一で焦点を絞った C/C++ ライブラリ —— 4 関数の C ABI と地道な C++17 ラッパーを備えた、ヘッダに支えられた静的ライブラリ —— であり、企業級の品質（警告＝エラー、サニタイザ、Valgrind、Doxygen）で構築され、その [ADR](../../../docs/adr/) に決定ごとに記録されています。

## 概要

- **確保：** O(1)、固定ブロックサイズ、連続した後備ストレージ。
- **空きリスト戦略：** 暗黙的 —— 空きブロックは自身の先頭 `sizeof(void*)` バイトに次の空きブロックへのポインタを格納するため、使用中ブロックのメタデータオーバーヘッドはゼロ。
- **メタデータオーバーヘッド：** 1 ブロックあたり 0 バイト（空きリストのリンクは未使用ブロックの記憶域を再利用）+ 1 プールあたり固定で約 40 バイト —— `block_count` に依存しない。CI で ≤ 128 バイトにゲート（[ADR-0015](../../../docs/adr/0015-metadata-overhead-budget-and-introspection.md)）。
- **標準：** ANSI C の公開インターフェース、C++17 の内部実装とラッパー。外部依存なし。
- **スレッドセーフ：** 任意、コンパイル時に設定可能（マイルストーン 4）。
- **動的拡張：** 任意の連続あふれ chunk（マイルストーン 5）。
- **可観測性：** 任意の `InstrumentedPool` Decorator（統計、占有）+ ライフサイクルイベント用の `PoolObserver` —— 装飾しないプールにはゼロコスト（マイルストーン 6）。
- **品質ゲート：** `clang-tidy` クリーン、ASan + UBSan +（スレッド化の着地後）TSan 通過、Valgrind クリーン、公開インターフェースは Doxygen ドキュメント付き。
- **ベンチマーク目標：** 1,000,000 回の反復で `malloc`/`free` と比較測定。

## 公開 C API

完全な公開インターフェースは 4 つの関数です。完全な契約は[仕様](docs/specs/01_spec_cpp_memory_pool.md)にあります：

```c
typedef struct memory_pool memory_pool_t;

memory_pool_t* memory_pool_create(size_t block_size, size_t block_count);
void*          memory_pool_alloc(memory_pool_t* pool);
void           memory_pool_free(memory_pool_t* pool, void* block);
void           memory_pool_destroy(memory_pool_t* pool);
```

C++17 の RAII ラッパー（`it::d4np::memorypool::Pool`）と型付きテンプレート（`TypedPool<T>`）がこのインターフェースの上に乗ります —— [`ROADMAP.md`](../../../ROADMAP.md) のマイルストーン 2.5 と 3.2 を参照。

完全で相互リンクされた **API リファレンス** —— 各公開シンボルのパラメータ / 戻り値 / 送出契約 —— はヘッダ内の Doxygen コメントから生成され、`master` への push ごとに [GitHub Pages](https://danielpolowork.github.io/pbr-cpp-memory-pool/) に公開されます（[ADR-0027](../../../docs/adr/0027-doxygen-html-site-and-publication-pipeline.md)）。同じビルドが各 PR で「警告＝エラー」ゲートとして実行されます。

## 使い方

公開インクルードルートは `src/main/cpp` なので、ヘッダは `<it/d4np/memorypool/…>` の形で include します。静的ライブラリターゲット `pbr_memory_pool` にリンクしてください（下記「ビルドとテスト」節を参照）。以下の各スニペットは、各リリース前に保守者がそのままコンパイル・実行しています。C++ インターフェースは名前空間 `it::d4np::memorypool` にあります（以下では簡潔さのため省略）。

### C API —— 4 関数のコア

```c
#include <it/d4np/memorypool/memory_pool.h>

memory_pool_t* pool = memory_pool_create(64, 1024);  /* 64 バイトのブロック 1024 個 */
if (pool != NULL) {
    void* block = memory_pool_alloc(pool);           /* O(1)；枯渇時は NULL */
    if (block != NULL) {
        memory_pool_free(pool, block);               /* O(1)；空きリストへ戻す */
    }
    memory_pool_destroy(pool);                       /* 全後備ストレージを解放 */
}
```

### C++ RAII ラッパー —— `Pool`

`Pool` はハンドルを所有し（構築＝作成、破棄＝破棄）、ムーブのみです。例外方針は二動詞式（[ADR-0016](../../../docs/adr/0016-exception-policy-at-the-c-cpp-boundary.md)）：`allocate()` は枯渇時に `std::bad_alloc` を送出し、`try_allocate()` は `noexcept` で `nullptr` を返します。

```cpp
#include <it/d4np/memorypool/memory_pool.hpp>
using namespace it::d4np::memorypool;

Pool pool(64, 1024);                       // 設定が不正なら std::bad_alloc を送出
if (void* block = pool.try_allocate()) {   // 例外を送出しない動詞
    pool.deallocate(block);
}

// 例外ではなく戻り値で失敗を表す —— Factory Method / Builder（ADR-0011）：
if (std::optional<Pool> p = Pool::make(64, 1024)) {
    void* b = p->allocate();               // 送出する動詞 —— 決して nullptr を返さない
    p->deallocate(b);
}

auto built = PoolBuilder{}.with_block_size(64).with_block_count(1024).build();
```

### 型安全なプール —— `TypedPool<T>`

`T` から仕様準拠の `block_size` をコンパイル時に導出し、オブジェクトのライフタイム動詞を追加します（`construct` は placement-new、`destroy` はデストラクタを実行）。

```cpp
#include <it/d4np/memorypool/typed_pool.hpp>

TypedPool<Widget> pool(1024);
Widget* w = pool.construct(arg1, arg2);    // 確保 + placement-new（強い保証）
// ... w を使う ...
pool.destroy(w);                           // ~Widget() + スロットをプールへ返す
```

### STL コンテナ —— `PoolAllocator<T>`

*Cpp17Allocator* Adapter（[ADR-0018](../../../docs/adr/0018-stl-allocator-adapter.md)）。ノードベースのコンテナ（`std::list`、`std::map`、`std::set`）は O(1) のプール高速経路で動作します。過大 / 複数要素の要求は透過的に `::operator new` へフォールバックします。プールはコンテナより長く生存しなければなりません。

```cpp
#include <it/d4np/memorypool/pool_allocator.hpp>
#include <list>

Pool pool(64, 1024);                       // 64 バイトのブロックは list<int> のノードに収まる
std::list<int, PoolAllocator<int>> values{PoolAllocator<int>{pool}};
for (int i = 0; i < 100; ++i) {
    values.push_back(i);                   // 各ノードはプールから供給される
}
```

### 動的拡張

任意・プール単位（[ADR-0022](../../../docs/adr/0022-dynamic-growth-policy-and-chunk-linking.md)）：枯渇時にプールは新しい連続 chunk を取得し、失敗する代わりに幾何級数的に拡張します。既定のプールは固定サイズのままです。

```cpp
if (std::optional<Pool> pool = Pool::make_dynamic(64, 256, /*growth_factor=*/2)) {
    for (int i = 0; i < 100000; ++i) {
        (void)pool->try_allocate();        // nullptr を返す代わりに 256 → 512 → … と拡張
    }
}
```

### 可観測性 —— `InstrumentedPool` + `PoolObserver`

確保活動を計数しライフサイクルイベントを発行する、任意の Decorator（[ADR-0025](../../../docs/adr/0025-decorator-for-instrumented-pool.md)、[ADR-0026](../../../docs/adr/0026-observer-for-pool-lifecycle-events.md)）。`Pool` を直接使うプログラムは何のコストも払いません。

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
    pool->add_observer(observer);          // 観察者はプールより長く生存しなければならない
    void* b = pool->try_allocate();
    pool->deallocate(b);
    PoolStats s = pool->stats();           // allocations_、deallocations_、live_、peak_live_……
    pool->write_summary(std::cout);
}
```

## アーキテクチャ

プールは**空きブロック自身の内部に埋め込まれた空きリスト**でフリーメモリを管理します：ブロックが空いているとき、その先頭 `sizeof(void*)` バイトが次の空きブロックのアドレスを保持します。使用中ブロックはメタデータを一切持ちません。

```text
+-------------------------------------------------------------------+
|                       メモリプール (Memory Pool)                   |
+-------------------------------------------------------------------+
| [ブロック 1（空き）]   -> 「ブロック 2」への next-free ポインタ
| [ブロック 2（使用中）] -> ユーザーデータ
| [ブロック 3（空き）]   -> 「ブロック 4」への next-free ポインタ
| [ブロック 4（空き）]   -> NULL（空きリストの終端）
+-------------------------------------------------------------------+
```

空きリストのレイアウト、`block_size ≥ sizeof(void*)` 制約、アライメント保証は [ADR-0009](../../../docs/adr/0009-free-list-layout-block-size-constraints-and-alignment-guarantee.md) で確定されています。周辺の決定は [ADR-0002](../../../docs/adr/0002-adopt-cross-language-source-layout.md) と [ADR-0003](../../../docs/adr/0003-design-patterns-policy.md) に記録されています。

## 性能

**要約。** `malloc`/`free` に対し、事前にサイズ確保した固定プールは単一スレッドで **4–11× 高速**。*拡張中*のプールは chunk 取得中でも **約 2× 高速**を維持します。スレッド競合下では lock-free 方針がミューテックスに勝りますが、単一共有ヘッドのプールは `malloc` のスレッド毎アリーナのスケーラビリティを上回れません。以下の数値はすべて保守者の **Intel i5-6600K (Skylake) × Windows 10 × MSVC 19.51 Release** ワークステーション、64 バイトブロック、1,000,000 回の反復 × 10 ラウンド（初回はウォームアップとして破棄）。方法論の契約：[ADR-0014](../../../docs/adr/0014-microbenchmark-methodology-pool-vs-malloc.md)。

**固定・単一スレッド**（M2.9 / 仕様 §6.3 —— 完全レポート：[`docs/bench/v0.2.0-windows-msvc-x64.md`](../../../docs/bench/v0.2.0-windows-msvc-x64.md)）：

| シナリオ     | `malloc` 中央値 (ns/op) | `pool` 中央値 (ns/op) | `malloc` / `pool` |
|--------------|------------------------:|----------------------:|------------------:|
| bulk-alloc   | 75.5                    | 6.9                   | **11.02 ×**       |
| bulk-free    | 44.5                    | 8.3                   | **5.35 ×**        |
| interleaved  | 49.9                    | 11.2                  | **4.45 ×**        |

**動的拡張**（M5.4 —— 完全レポート：[`docs/bench/v0.5.0-windows-msvc-x64-growth.md`](../../../docs/bench/v0.5.0-windows-msvc-x64-growth.md)）：実行中に 256 ブロックから 1,000,000 ブロックへ拡張するプールは、償却バルク確保で **55 ns/op、対して `malloc` は 108 —— 1.96× 高速**。代償は約 12 回の幾何級数的 chunk 取得です。ワーキングセットのサイズが既知なら事前確保（約 11× の経路）、未知なら拡張モードを使ってください。

**スレッド化**（M4.5 —— 完全レポート：[`docs/bench/v0.4.0-windows-msvc-x64-threading.md`](../../../docs/bench/v0.4.0-windows-msvc-x64-threading.md)）：`--scenario concurrent` は `T` スレッドで共有プールを操作し、スレッドセーフ方針ごとにビルドします。単一スレッド高速経路は保たれます（`NONE` の interleaved ≈ 9 ns/op、不変）。4 スレッド競合下では `LOCKFREE`（41.8 ns/op）が `MUTEX`（69.5 ns/op）に勝ります。

ベンチマークバイナリは既定でビルドされません。`bench` プリセット（Release + ベンチ有効 + テスト無効）で有効化します：

```bash
cmake --preset bench
cmake --build --preset bench
./build/bench/src/bench/cpp/it/d4np/memorypool/pool_vs_malloc_bench
```

他の ホスト × コンパイラ の組み合わせ（Linux / GCC、Linux / Clang、macOS / Apple Clang）のレポートも歓迎します —— 貢献の手引きは [`docs/bench/README.md`](../../../docs/bench/README.md) を参照。

## ステータス

`v1.1.1` —— **保守リリース**（ドキュメント / プロセス / ツール）、`v1.1.0` 以降最初の PATCH。出荷されるライブラリは `v1.1.0` とバイト単位で同一 —— API/ABI/挙動の変更なし。リポジトリ内の[バグ台帳（bug ledger）](../../../docs/bugs/)とトリアージプロトコル（[ADR-0039](../../../docs/adr/0039-bug-ledger-and-triage-protocol.md)）、[PR メタデータポリシー](../../../docs/adr/0040-pull-request-metadata-policy.md)、[`SECURITY.md`](../../../SECURITY.md)、vcpkg/Conan recipe 向けの `packaging-smoke` CI、[セッションジャーナル（session journal）](../../../docs/journal/)（[ADR-0036](../../../docs/adr/0036-session-journal-extraction.md)）、新機能のロードマップ配置ルール（[ADR-0037](../../../docs/adr/0037-new-feature-roadmap-placement.md)）、リリース単位のチェンジログ分割（[ADR-0038](../../../docs/adr/0038-changelog-version-split.md)）を追加。5 つの新規 ADR（0036–0040）で総数は 40 になりました。リリースノート：[`docs/releases/v1.1.1.md`](../../../docs/releases/v1.1.1.md)。より以前のバージョン：

`v1.1.0` —— **国際化とリリース後ガバナンス**（マイルストーン 8）、1.0 以降最初の MINOR。純粋に**追加的** —— ライブラリのバイナリは `v1.0.x` と同一です。ドキュメントは**簡体字中国語（`zh-Hans`）と日本語（`ja`）**で提供されるようになりました（英語が正本 —— [`docs/i18n/`](../../../docs/i18n/)、[ADR-0032](../../../docs/adr/0032-documentation-i18n-architecture.md)）；仕様は英語を正本とします（[ADR-0033](../../../docs/adr/0033-english-as-the-spec-normative-language.md)）；[リリース後保守プロトコル](../../../docs/workflow/maintenance.md)（[ADR-0034](../../../docs/adr/0034-post-release-maintenance-protocol.md)）が保守フェーズを統治し；エージェントが実行可能な[一貫性 lint](../../../tools/consistency_lint.py)（[ADR-0035](../../../docs/adr/0035-agent-runnable-consistency-lint.md)）が CI とエージェント契約で成果物間の整合をゲートします。4 つの新規 ADR（0032–0035）で総数は 35 になりました。リリースノート：[`docs/releases/v1.1.0.md`](../../../docs/releases/v1.1.0.md)。より以前のバージョン：

`v1.0.1` —— 凍結された `v1.0.0` API 上の**パッケージングパッチ**：vcpkg port と Conan 2.x recipe を追加（第 2 段階の配布、ADR [0030](../../../docs/adr/0030-vcpkg-port.md) / [0031](../../../docs/adr/0031-conan-recipe.md)）。出荷されるライブラリは `v1.0.0` とバイト単位で同一 —— `PATCH` であり、API/ABI/挙動の変更なし。リリースノート：[`docs/releases/v1.0.1.md`](../../../docs/releases/v1.0.1.md)。これがパッチを当てた安定ベースライン：

`v1.0.0` —— **最初の安定リリース。** 公開 C ABI（`memory_pool_create` / `_alloc` / `_free` / `_destroy` と O(1) の自省アクセサ）と C++ インターフェース（`Pool`、`TypedPool<T>`、`PoolAllocator<T>`、`InstrumentedPool`、`PoolObserver`）は SemVer 1.0 の約束の下で凍結されます —— `2.0.0` なしに破壊的変更はありません。`v1.0.0` はマイルストーン 0–6 で構築した機能群 —— O(1) 暗黙空きリストの固定ブロックプール（1 ブロックあたりメタデータゼロ）、RAII / 型付き / STL アロケータの C++ ラッパー、コンパイル時設定可能なスレッドセーフ、任意の幾何級数的動的拡張、任意の可観測性 —— を封印し、マイルストーン 7 の仕上げを加えます：公開された Doxygen API サイト（M7.1）、拡充された 使い方 / 性能 / 互換性 README（M7.2）、`find_package` インストール + pkg-config パッケージング（M7.4）、デザインパターンカタログ（M7.5）と仕様適合性（M7.6、[ADR-0029](../../../docs/adr/0029-spec-compliance-acceptance-audit.md)）の受け入れ監査。Spec Coverage Map の全 15 行が ✅ で、エンドツーエンドに再検証済み。29 個の ADR（0001–0029）が各決定を記録し、採用済みの全 11 デザインパターンが Implemented。`v1.0.0` のリリースノートは [`docs/releases/v1.0.0.md`](../../../docs/releases/v1.0.0.md) にあります。

| マイルストーン | タイトル | ステータス |
|----------------|----------|------------|
| 0 | エージェントとワークフローの足場 | ✅ 完了 |
| 1 | ビルドシステムとプロジェクト骨格 | ✅ 完了 |
| 2 | コアメモリプール（単一スレッド） | ✅ 完了 |
| 3 | C++ ラッパーと型安全 | ✅ 完了 |
| 4 | スレッドセーフ版 | ✅ 完了 |
| 5 | 動的拡張モード | ✅ 完了 |
| 6 | 可観測性と Decorator | ✅ 完了 |
| 7 | リリースと仕上げ | ✅ 完了 |
| 8 | 国際化とリリース後ガバナンス | ✅ 完了 |

タスク単位の内訳と、末尾の Spec Coverage Map（仕様章から路線図項目への追跡可能性）は [`ROADMAP.md`](../../../ROADMAP.md) を参照。

## 互換性

本ライブラリは実装に **C++17**（厳格、コンパイラ拡張なし）、公開 C ヘッダに **ANSI C (C89) + C99** を対象とします —— いずれも CI で検証済み。**外部依存はありません**：C と C++ の標準ライブラリのみ。完全な根拠と最低バージョンの推論は [ADR-0005](../../../docs/adr/0005-toolchain-matrix-and-supported-platforms.md)。

**第 1 ティア —— 全 PR でゲート**（Linux × {GCC, Clang} + Windows × MSVC + macOS × Apple Clang、Debug / Release と、該当する場合 ASan / UBSan / TSan）：

| OS | アーキ | コンパイラ（最低） |
|----|--------|---------------------|
| Linux   | x86_64 | GCC ≥ 11, Clang ≥ 14 |
| Windows | x86_64 | MSVC ≥ 19.30 (VS 2022 17.0)；clang-cl ≥ 14 任意 |
| macOS   | arm64  | Apple Clang ≥ 14 (Xcode 14) |

**第 2 ティア —— ベストエフォート、ゲートなし：** Linux aarch64、macOS x86_64（Apple Silicon 以前）、FreeBSD x86_64、Windows の MinGW-w64。

**言語標準：**

| インターフェース | 標準 | ビルドフラグ |
|------------------|------|--------------|
| C++ 実装 | C++17（厳格） | `-std=c++17` / `/std:c++17`、`-Wall -Wextra -Wpedantic -Werror` / `/W4 /WX` |
| C 公開ヘッダ | ANSI C (C89) **と** C99 | 専用 CI ジョブ：`-std=c89 -pedantic -Werror`、`-std=c99 -pedantic -Werror` |

**スレッドセーフ**はビルド時に `PBR_MEMORY_POOL_THREAD_SAFETY` CMake オプションで選択します —— `NONE`（既定、単一スレッド高速経路）、`MUTEX`、または `LOCKFREE`（[ADR-0020](../../../docs/adr/0020-thread-safety-strategy-and-compile-time-knob.md)）。動的拡張は `NONE` と `MUTEX` でサポートされます（`LOCKFREE` では非対応 —— [ADR-0024](../../../docs/adr/0024-dynamic-growth-synchronization-and-creation-surface.md) §2）。

## 技術スタック

C と C++ の標準ライブラリを除き、本ライブラリに**実行時／ビルド依存はありません**（仕様 §3.3）；下表の各項目は、言語標準か、ビルド／テスト／ドキュメントのツールか、パッケージング連携のいずれかです。利用者向けのコンパイラとプラットフォーム行列は上記「互換性」節を参照してください。

| 層 | 技術 | バージョン |
|----|------|------------|
| 言語（実装） | C++ | C++17（厳格、拡張なし） |
| 言語（C 公開ヘッダ） | ANSI C | C89 **と** C99 |
| ビルドシステム | CMake | ≥ 3.21 |
| ビルドジェネレータ | Ninja（CI とプリセット） | 任意の新しめのバージョン |
| コンパイラ（最低） | GCC / Clang / MSVC / Apple Clang | 11 / 14 / 19.30 (VS 2022 17.0) / 14（[ADR-0005](../../../docs/adr/0005-toolchain-matrix-and-supported-platforms.md)） |
| 単体テスト | [doctest](https://github.com/doctest/doctest)（CMake `FetchContent` 経由、テスト時のみ） | v2.4.11 |
| 実行時サニタイザ | ASan · UBSan · TSan | コンパイラ付属 |
| メモリチェッカ | Valgrind | ディストリ版（CI：Ubuntu 24.04） |
| 静的解析 / スタイル | clang-tidy · clang-format | LLVM 14+ |
| API ドキュメント | Doxygen → GitHub Pages | 1.10.x（[ADR-0027](../../../docs/adr/0027-doxygen-html-site-and-publication-pipeline.md)） |
| プロジェクトツール | Python（一貫性 lint、標準ライブラリのみ） | 3.x（[ADR-0035](../../../docs/adr/0035-agent-runnable-consistency-lint.md)） |
| パッケージング | CMake `find_package` + pkg-config · vcpkg port · Conan recipe | [ADR-0028](../../../docs/adr/0028-install-and-packaging-layout.md) / [0030](../../../docs/adr/0030-vcpkg-port.md) / [0031](../../../docs/adr/0031-conan-recipe.md) |
| CI | GitHub Actions | — |
| 実行時依存 | **なし** | — |

## 検証と品質ゲート

各 PR は最低限、以下を通過しなければなりません：

| ゲート | 要件 |
|--------|------|
| コンパイラ行列 | MSVC、GCC、Clang —— Debug と Release ビルド |
| 警告 | `-Wall -Wextra -Wpedantic -Werror`（GCC/Clang）または `/W4 /WX`（MSVC）—— ゼロ |
| `clang-tidy` | diff 上でクリーン。広範な無効化なし |
| 単体テスト | 新規/変更した挙動を網羅。全コンパイラで通過 |
| サニタイザ | ASan + UBSan 通過。スレッド化に触れたら TSan |
| Valgrind | デモ用テストで `ERROR SUMMARY: 0 errors from 0 contexts` |
| 公開 API ドキュメント | Doxygen 互換、警告なしでビルド |
| 性能の主張 | `src/bench/` 配下の再現可能なベンチで裏付け |

完全な品質契約：[`AGENTS.md`](../../../AGENTS.md) §10。C++ ビルド行列、サニタイザ、`clang-format`、`clang-tidy` diff ゲート、ANSI C / C99 検証、ゼロ外部依存監査は各 PR で [`ci.yml`](../../../.github/workflows/ci.yml) により実行されます。ドキュメント専用ワークフロー（[`docs.yml`](../../../.github/workflows/docs.yml)）が markdownlint、内部リンク検査、ADR 番号の健全性を扱い、docs-site ワークフロー（[`docs-site.yml`](../../../.github/workflows/docs-site.yml)）が各 PR で「警告＝エラー」ゲートとして Doxygen API リファレンスをビルドし、`master` への push 時に GitHub Pages へ公開します（[ADR-0027](../../../docs/adr/0027-doxygen-html-site-and-publication-pipeline.md)）。上記のバッジが `master` をゲートします。

## ビルドとテスト

ツールチェーンを導入すれば、正規の 3 ステップ手順はすべての対応プラットフォームで同じです —— 異なるのはシェルレベルの連結だけです。

```bash
# Linux、macOS、MinGW/MSYS2、WSL —— 任意の POSIX シェル
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

```powershell
# Windows —— Developer PowerShell for VS 2022（PowerShell 5.1 には && が無い）
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

いずれの呼び出しも、`master` への push ごとに [CI 行列](../../../.github/workflows/ci.yml)でエンドツーエンドに演習されます：Linux × {GCC, Clang}、Windows × MSVC、macOS × Apple Clang —— `debug`、`release`、`asan`、`ubsan` プリセットにわたって（サニタイザプリセットは POSIX のみ、[ADR-0005](../../../docs/adr/0005-toolchain-matrix-and-supported-platforms.md) §3）。上記の緑の `ci` バッジが「クイックスタートが Windows と Linux で動く」という権威ある合図です。

新規クローンでの初回セットアップ —— プラットフォーム別の CMake・Ninja・対応コンパイラの導入、トラブルシューティング、完全な品質ゲート手順 —— は **[ローカルビルドガイド](../../../docs/development/local-build.md)** を参照。

## インストールと利用

ある prefix にインストールし、CMake の `find_package` で利用します（[ADR-0028](../../../docs/adr/0028-install-and-packaging-layout.md) —— 第 1 段階の配布）：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPBR_MEMORY_POOL_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /your/prefix
```

```cmake
# 利用側の CMakeLists.txt にて —— パッケージをインストールしても、
# add_subdirectory / FetchContent でベンダリングしても、import ターゲット名は同じ。
find_package(pbr_memory_pool CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE pbr::memory_pool)
```

インストールツリーには、全公開ヘッダ（`include/it/d4np/memorypool/`）、静的ライブラリ、CMake パッケージ設定（`SameMajorVersion` 互換性）、および非 CMake（Make / autotools / Meson）ビルド向けの pkg-config `pbr-memory-pool.pc` が含まれます。各 GitHub Release の tar アーカイブに含まれるのは同じ `cmake --install` ツリーです。インストールせずにベンダリングすることもできます：

```cmake
add_subdirectory(path/to/pbr-cpp-memory-pool)   # または FetchContent
target_link_libraries(my_app PRIVATE pbr::memory_pool)
```

**vcpkg**（第 2 段階 —— [ADR-0030](../../../docs/adr/0030-vcpkg-port.md)）：`v1.0.0` に固定された port が [`ports/`](../../../ports/) にあり、今日からオーバーレイ port として利用できます（同じ `pbr::memory_pool` ターゲット）：

```bash
vcpkg install pbr-memory-pool --overlay-ports=ports
```

**Conan**（第 2 段階 —— [ADR-0031](../../../docs/adr/0031-conan-recipe.md)）：`v1.0.0` に固定された Conan 2.x recipe が [`conan/`](../../../conan/) にあり、今日から作成できます（`CMakeDeps` 経由で同じ `pbr::memory_pool` ターゲット）：

```bash
conan create conan/        # その後 pbr-memory-pool/1.0.0 に依存
```

レジストリ公開（microsoft/vcpkg、ConanCenter / セルフホスト）は延期されています —— [`ports/README.md`](../../../ports/README.md) と [`conan/README.md`](../../../conan/README.md) を参照。

## リポジトリ構成

| パス | 内容 |
|------|------|
| [`AGENTS.md`](../../../AGENTS.md) | AI コーディングエージェント（Codex、Claude、Gemini）向けのツール横断契約。 |
| [`CLAUDE.md`](../../../CLAUDE.md) | Claude Code アダプタ —— `AGENTS.md` に従う。 |
| [`GEMINI.md`](../../../GEMINI.md) | Gemini Antigravity アダプタ —— `AGENTS.md` に従う。 |
| [`ROADMAP.md`](../../../ROADMAP.md) | 番号付きチェックボックス計画 + Spec Coverage Map。 |
| [`CHANGELOG.md`](../../../CHANGELOG.md) | Keep a Changelog 1.1.0 の履歴。リリース毎のユーザー可視の変更（[ADR-0004](../../../docs/adr/0004-versioning-and-release-policy.md) を参照）。 |
| [`src/`](../../../src/) | 全ソースコード、Maven 風レイアウト —— `src/{main,test,bench}/cpp/it/d4np/memorypool/`。 |
| [`docs/specs/`](../../../docs/specs/) | 機能仕様・技術仕様。 |
| [`docs/adr/`](../../../docs/adr/) | アーキテクチャ決定記録。 |
| [`docs/patterns/`](../../../docs/patterns/) | デザインパターンカタログ + 正規の企業級分類法。 |
| [`docs/workflow/`](../../../docs/workflow/) | Git とドキュメントの規約。 |
| [`docs/development/`](../../../docs/development/) | ローカル開発の手順ガイド（ビルド、デバッグ、プロファイル）。 |
| [`docs/i18n/`](../../../docs/i18n/) | ドキュメント翻訳（`zh-Hans`、`ja`）。英語が正本（[ADR-0032](../../../docs/adr/0032-documentation-i18n-architecture.md)）。 |

## 人間の貢献者へ

次の順序で読み始めてください：

1. [`docs/specs/01_spec_cpp_memory_pool.md`](../../../docs/specs/01_spec_cpp_memory_pool.md) —— 何を作っているか。
2. [`docs/development/local-build.md`](../../../docs/development/local-build.md) —— 自分のマシンでのビルドとテスト方法。
3. [`docs/adr/`](../../../docs/adr/) —— プロジェクトがこう構成されている理由。
4. [`docs/patterns/README.md`](../../../docs/patterns/README.md) —— どのデザインパターンをなぜ用いるか。
5. [`docs/workflow/git-workflow.md`](../../../docs/workflow/git-workflow.md) —— ブランチ・コミット・PR の規約。
6. [`ROADMAP.md`](../../../ROADMAP.md) —— 完了したことと次のこと。

## AI コーディングエージェントへ

本リポジトリは **Claude Code**、**Gemini Antigravity**、**ChatGPT Codex** と協調するよう構成されています。エージェント契約は [`AGENTS.md`](../../../AGENTS.md) にあります —— 何かをする前に読んでください。短縮版：

- すべての成果物（コード、ドキュメント、コミット、ブランチ、PR）は**英語**。
- 機能ブランチでコミット・push し、PR を**起草**します。保守者が PR を手動で**開き、マージ**します。
- 非自明な設計決定は同じ PR で ADR として記録します。
- 採用した各デザインパターンは ADR で論証し、[`docs/patterns/README.md`](../../../docs/patterns/README.md) に列挙します。
- 企業級の品質ゲート：警告＝エラー、`clang-tidy` クリーン、ASan/UBSan/TSan 通過、Valgrind クリーン、Doxygen ドキュメント付き。
- `README.md` と `ROADMAP.md` は、それらが記述する作業と同じ PR で最新に保ちます。

## ライセンス

[MIT](../../../LICENSE) © 2026 Daniel Polo。
