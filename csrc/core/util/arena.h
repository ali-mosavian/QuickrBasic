/* The bump allocator's interface. How it works: arena.c. */
#ifndef UTIL_ARENA_H
#define UTIL_ARENA_H
#include <stddef.h>
#include <stdint.h>

typedef struct ArenaBlock ArenaBlock;

typedef struct {
    ArenaBlock *first; /* its blocks, in the order taken */
    ArenaBlock *cur;   /* the one allocations come from */
    uint8_t *top;      /* the next free byte in cur */
    size_t chunk;      /* a new block's least size */
} Arena;

void arena_init(Arena *a, size_t chunk);  /* empty: no block taken yet */
void *arena_alloc(Arena *a, size_t size); /* zeroed, aligned for any object; null: no memory left */
/* old's size bytes as newsize (larger), the new ones zeroed: in place when old is the last
   allocation and there is room, else a copy. Null: no memory left, old kept. */
void *arena_grow(Arena *a, void *old, size_t size, size_t newsize);
void arena_reset(Arena *a); /* every allocation freed */
#endif
