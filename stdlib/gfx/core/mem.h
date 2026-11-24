/* gfx's memory: the host's, through QxHost (a slice has no C library); copies and fills. */
#ifndef GFX_MEM_H
#define GFX_MEM_H
#include <stddef.h>
#include <stdint.h>

/* bytes zeroed: null, none left. */
void *mem_alloc(uint32_t bytes);
void mem_free(void *p);
/* *items grown to hold need of size bytes each, *cap counting them: false, no memory (*items as
   it was). */
bool mem_grow(void **items, uint32_t *cap, uint32_t need, uint32_t size);

static inline void mem_copy(void *const d, const void *const s, const size_t n)
{
    __builtin_memcpy(d, s, n);
}

static inline void mem_fill(void *const d, const uint8_t v, const size_t n)
{
    __builtin_memset(d, v, n);
}
#endif
