// Minimal consumer for the Conan test_package: construct a pool, allocate and
// free a block, and exercise the typed surface — proving the package links.
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

    std::printf("conan test_package OK: got=%d block_size=%zu\n", got, pool.block_size());
    return got == 7 ? 0 : 1;
}
