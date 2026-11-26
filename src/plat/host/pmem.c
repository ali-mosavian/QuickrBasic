/* The platform's memory on the host.
 *
 * # How it works
 * One zeroed block, the heap's one region, or two for a test (pmem_split). Addr 0 stays unused,
 * as on DOS32.
 */
#include <stdlib.h>
#include "plat/pmem.h"
#include "util/heap.h"

enum : uint32_t { SIZE = 64u << 20, LOW = 0x1000 };

uint8_t *mem_base;
static Heap heap;
static size_t split; /* pmem_split's */

/* Tests': the next pmem_init's heap two regions, as DOS's under 1 MB and DPMI's: up to low, and
   the rest. */
void pmem_split(const size_t low) { split = low; }

bool pmem_init(void)
{
    mem_base = calloc(1, SIZE);

    if (!mem_base)
        return false;

    const size_t from = split ? split : LOW;
    heap_init(&heap);
    return (!split || heap_add(&heap, mem_base + LOW, split - LOW)) &&
           heap_add(&heap, mem_base + from, SIZE - from);
}

/* Null before pmem_init, as DOS32's. */
void *pmem_resize(void *const p, const size_t size) { return mem_base ? heap_resize(&heap, p, size) : nullptr; }

void pmem_free(void *const p) { heap_free(&heap, p); }

void *pmem_bottom(const size_t size) { return heap_bottom(&heap, size); }

bool pmem_extend(void *const p, const size_t size) { return heap_extend(&heap, p, size); }

size_t pmem_after(const void *const p) { return heap_after(&heap, p); }

size_t pmem_left(const void *const below) { return heap_left(&heap, below); }
