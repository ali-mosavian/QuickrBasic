/* Growable buffers on the C side's heap (plat/pmem.h). */
#ifndef UTIL_GROW_H
#define UTIL_GROW_H
#include <stdint.h>
#include <string.h>
#include "plat/pmem.h"

enum { GROW_LEAST = 64 }; /* a buffer's first items at least */

/* *items, a T * of *cap items of size bytes, holds n at least: grown by half again, moved maybe,
   the new items zeroed. false: no memory, unchanged. */
static inline bool grow_room(
    void *const items,
    uint32_t *const cap,
    const uint32_t n,
    const uint32_t size
)
{
    if (n <= *cap)
        return true;

    const uint32_t half_again = *cap + *cap / 2;
    uint32_t want = half_again > n ? half_again : n;
    want = want < GROW_LEAST ? GROW_LEAST : want;

    void *old;
    memcpy(&old, items, sizeof old);
    uint8_t *const p = pmem_resize(old, (size_t)want * size);

    if (!p)
        return false;

    memset(p + (size_t)*cap * size, 0, (size_t)(want - *cap) * size);
    memcpy(items, &p, sizeof p);
    *cap = want;
    return true;
}

/* grow_room for p, a T *, and its cap. */
#define GROW(p, cap, n) grow_room(&(p), &(cap), (n), sizeof *(p))
#endif
