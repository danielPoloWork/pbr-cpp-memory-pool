# 设计模式目录（概览）

> 🌐 本页面是 [`docs/patterns/README.md`](../../../../../docs/patterns/README.md) 概览部分的简体中文翻译（基于提交 `524f0cc`）。
> **英文版本为唯一权威来源** —— 如本译文与原文不一致，以英文版为准。**逐行的“已采用”表格（含 ADR 链接与代码位置）以英文目录为准**（ADR-0032 §2：本地化的是概览叙述，而非逐行的 ADR 链接）。

本目录记录 `pbr-cpp-memory-pool` 中所有**已采用**、**计划中**、**经评估后拒绝**或**正在评估**的设计模式。每当某个 PR 引入或移除一个模式时，都必须阅读并在同一个 PR 中更新本目录。

- **策略（Policy）** —— [ADR-0003 —— 设计模式策略](../../../../../docs/adr/0003-design-patterns-policy.md)。
- **规则摘要** —— [`AGENTS.md`](../../../../../AGENTS.md) §8。
- **规范分类法** —— [`design-patterns.md`](../../../../../docs/patterns/design-patterns.md)。涵盖八大类别（Creational、Structural、Behavioral、EIP、Architectural、Concurrency、Cloud/Distributed、Data & Persistence）的完整企业级模式清单。本目录、ADR 和提交信息中使用的所有模式名称都必须与该文件中的拼写一致。

## 如何使用本目录

- **新增模式。** 当某个 PR 采用一个模式时，向 *Adopted* 表添加一行（若实现跨多个里程碑分阶段进行，则添加到 *Planned*），附上 ADR 链接和代码位置。
- **细化模式。** 当某个既有模式的实现发生实质性变化时，更新该行；若引入了新的 ADR，则附上链接。
- **拒绝模式。** 当某个模式曾是某问题的可信候选但最终被排除时，将其加入 *Rejected* 并记录原因。不要悄悄丢弃 —— 拒绝本身具有教学价值。
- **移除模式。** 当某个已采用的模式被取代时，将其行移到 *Superseded*，链接取代它的 ADR，并保留历史条目。

状态词汇（状态关键字保持英文，作为项目范围内的规范状态）：

| 状态 (Status) | 含义 |
|---------------|------|
| Planned     | 已在某个 ADR 中决定采用；实现尚未落地。 |
| Implemented | 模式已存在于 `src/main/cpp/...`；其链接的 ADR 状态为 `Accepted`。 |
| Considered  | 模式曾针对某个具体问题被评估；结论记录在该行。 |
| Rejected    | 经评估后被排除 —— 原因已记录。 |
| Superseded  | 曾被实现；在后续 PR 中被另一种方案取代。 |

## 已采用 / 计划中（概览）

> 已于 M7.5（2026-06-14）审计：全部十一个已采用模式都已核实具备 (a) 一个 `Accepted` 的 ADR，以及 (b) 在 `src/main/cpp/it/d4np/memorypool/` 中的一处实际代码位置。Object Pool 本身是项目的前提（由规范确定，而非可自由裁量的采用），因此被归在下方的*候选模式*中，而不是作为一行可自由裁量的“已采用”条目。

下表为概览（一行一句说明）。**权威的逐行表格 —— 含每个模式的 ADR 链接与代码位置 —— 见英文目录** [`docs/patterns/README.md`](../../../../../docs/patterns/README.md)。

