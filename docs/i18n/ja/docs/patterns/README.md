# デザインパターンカタログ（概要）

> 🌐 本ページは [`docs/patterns/README.md`](../../../../../docs/patterns/README.md) の概要部分の日本語訳です（コミット `6c6aeb7` 時点）。
> **英語版が唯一の正本です** —— 本訳文と原文に相違がある場合は英語版が優先します。**行ごとの「採用済み」表（ADR リンクとコード位置を含む）は英語カタログが正本です**（ADR-0032 §2：ローカライズの対象は概要の叙述であり、行ごとの ADR リンクではありません）。

本カタログは、`pbr-cpp-memory-pool` において**採用済み**・**計画中**・**検討の上で却下**・**評価中**のすべてのデザインパターンを記録します。あるパターンを導入または削除する PR では、必ず本カタログを読み、同じ PR で更新してください。

- **方針（Policy）** —— [ADR-0003 —— デザインパターン方針](../../../../../docs/adr/0003-design-patterns-policy.md)。
- **規則の要約** —— [`AGENTS.md`](../../../../../AGENTS.md) §8。
- **正規の分類法** —— [`design-patterns.md`](../../../../../docs/patterns/design-patterns.md)。8 つのカテゴリ（Creational、Structural、Behavioral、EIP、Architectural、Concurrency、Cloud/Distributed、Data & Persistence）にわたる企業級パターンの全リスト。本カタログ・ADR・コミットメッセージで使うすべてのパターン名は、そこでの綴りに一致しなければなりません。

## 本カタログの使い方

- **パターンの追加。** ある PR がパターンを採用したら、*Adopted* 表に行を追加します（実装が複数のマイルストーンにまたがる場合は *Planned* に）。ADR リンクとコード位置を添えます。
- **パターンの精緻化。** 既存パターンの実装が実質的に変化したら、その行を更新し、新しい ADR があればリンクします。
- **パターンの却下。** あるパターンがある問題の有力候補であったが最終的に除外された場合、理由とともに *Rejected* に追加します。黙って捨てないでください —— 却下には教育的価値があります。
- **パターンの削除。** 採用済みのパターンが置き換えられたら、その行を *Superseded* に移し、置き換えた ADR をリンクし、歴史的記録を残します。

ステータス語彙（ステータスのキーワードは、プロジェクト全体での正規の状態として英語のまま）：

| ステータス (Status) | 意味 |
|---------------------|------|
| Planned     | ある ADR で採用が決定済み。実装はまだ着地していない。 |
| Implemented | パターンが `src/main/cpp/...` に存在する。リンクされた ADR は `Accepted`。 |
| Considered  | ある具体的な問題に対して評価された。結論はその行に記載。 |
| Rejected    | 検討の上で除外された —— 理由を記載。 |
| Superseded  | かつて実装されたが、後続の PR で別の方式に置き換えられた。 |

## 採用済み / 計画中（概要）

> M7.5（2026-06-14）に監査済み：採用済みの全 11 パターンについて、(a) `Accepted` の ADR と、(b) `src/main/cpp/it/d4np/memorypool/` 内の実際のコード位置を有することを確認した。Object Pool それ自体はプロジェクトの前提（仕様により定まり、裁量による採用ではない）であるため、裁量的な「採用済み」行ではなく、下記の*候補パターン*として扱う。

以下は概要（1 行 1 文の説明）です。**権威ある行ごとの表 —— 各パターンの ADR リンクとコード位置 —— は英語カタログ** [`docs/patterns/README.md`](../../../../../docs/patterns/README.md) を参照してください。

