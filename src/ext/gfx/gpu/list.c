/* The GPU's list (gpu/list.h). */
#include "gpu/list.h"
#include "mem.h"

enum {
    VERTICES_WORDS = 4, /* the op, its count and pointer */
    DRAW_WORDS = 6,     /* the op, first, count, texture, blend, alpha */
    COUNT = 2,          /* a draw's count, its word */
};

static uint32_t head(const QxListOp op, const uint32_t words) { return op | words << QX_LIST_SIZE; }

bool list_init(List *const l)
{
    *l = (List){};

    return list_op(l, QX_LIST_VERTICES, (const uint32_t[]){0, 0, 0}, VERTICES_WORDS - 1);
}

void list_done(List *const l)
{
    list_reset(l);
    mem_free(l->words);
    mem_free(l->vertices);
    mem_free(l->kept);
    *l = (List){};
}

bool list_op(List *const l, const QxListOp op, const uint32_t *const args, const uint32_t n)
{
    if (!mem_grow((void **)&l->words, &l->cap, l->n + 1 + n, sizeof *l->words))
        return false;

    l->words[l->n++] = head(op, 1 + n);

    for (uint32_t i = 0; i < n; i++)
        l->words[l->n++] = args[i];

    return true;
}

void list_pointer(uint32_t *const args, const void *const p)
{
    const uint64_t at = (uintptr_t)p;
    args[0] = (uint32_t)at;
    args[1] = (uint32_t)(at >> 32);
}

void *list_keep(List *const l, const uint32_t bytes)
{
    if (!mem_grow((void **)&l->kept, &l->capkept, l->nkept + 1, sizeof *l->kept))
        return nullptr;

    void *const p = mem_alloc(bytes ? bytes : 1);

    if (p)
        l->kept[l->nkept++] = p;

    return p;
}

/* The open draw's, and nothing since. */
static bool joins(
    const List *const l,
    const uint32_t texture,
    const QxBlend blend,
    const uint8_t alpha
)
{
    const uint32_t *const d = l->words + l->draw;
    return l->draw && l->draw + DRAW_WORDS == l->n && d[3] == texture && d[4] == blend &&
           d[5] == alpha;
}

bool list_draw(
    List *const l,
    const QxVertex *const v,
    const uint32_t n,
    const uint32_t texture,
    const QxBlend blend,
    const uint8_t alpha
)
{
    if (!mem_grow((void **)&l->vertices, &l->capvertices, l->nvertices + n, sizeof *v))
        return false;

    const uint32_t first = l->nvertices;
    mem_copy(l->vertices + first, v, (size_t)n * sizeof *v);
    l->nvertices += n;

    if (joins(l, texture, blend, alpha)) {
        l->words[l->draw + COUNT] += n;
        return true;
    }

    const uint32_t at = l->n;

    if (!list_op(l, QX_LIST_DRAW, (const uint32_t[]){first, n, texture, blend, alpha}, 5)) {
        l->nvertices = first;
        return false;
    }

    l->draw = at;
    return true;
}

const uint32_t *list_end(List *const l, uint32_t *const n)
{
    if (!list_op(l, QX_LIST_END, nullptr, 0))
        return nullptr;

    l->words[1] = l->nvertices;
    list_pointer(l->words + 2, l->vertices);
    *n = l->n;
    return l->words;
}

void list_reset(List *const l)
{
    for (uint32_t i = 0; i < l->nkept; i++)
        mem_free(l->kept[i]);

    l->nkept = 0;
    l->nvertices = 0;
    l->draw = 0;
    l->n = 0;

    if (l->words)
        list_op(l, QX_LIST_VERTICES, (const uint32_t[]){0, 0, 0}, VERTICES_WORDS - 1);
}