| # | 模式 (Pattern) | 状态 | 简述 |
|---|----------------|------|------|
| 1 | RAII | Implemented | C++ 包装器拥有 C 句柄的生命周期（构造调用 `memory_pool_create`，析构调用 `memory_pool_destroy`；禁用拷贝、仅移动）。（ADR-0010） |
| 2 | Pimpl | Implemented | `struct memory_pool` 的布局对所有 C/C++ 消费者不透明；C 句柄*即*实现体（C 风格 Pimpl）。（ADR-0010） |
| 3 | Factory Method | Implemented | `Pool::make` 以 `std::optional<Pool>` 返回构造结果（失败为 `std::nullopt`）；`Pool::make_dynamic` 是可增长池的具名构造工厂。（ADR-0011） |
| 4 | Builder | Implemented | `PoolBuilder` 提供流式配置 `.with_block_size().with_block_count().build()`，并经 `.with_growth_factor()` 吸收 M5 的增长策略。（ADR-0011） |
| 5 | Adapter | Implemented | `PoolAllocator<T>` 将池的定长 `void*` 接口适配到每个标准容器所期望的 *Cpp17Allocator* 契约；不适配的请求回退到 `::operator new`。（ADR-0018） |
| 6 | Iterator | Implemented | `FreeListIterator` / `FreeListView`：只读遍历隐式空闲链表用于诊断，整套接口受 `PBR_MEMORY_POOL_DIAGNOSTICS` 门控（默认仅 debug 构建启用）。（ADR-0019） |
| 7 | Strategy | Implemented | 可插拔的线程安全策略（`SingleThreadedPolicy` / `MutexPolicy` / `LockFreePolicy`），**在编译期**绑定，故单线程构建零开销。（ADR-0020） |
| 8 | Template Method | Implemented | `alloc_skeleton` / `free_skeleton` 定义不变的分配/释放框架，并把同步的头部修改委托给两个策略钩子 `pop_head` / `push_head`。（ADR-0021） |
| 9 | Composite | Implemented | 池将其内联的首个 chunk 与溢出 `Chunk` 叶节点的前向链表组合在同一个共享空闲链表之下，支撑动态增长。（ADR-0023） |
| 10 | Decorator | Implemented | `InstrumentedPool` 组合一个 `Pool` 并统计分配活动；按类型可选，未装饰的池零开销。（ADR-0025） |
| 11 | Observer | Implemented | 运行期 `PoolObserver`（经 `InstrumentedPool::add_observer` 注册）通知 `exhausted` / `grew` / `destroyed` 生命周期事件。（ADR-0026） |

## 候选模式

[`design-patterns.md`](../../../../../docs/patterns/design-patterns.md) 中的分类法列出了对整个 PBR 系列而言在范围内的所有模式。本节将该全集收窄到**对*本*制品（一个定长块内存池库）可能适用**的模式。每个模式在被未来某个 ADR 采用或明确拒绝之前都仍是候选；该清单**并非**应用它们的承诺。

模式按其在 `design-patterns.md` 中所属的分类法类别分组。此处未列出的类别（EIP、Architectural、Cloud & Distributed、Data & Persistence）对本制品视为**超出范围** —— 见下方*超出范围的类别*。

### Creational（创建型）

| 模式 | 可能的应用 |
|------|------------|
| Object Pool | 整个项目*本身*即是此模式。 |
| Factory Method | 构造已配置的池变体（线程安全 vs 单线程、可增长 vs 固定）。*M2 初次采用 → 已采用 #3；线程安全最终以编译期 Strategy 实现，而非工厂派发。* |
| Abstract Factory | 协调一致的变体家族（例如配对“带instrumentation的池 + 追踪器”的调试家族）。 |
| Builder | 流式配置：`PoolBuilder().with_block_size(64).with_block_count(1024).build()`。*M2 采用 → 已采用 #4。* |
| Prototype | 克隆一个既有的池*配置*（而非其状态）以衍生同类池。 |
| Lazy Initialization | 推迟后备存储的分配，直到首次 `alloc()`。 |
| Singleton | 仅当出现一个有充分理由限定作用域的默认池时。默认立场：**避免**。 |
| Multiton | 按块大小命名的池注册表。仅当出现注册表用例时才可能。 |
| Dependency Injection | 经 STL 分配器适配器把池注入消费者（与 Adapter 重叠）。 |

### Structural（结构型）

