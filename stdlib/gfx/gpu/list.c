/* The GPU's list (gpu/list.h). */
#include "gpu/list.h"
#include "mem.h"

enum {
    ARRAY_WORDS = 4, /* VERTICES and DISCS: the op, its count and pointer */
    DISCS_AT = 4,    /* DISCS's first word */
    DRAW_WORDS = 6,  /* the op, first, count, texture, blend, alpha */
    COUNT = 2,       /* a draw's count, its word */
};

/* VERTICES and DISCS, empty. */
static bool arrays(List *const l)
{
    const uint32_t none[] = {0, 0, 0};

    return list_op(l, QX_LIST_VERTICES, none, ARRAY_WORDS - 1) &&
           list_op(l, QX_LIST_DISCS, none, ARRAY_WORDS - 1);
}

static uint32_t head(const QxListOp op, const uint32_t words) { return op | words << QX_LIST_SIZE; }

bool list_init(List *const l)
{
    *l = (List){};

    return arrays(l);
}

void list_done(List *const l)
{
    list_reset(l);
    mem_free(l->words);
    mem_free(l->vertices);
    mem_free(l->discs);
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
    const QxListOp op,
    const uint32_t texture,
    const QxBlend blend,
    const uint8_t alpha
)
{
    const uint32_t *const d = l->words + l->draw;
    return l->draw && l->draw + DRAW_WORDS == l->n && (d[0] & 0xFF) == op && d[3] == texture &&
           d[4] == blend && d[5] == alpha;
}

/* n of size bytes each onto the array, *count of them, and drawn by op: false, no memory. */
static bool draw(
    List *const l,
    const QxListOp op,
    void **const array,
    uint32_t *const count,
    uint32_t *const cap,
    const uint32_t size,
    const void *const v,
    const uint32_t n,
    const uint32_t texture,
    const QxBlend blend,
    const uint8_t alpha
)
{
    if (!mem_grow(array, cap, *count + n, size))
        return false;

    const uint32_t first = *count;
    mem_copy((uint8_t *)*array + (size_t)first * size, v, (size_t)n * size);
    *count += n;

    if (joins(l, op, texture, blend, alpha)) {
        l->words[l->draw + COUNT] += n;
        return true;
    }

    const uint32_t at = l->n;

    if (!list_op(l, op, (const uint32_t[]){first, n, texture, blend, alpha}, DRAW_WORDS - 1)) {
        *count = first;
        return false;
    }

    l->draw = at;
    return true;
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
    return draw(
        l,
        QX_LIST_DRAW,
        (void **)&l->vertices,
        &l->nvertices,
        &l->capvertices,
        sizeof *v,
        v,
        n,
        texture,
        blend,
        alpha
    );
}

bool list_discs(
    List *const l,
    const QxDisc *const v,
    const uint32_t n,
    const uint32_t texture,
    const QxBlend blend,
    const uint8_t alpha
)
{
    return draw(
        l,
        QX_LIST_DRAW_DISCS,
        (void **)&l->discs,
        &l->ndiscs,
        &l->capdiscs,
        sizeof *v,
        v,
        n,
        texture,
        blend,
        alpha
    );
}

const uint32_t *list_end(List *const l, uint32_t *const n)
{
    if (!list_op(l, QX_LIST_END, nullptr, 0))
        return nullptr;

    l->words[1] = l->nvertices;
    list_pointer(l->words + 2, l->vertices);
    l->words[DISCS_AT + 1] = l->ndiscs;
    list_pointer(l->words + DISCS_AT + 2, l->discs);
    *n = l->n;
    return l->words;
}

void list_reset(List *const l)
{
    for (uint32_t i = 0; i < l->nkept; i++)
        mem_free(l->kept[i]);

    l->nkept = 0;
    l->nvertices = 0;
    l->ndiscs = 0;
    l->draw = 0;
    l->n = 0;

    if (l->words)
        arrays(l);
}
