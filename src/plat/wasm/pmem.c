/* The platform's memory on wasm.
 *
 * # How it works
 * Linear memory grown past what the module has: the heap's one region. Addr 0 stays unused, as on
 * DOS32.
 */
#include "plat/pmem.h"
#include "util/heap.h"

enum : uint32_t { PAGE = 65536, SIZE = 96u << 20, LOW = 0x1000 };

uint8_t *mem_base;
static Heap heap;

bool pmem_init(void)
{
    const intptr_t first = __builtin_wasm_memory_grow(0, SIZE / PAGE);

    if (first < 0)
        return false;

    mem_base = (uint8_t *)(first * PAGE);
    heap_init(&heap);
    return heap_add(&heap, mem_base + LOW, SIZE - LOW);
}

/* Null before pmem_init, as DOS32's. */
void *pmem_resize(void *const p, const size_t size) { return mem_base ? heap_resize(&heap, p, size) : nullptr; }

void pmem_free(void *const p) { heap_free(&heap, p); }

void *pmem_bottom(const size_t size) { return heap_bottom(&heap, size); }

bool pmem_extend(void *const p, const size_t size) { return heap_extend(&heap, p, size); }

size_t pmem_after(const void *const p) { return heap_after(&heap, p); }

size_t pmem_left(const void *const below) { return heap_left(&heap, below); }