| 模式 | 可能的应用 |
|------|------------|
| Adapter | 基于底层池的 STL 兼容分配器。*M3.3 采用 → 已采用 #5（`PoolAllocator<T>`）。* |
| Bridge | 将公共的 `Pool` 抽象与可互换的后端实现解耦。 |
| Composite | 动态增长变体的链式 chunk 实现。*M5.2 采用 → 已采用 #9（`Chunk` 链表 + `overflow_`）。* |
| Decorator | 围绕池实例的日志/追踪/统计包装。*M6.1 采用 → 已采用 #10（`InstrumentedPool`）。* |
| Facade | 在细粒度 C API 之上提供高层便捷入口。 |
| Flyweight | 已隐含于块复用之中；可能为共享的每池元数据扮演显式角色。 |
| Proxy | 在原始块指针之上的智能句柄类型，用于带边界检查的调试构建。 |
| Private Class Data | 封装池内部 —— 与 **Pimpl** 高度重叠（见*惯用法*）。 |

### Behavioral（行为型）

| 模式 | 可能的应用 |
|------|------------|
| Strategy | 可插拔的线程安全策略（无锁/互斥/lock-free）。*M4.1 采用 → 已采用 #7（编译期策略）；实现见 M4.2/M4.3。* |
| Template Method | 带钩子点的分配算法骨架，供 Strategy 接入。*M4.2 采用 → 已采用 #8（`alloc_skeleton` / `free_skeleton`）。* |
| Iterator | 只读遍历空闲链表用于诊断与测试。*M3.4 采用 → 已采用 #6（`FreeListIterator` / `FreeListView`）。* |
| State | 显式的池状态：空 / 部分 / 耗尽；状态转移驱动观察者事件。 |
| Memento | 为确定性测试场景快照池的空闲链表状态。 |
| Null Object | 用于需要在不分配的情况下演练 API 的测试的空操作池。 |
| Command | 延迟 alloc/free 操作以做批处理或可重放执行（小众；调试工具）。 |
| Chain of Responsibility | 跨多个不同块大小的池的回退链。 |
| Specification | 对候选块的声明式谓词，用于诊断查询。 |

### Concurrency（并发型）

| 模式 | 可能的应用 |
|------|------------|
| Monitor Object | 默认的基于锁的线程安全 Strategy 实现。 |
| Immutable Object | 配置对象（块大小、数量、策略标志）在构造后不可变。 |
| Guarded Suspension | 若/当加入等待式 API 时的“耗尽即阻塞”变体（`alloc_blocking`）。 |
| Active Object | 异步池服务；对分配器而言不太可能，但作为一种考量保留。 |
| Thread Pool | 不属于池的职责，但很可能是其*消费者* —— 与基准测试设计相关。 |

### C++ 惯用补充（不在 `design-patterns.md` 中，但被理所当然地期望）

以下并非经典 GoF 模式，但在现代 C++ 中是惯用且实际上必需的：

- **RAII** —— 每个拥有资源的 C++ 类型都通过其析构函数管理资源生命周期。
- **Pimpl** —— 用不透明指针隐藏 C++ 类布局以做 ABI 隔离；与 *Private Class Data* 重叠。

当一个候选移到 *Planned* 或 *Implemented* 时，它会获得自己的 ADR，并且其行会迁移到上方的*已采用 / 计划中*表。

## 超出范围的类别

以下来自 [`design-patterns.md`](../../../../../docs/patterns/design-patterns.md) 的类别被预先归类为对本制品**不适用**。在此记录是为了在不向 *Rejected* 表填入 N/A 噪声的前提下，恪守“显式拒绝”的策略。若未来某个 PR 中出现意外契合，则提交一个 ADR 并把相关模式从本节迁出。

| 类别 | 原因 |
|------|------|
| Enterprise Integration Patterns (EIP) | 这是一个库，而非消息系统。 |
| Architectural application styles（MVC/MVP/MVVM、Hexagonal、Clean、DDD……） | 这是一个构建块，而非应用。消费者可以在其周围应用这些。 |
| Cloud & Distributed Systems Patterns | 内存分配器按定义是进程本地的。 |
| Data & Persistence Patterns | 没有持久化层；池状态按设计是易失的。 |
