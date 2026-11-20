/* Tables of fixed-size entries in blocks from an arena.
 *
 * # How it works
 * An entry's id is its block's number shifted left BTAB_SHIFT bits, ORed with its place in the
 * block: 1024 entries a block, 2^22 blocks. A full table takes another block and never moves an
 * entry, so a pointer to one stays good until the arena is reset; only the blocks' list grows,
 * doubling. Dropping the last entries (n lowered) keeps their room for the next adds.
 */
#include "util/btab.h"
#include <string.h>

void *btab_add(BTab *const t, Arena *const a)
{
    const uint32_t k = t->n >> BTAB_SHIFT;

    if (k == t->nblocks) {
        if (k == t->cap) {
            const uint32_t cap = t->cap ? t->cap * 2 : 8;
            uint8_t **const list =
                arena_grow(a, t->blocks, t->cap * sizeof *list, cap * sizeof *list);

            if (!list)
                return nullptr;

            t->blocks = list;
            t->cap = cap;
        }

        uint8_t *const b = arena_alloc(a, (size_t)BTAB_N * t->size);

        if (!b)
            return nullptr;

        t->blocks[t->nblocks++] = b;
    }

    void *const e = btab_at(t, t->n++);
    memset(e, 0, t->size);
    return e;
}
