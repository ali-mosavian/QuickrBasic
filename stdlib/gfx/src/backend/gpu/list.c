/* The GPU's list (backend/gpu/list.h). */
#include "backend/gpu/list.h"
#include "core/mem.h"

enum {
    BUFFER_WORDS = 4, /* BUFFER: the op, id, bytes, kind */
    WRITE_WORDS = 5,  /* WRITE: the op, id, bytes, pointer */
    ARRAY_WORDS = BUFFER_WORDS + WRITE_WORDS,
    DRAW_WORDS = 7 + LIST_TEXTURES, /* the op, pipeline, vertices, indices, first, count, alpha */
    COUNT = 5,                      /* a draw's count, its word */
    QUAD = 4,
    QUAD_INDICES = 6,
};

/* The buffers' BUFFER and WRITE, empty till the list ends. */
static bool arrays(GpuList *const l)
{
    const uint32_t none[] = {0, 0, 0, 0};

    bool made = true;

    for (uint32_t b = LIST_VERTICES; b <= LIST_INDICES; b++)
        made = made && list_op(l, QX_LIST_BUFFER, none, BUFFER_WORDS - 1) &&
               list_op(l, QX_LIST_WRITE, none, WRITE_WORDS - 1);

    return made;
}

static uint32_t head(const QxListOp op, const uint32_t words) { return op | words << QX_LIST_SIZE; }

bool list_init(GpuList *const l)
{
    *l = (GpuList){};

    return arrays(l);
}

void list_done(GpuList *const l)
{
    list_reset(l);
    mem_free(l->words);
    mem_free(l->vertices);
    mem_free(l->discs);
    mem_free(l->kept);
    *l = (GpuList){};
}