| # | パターン (Pattern) | ステータス | 概説 |
|---|--------------------|------------|------|
| 1 | RAII | Implemented | C++ ラッパーが C ハンドルのライフタイムを所有する（構築で `memory_pool_create`、破棄で `memory_pool_destroy` を呼ぶ。コピー禁止・ムーブのみ）。（ADR-0010） |
| 2 | Pimpl | Implemented | `struct memory_pool` のレイアウトはすべての C/C++ 利用者に対して不透明。C ハンドル*そのもの*が実装体（C 風 Pimpl）。（ADR-0010） |
| 3 | Factory Method | Implemented | `Pool::make` は構築結果を `std::optional<Pool>` で返す（失敗は `std::nullopt`）。`Pool::make_dynamic` は拡張可能プールの名前付き構築ファクトリ。（ADR-0011） |
| 4 | Builder | Implemented | `PoolBuilder` が流暢な設定 `.with_block_size().with_block_count().build()` を提供し、`.with_growth_factor()` で M5 の拡張ポリシーを吸収。（ADR-0011） |
| 5 | Adapter | Implemented | `PoolAllocator<T>` がプールの固定長 `void*` インターフェースを、各標準コンテナが期待する *Cpp17Allocator* 契約へ適合させる。適合しない要求は `::operator new` にフォールバック。（ADR-0018） |
| 6 | Iterator | Implemented | `FreeListIterator` / `FreeListView`：診断用に暗黙の空きリストを読み取り専用で走査。`PBR_MEMORY_POOL_DIAGNOSTICS` でゲート（既定では debug ビルドのみ有効）。（ADR-0019） |
| 7 | Strategy | Implemented | 差し替え可能なスレッドセーフ方針（`SingleThreadedPolicy` / `MutexPolicy` / `LockFreePolicy`）。**コンパイル時**に束縛されるため単一スレッドビルドはゼロコスト。（ADR-0020） |
| 8 | Template Method | Implemented | `alloc_skeleton` / `free_skeleton` が不変の確保/解放フレームを定義し、同期されるヘッド更新を 2 つの方針フック `pop_head` / `push_head` に委譲。（ADR-0021） |
| 9 | Composite | Implemented | プールはインラインの先頭 chunk と、あふれ用 `Chunk` リーフノードの前方連結リストを、単一の共有空きリストの下で合成し、動的拡張を支える。（ADR-0023） |
| 10 | Decorator | Implemented | `InstrumentedPool` が `Pool` を合成して確保活動を計数する。型による opt-in で、装飾しないプールはゼロコスト。（ADR-0025） |
| 11 | Observer | Implemented | 実行時 `PoolObserver`（`InstrumentedPool::add_observer` で登録）が `exhausted` / `grew` / `destroyed` のライフサイクルイベントを通知。（ADR-0026） |

## 候補パターン

[`design-patterns.md`](../../../../../docs/patterns/design-patterns.md) の分類法は、PBR シリーズ全体として範囲内のすべてのパターンを列挙します。本節はその全体を、**この成果物（固定長ブロックのメモリプールライブラリ）に適用しうる**パターンへと絞り込みます。各パターンは、将来の ADR で採用されるか明示的に却下されるまで候補のままです。本リストはそれらを適用するという**約束ではありません**。

パターンは `design-patterns.md` における所属カテゴリで分類しています。ここに挙げないカテゴリ（EIP、Architectural、Cloud & Distributed、Data & Persistence）はこの成果物にとって**範囲外**として扱います —— 下記の*範囲外のカテゴリ*を参照。

### Creational（生成）

| パターン | 適用の可能性 |
|----------|--------------|
| Object Pool | プロジェクト全体が*それ自体*このパターン。 |
| Factory Method | 設定済みプールのバリアントを構築（スレッドセーフ vs 単一スレッド、拡張可能 vs 固定）。*M2 で初採用 → 採用済み #3。スレッドセーフはファクトリ派遣ではなくコンパイル時 Strategy として実現された。* |
| Abstract Factory | 一貫したバリアント群（例：instrumentation 付きプール + トレーサを組にしたデバッグ群）。 |
| Builder | 流暢な設定：`PoolBuilder().with_block_size(64).with_block_count(1024).build()`。*M2 採用 → 採用済み #4。* |
| Prototype | 既存プールの*設定*（状態ではない）を複製して姉妹プールを生成。 |
| Lazy Initialization | 最初の `alloc()` まで後備ストレージの確保を遅延。 |
| Singleton | 正当に範囲限定された既定プールが現れた場合のみ。既定の立場：**避ける**。 |
| Multiton | ブロックサイズ別の名前付きプールのレジストリ。レジストリのユースケースが現れた場合のみ妥当。 |
| Dependency Injection | STL アロケータアダプタ経由でプールを利用者に注入（Adapter と重複）。 |

### Structural（構造）

