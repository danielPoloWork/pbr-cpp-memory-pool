# Source Tree

This directory holds every source file in the repository, organised under a **Maven-style cross-language layout** so that sibling Purpose-Built References projects in other languages share an instantly recognisable shape.

## Layout

```
src/
├── main/
│   └── cpp/it/d4np/memorypool/      # production sources (.h, .hpp, .c, .cpp)
├── test/
│   └── cpp/it/d4np/memorypool/      # test sources
└── bench/
    └── cpp/it/d4np/memorypool/      # benchmark sources
```

The shape generalises to other languages — a hypothetical Java sibling would use `src/main/java/it/d4np/memorypool/`, a Python sibling `src/main/python/it/d4np/memorypool/`, and so on. The reverse-domain segment `it/d4np/` is the organisational root for the PBR series.

## C++ specifics

- **Namespace:** `it::d4np::memorypool`, mirroring the path 1:1. Components live in nested sub-namespaces (`it::d4np::memorypool::freelist`, …); implementation details live in `it::d4np::memorypool::detail`.
- **Include path:** the build system exposes `src/main/cpp` as the public include root. External consumers and internal translation units alike `#include <it/d4np/memorypool/memory_pool.h>`.
- **Headers and sources co-locate** under `memorypool/`. Private headers are either suffixed `_internal.h` / `_internal.hpp` or placed under a `detail/` subfolder.
- **Component subdivision** is by concern, not by file type — e.g. `memorypool/pool/`, `memorypool/freelist/`, `memorypool/threading/`.

## Why this layout

See [ADR-0002](../docs/adr/0002-adopt-cross-language-source-layout.md) for the full rationale (problem, alternatives considered, consequences). In short: PBR siblings should be navigable by anyone who knows the convention, regardless of which language the specific reference is written in.