bool list_op(GpuList *const l, const QxListOp op, const uint32_t *const args, const uint32_t n)
{
    if (!mem_grow((void **)&l->words, &l->cap, l->n + 1 + n, sizeof *l->words))
        return false;

    l->words[l->n++] = head(op, 1 + n);
    l->batches += op == QX_LIST_DRAW;
    l->uploads += op == QX_LIST_UPLOAD;

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

void *list_keep(GpuList *const l, const uint32_t bytes)
{
    if (!mem_grow((void **)&l->kept, &l->capkept, l->nkept + 1, sizeof *l->kept))
        return nullptr;

    void *const p = mem_alloc(bytes ? bytes : 1);

    if (p)
        l->kept[l->nkept++] = p;

    return p;
}

/* The draw's words from its pipeline on. */
static void drawn(
    uint32_t *const d,
    const uint32_t pipeline,
    const uint32_t buffer,
    const uint32_t first,
    const uint32_t n,
    const uint32_t *const textures,
    const uint8_t alpha
)
{
    const uint32_t w[DRAW_WORDS - 1] = {
        pipeline,
        buffer,
        LIST_INDICES,
        first / QUAD * QUAD_INDICES,
        n / QUAD * QUAD_INDICES,
        alpha,
        textures[0],
        textures[1],
        textures[2],
    };

    mem_copy(d, w, sizeof w);
}

/* n of size bytes each onto the array, *count of them, and drawn from buffer: false, no memory. */
static bool draw(
    GpuList *const l,
    const uint32_t buffer,
    void **const array,
    uint32_t *const count,
    uint32_t *const cap,
    const uint32_t size,
    const void *const v,
    const uint32_t n,
    const uint32_t pipeline,
    const uint32_t *const textures,
    const uint8_t alpha,
    const bool any
)
{
    if (!mem_grow(array, cap, *count + n, size))
        return false;

    const uint32_t first = *count;
    uint32_t want[DRAW_WORDS - 1];

    mem_copy((uint8_t *)*array + (size_t)first * size, v, (size_t)n * size);
    *count += n;
    drawn(want, pipeline, buffer, 0, 0, textures, alpha);

    uint32_t *const d = l->words + l->draw;
    const bool open = l->draw && l->draw + DRAW_WORDS == l->n && d[1] == want[0] &&
                      d[2] == want[1] && d[6] == want[5] && d[8] == want[7] && d[9] == want[8];

    if (open && (d[7] == want[6] || any || l->any)) {
        if (!any)
            d[7] = want[6];

        l->any = l->any && any;
        d[COUNT] += n / QUAD * QUAD_INDICES;
        return true;
    }

    const uint32_t at = l->n;

    drawn(want, pipeline, buffer, first, n, textures, alpha);

    if (!list_op(l, QX_LIST_DRAW, want, DRAW_WORDS - 1)) {
        *count = first;
        return false;
    }

    l->draw = at;
    l->any = any;
    return true;
}

bool list_draw(
    GpuList *const l,
    const GpuVertex *const v,
    const uint32_t n,
    const uint32_t pipeline,
    const uint32_t textures[LIST_TEXTURES],
    const uint8_t alpha,
    const bool any
)
{
    return draw(
        l,
        LIST_VERTICES,
        (void **)&l->vertices,
        &l->nvertices,
        &l->capvertices,
        sizeof *v,
        v,
        n,
        pipeline,
        textures,
        alpha,
        any
    );
}

bool list_discs(
    GpuList *const l,
    const GpuDisc *const v,
    const uint32_t n,
    const uint32_t pipeline,
    const uint32_t textures[LIST_TEXTURES],
    const uint8_t alpha,
    const bool any
)
{
    return draw(
        l,
        LIST_DISCS,
        (void **)&l->discs,
        &l->ndiscs,
        &l->capdiscs,
        sizeof *v,
        v,
        n,
        pipeline,
        textures,
        alpha,
        any
    );
}

/* Buffer b's ops: made bytes of kind, its first wrote of them written from p. */
static void array(
    GpuList *const l,
    const uint32_t b,
    const uint32_t bytes,
    const QxBuffer kind,
    const uint32_t wrote,
    const void *const p
)
{
    uint32_t *const w = l->words + (b - LIST_VERTICES) * ARRAY_WORDS;
    w[1] = b;
    w[2] = bytes;
    w[3] = kind;
    w[BUFFER_WORDS + 1] = b;
    w[BUFFER_WORDS + 2] = wrote;
    list_pointer(w + BUFFER_WORDS + 3, p);
}

/* The indices of twice the quads drawn, when more than they cover: false, no memory. */
static bool indices(GpuList *const l)
{
    static const uint32_t corners[QUAD_INDICES] = {0, 1, 2, 0, 2, 3};

    const uint32_t drawn = (l->nvertices > l->ndiscs ? l->nvertices : l->ndiscs) / QUAD;
    const uint32_t quads = drawn * 2;
    const uint32_t bytes = quads * QUAD_INDICES * sizeof(uint32_t);

    if (drawn <= l->quads) {
        array(
            l,
            LIST_INDICES,
            l->quads * QUAD_INDICES * sizeof(uint32_t),
            QX_BUFFER_INDEX,
            0,
            nullptr
        );
        return true;
    }

    uint32_t *const ix = list_keep(l, bytes);

    if (!ix)
        return false;

    for (uint32_t q = 0; q < quads; q++)
        for (uint32_t i = 0; i < QUAD_INDICES; i++)
            ix[q * QUAD_INDICES + i] = q * QUAD + corners[i];

    l->quads = quads;
    array(l, LIST_INDICES, bytes, QX_BUFFER_INDEX, bytes, ix);
    return true;
}

const uint32_t *list_end(GpuList *const l, uint32_t *const n)
{
    const uint32_t plain = l->nvertices * sizeof *l->vertices;
    const uint32_t round = l->ndiscs * sizeof *l->discs;

    if (!indices(l) || !list_op(l, QX_LIST_END, nullptr, 0))
        return nullptr;

    array(l, LIST_VERTICES, plain, QX_BUFFER_VERTEX, plain, l->vertices);
    array(l, LIST_DISCS, round, QX_BUFFER_VERTEX, round, l->discs);
    *n = l->n;
    return l->words;
}

void list_reset(GpuList *const l)
{
    for (uint32_t i = 0; i < l->nkept; i++)
        mem_free(l->kept[i]);

    l->nkept = 0;
    l->nvertices = 0;
    l->ndiscs = 0;
    l->draw = 0;
    l->any = false;
    l->n = 0;
    l->batches = 0;
    l->uploads = 0;

    if (l->words)
        arrays(l);
}
