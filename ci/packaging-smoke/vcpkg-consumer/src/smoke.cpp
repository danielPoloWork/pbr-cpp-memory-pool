// Minimal consumer for the vcpkg packaging smoke: construct a pool, allocate
// and free a block, and exercise the typed surface — proving the vcpkg-installed
// package links through the pbr::memory_pool imported target. Mirrors the Conan
// test_package example so both packaging paths assert the same public surface.
#include <it/d4np/memorypool/memory_pool.hpp>
#include <it/d4np/memorypool/typed_pool.hpp>

#include <cstdio>

int main() {
    using namespace it::d4np::memorypool;

    Pool pool(64, 16);
    void* block = pool.try_allocate();
    pool.deallocate(block);

    TypedPool<int> typed(8);
    int* value = typed.construct(7);
    const int got = *value;
    typed.destroy(value);

    std::printf("vcpkg smoke OK: got=%d block_size=%zu\n", got,
                pool.block_size());
    return got == 7 ? 0 : 1;
}
