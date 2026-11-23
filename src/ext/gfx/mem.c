/* gfx's memory: QxHost's alloc and free (mem.h). */
#include "mem.h"
#include "gfx_qx.h"

void *mem_alloc(const uint32_t bytes) { return qx_host->alloc(bytes); }

void mem_free(void *const p)
{
    if (p)
        qx_host->free(p);
}

bool mem_grow(void **const items, uint32_t *const cap, const uint32_t need, const uint32_t size)
{
    if (need <= *cap)
        return true;

    const uint32_t more = need > *cap * 2 ? need : *cap * 2;
    void *const grown = mem_alloc(more * size);

    if (!grown)
        return false;

    if (*items)
        mem_copy(grown, *items, (size_t)*cap * size);

    mem_free(*items);
    *items = grown;
    *cap = more;
    return true;
}