| パターン | 適用の可能性 |
|----------|--------------|
| Adapter | 基盤プール上の STL 互換アロケータ。*M3.3 採用 → 採用済み #5（`PoolAllocator<T>`）。* |
| Bridge | 公開 `Pool` 抽象を、差し替え可能なバックエンド実装から分離。 |
| Composite | 動的拡張バリアント向けの連結 chunk 実装。*M5.2 採用 → 採用済み #9（`Chunk` リスト + `overflow_`）。* |
| Decorator | プールインスタンス周りのロギング/トレース/統計ラッパー。*M6.1 採用 → 採用済み #10（`InstrumentedPool`）。* |
| Facade | 粒度の細かい C API 上の高水準な便宜エントリポイント。 |
| Flyweight | ブロック再利用に既に暗黙的。共有のプール毎メタデータに明示的役割の可能性。 |
| Proxy | 生ブロックポインタ上のスマートハンドル型。境界チェック付きデバッグビルド向け。 |
| Private Class Data | プール内部の隠蔽 —— **Pimpl** と強く重複（*イディオム*参照）。 |

### Behavioral（振る舞い）

| パターン | 適用の可能性 |
|----------|--------------|
| Strategy | 差し替え可能なスレッドセーフ方針（ノーロック/ミューテックス/lock-free）。*M4.1 採用 → 採用済み #7（コンパイル時方針）。実装は M4.2/M4.3。* |
| Template Method | Strategy が接続するフック点を備えた確保アルゴリズムの骨格。*M4.2 採用 → 採用済み #8（`alloc_skeleton` / `free_skeleton`）。* |
| Iterator | 診断とテストのための空きリストの読み取り専用走査。*M3.4 採用 → 採用済み #6（`FreeListIterator` / `FreeListView`）。* |
| State | 明示的なプール状態：空 / 部分 / 枯渇。状態遷移が観察者イベントを駆動。 |
| Memento | 決定的テストシナリオのためにプールの空きリスト状態をスナップショット。 |
| Null Object | 確保せずに API を演習するテスト向けのノーオペレーションプール。 |
| Command | バッチ実行・再実行のために alloc/free 操作を遅延（ニッチ。デバッグツール）。 |
| Chain of Responsibility | 異なるブロックサイズの複数プールにまたがるフォールバック連鎖。 |
| Specification | 診断クエリのための候補ブロックに対する宣言的述語。 |

### Concurrency（並行）

| パターン | 適用の可能性 |
|----------|--------------|
| Monitor Object | 既定のロックベースのスレッドセーフ Strategy 実装。 |
| Immutable Object | 設定オブジェクト（ブロックサイズ・数・方針フラグ）は構築後に不変。 |
| Guarded Suspension | 待機型 API を追加する場合の「枯渇時ブロック」バリアント（`alloc_blocking`）。 |
| Active Object | 非同期プールサービス。アロケータには考えにくいが、考慮として保持。 |
| Thread Pool | プールの関心事ではないが、その*利用者*である可能性が高い —— ベンチマーク設計に関連。 |

### C++ イディオム的補完（`design-patterns.md` には無いが当然に期待される）

以下は古典的 GoF パターンではありませんが、現代 C++ では慣用的で実質的に必須です：

- **RAII** —— 資源を所有するすべての C++ 型は、そのデストラクタで資源のライフタイムを管理する。
- **Pimpl** —— ABI 隔離のために C++ クラスのレイアウトを隠す不透明ポインタ。*Private Class Data* と重複。

ある候補が *Planned* または *Implemented* に移ると、それ専用の ADR を得て、その行は上記の*採用済み / 計画中*表へ移行します。

## 範囲外のカテゴリ

[`design-patterns.md`](../../../../../docs/patterns/design-patterns.md) の以下のカテゴリは、この成果物には**該当しない**と事前に分類します。*Rejected* 表を N/A のノイズで埋めずに「明示的な却下」の方針を守るため、ここに記録します。将来の PR で意外な適合が現れた場合は、ADR を提出し、該当パターンを本節から移してください。

| カテゴリ | 理由 |
|----------|------|
| Enterprise Integration Patterns (EIP) | これはライブラリであり、メッセージングシステムではない。 |
| Architectural application styles（MVC/MVP/MVVM、Hexagonal、Clean、DDD……） | これは構築ブロックであり、アプリケーションではない。利用者がその周囲に適用しうる。 |
| Cloud & Distributed Systems Patterns | メモリアロケータは定義上プロセスローカル。 |
| Data & Persistence Patterns | 永続化層は無い。プール状態は設計上揮発性。 |
