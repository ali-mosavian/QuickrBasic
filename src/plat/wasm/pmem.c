/* The arena on wasm.
 *
 * # How it works
 * Linear memory grown past what the module has: the arena, then the C side's heap (util/heap.h) at
 * its top. Addr 0 stays unused, as on DOS32.
 */
#include "plat/pmem.h"
#include "util/heap.h"

enum : uint32_t { PAGE = 65536, ARENA_SIZE = 64u << 20, ARENA_LOW = 0x1000, C_HEAP = 32u << 20 };

uint8_t *mem_base;
static Heap c_heap;

bool pmem_init(const uint32_t least, const uint32_t heap, Addr *const lo, Addr *const hi)
{
    if (ARENA_SIZE - ARENA_LOW < least)
        return false;

    const uint32_t pages = (ARENA_SIZE + C_HEAP) / PAGE;
    const intptr_t first = __builtin_wasm_memory_grow(0, pages);

    if (first < 0)
        return false;

    mem_base = (uint8_t *)(first * PAGE);
    heap_init(&c_heap);
    heap_add(&c_heap, mem_base + ARENA_SIZE, C_HEAP);
    *lo = ARENA_LOW;
    *hi = ARENA_SIZE;
    return true;
}

/* Null before pmem_init, as DOS32's heap is. */
void *pmem_resize(void *const p, const size_t size)
{
    return mem_base ? heap_resize(&c_heap, p, size) : nullptr;
}

void pmem_free(void *const p) { heap_free(&c_heap, p); }
