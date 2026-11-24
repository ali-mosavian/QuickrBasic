/* A GPU's recorder (backend/gpu/gpu.h). */
#include "backend/gpu/gpu.h"
#include "gfx_qx.h"
#include "backend/gpu/shaders.h"
#include "core/mem.h"

enum {
    RGBA = 4,
    HALF = 0x80,
    OPAQUE = 0xFF,
    BLENDS = 4,
    DEPTHS = (QX_DEPTH_TEST | QX_DEPTH_WRITE) + 1,
    PLAIN_ATTRIBUTES = 5,
    DISC_ATTRIBUTES = 7,
};

/* A box none. */
static constexpr GpuBox NONE = {0, 0, 0, 0};

/* The vertices of list_draw and of list_discs. */
typedef enum { PLAIN, ROUND, KINDS } Kind;

static_assert(offsetof(GpuDisc, v) == 0, "a disc's vertex is a plain one's attributes");

/* The pipeline for vertices of kind k with blend and depth (QxDepth): 1 up, a bit of Gpu.made
   each. */
static uint32_t pipeline_of(const Kind k, const QxBlend blend, const uint32_t depth)
{
    return 1 + (k * BLENDS + blend) * DEPTHS + depth;
}

static_assert(KINDS * BLENDS * DEPTHS <= 32, "a pipeline a bit of Gpu.made");

/* Root h's depth texture. */
static uint32_t depth_of(const uint32_t h) { return h | (uint32_t)GPU_DEPTH << HDL_KIND_SHIFT; }

/* texture's entry: null, no memory. */
static GpuTexture *entry(Gpu *const g, const uint32_t texture)
{
    if (texture <= PIC_FONT)
        return &g->own[texture];

    const uint32_t slot = hdl_slot(texture), had = g->ntextures;

    if (!mem_grow((void **)&g->textures, &g->ntextures, slot + 1, sizeof *g->textures))
        return nullptr;

    mem_fill(g->textures + had, 0, (g->ntextures - had) * sizeof *g->textures);
    return &g->textures[slot];
}

static uint32_t load(const Pixels *const px, const int32_t x, const int32_t y)
{
    uint32_t v = 0;
    mem_copy(&v, px->p + y * px->pitch + x * px_bytes(px->format), px_bytes(px->format));
    return v;
}

static void store(const Pixels *const px, const int32_t x, const int32_t y, const uint32_t v)
{
    mem_copy(px->p + y * px->pitch + x * px_bytes(px->format), &v, px_bytes(px->format));
}

/* A texel as ARGB, a keyed one's alpha 0; an index, itself in every channel. */
static uint32_t argb(const PxFormat f, const uint32_t v)
{
    if (f == PX_8)
        return v * 0x01010101u;

    if (f == PX_32)
        return v;

    const uint32_t c = px_argb(f, v);
    return px_key(f, v) ? c & PX_WHITE : c;
}

/* px's texels in format, in the list's kept memory, *pitch bytes a row: null, no memory. */
static const uint8_t *texels(
    GpuList *const l,
    const Pixels *const px,
    const QxTextureFormat format,
    uint32_t *const pitch
)
{
    const bool index = format == QX_TEXTURE_INDEX;
    *pitch = (uint32_t)px->w * (index ? 1 : RGBA);
    uint8_t *const out = list_keep(l, *pitch * (uint32_t)px->h);

    for (int32_t y = 0; out && y < px->h; y++)
        for (int32_t x = 0; x < px->w; x++) {
            uint8_t *const o = out + y * *pitch + x * (index ? 1 : RGBA);
            const uint32_t v = load(px, x, y);
            const uint32_t c = argb(px->format, v);

            if (index)
                o[0] = (uint8_t)v;
            else
                mem_copy(o, (const uint8_t[]){c >> 16, c >> 8, c, c >> 24}, RGBA);
        }

    return out;
}

/* A language's code: each kind's vertex stage, the fragment stage. */
typedef struct {
    const void *vertex[KINDS], *fragment;
    uint32_t vertex_bytes[KINDS], fragment_bytes;
} Code;

/* A language's code, its text's NUL left out (text 1) or its bytes whole (0). */
#define CODE(language, text)                                                                       \
    {                                                                                              \
        {draw_vert_##language, disc_vert_##language},                                              \
        draw_frag_##language,                                                                      \
        {sizeof draw_vert_##language - (text), sizeof disc_vert_##language - (text)},              \
        sizeof draw_frag_##language - (text),                                                      \
    }

static const Code codes[] = {
    [QX_SHADER_MSL] = CODE(msl, 1),
    [QX_SHADER_ESSL] = CODE(essl, 1),
    [QX_SHADER_SPIRV] = CODE(spirv, 0),
};

/* The pipeline for vertices of kind k with blend and depth, made if not yet: false, no memory. */
static bool pipeline(Gpu *const g, const Kind k, const QxBlend blend, const uint32_t depth)
{
    /* PIPELINE's words: its code's, its first attribute's, an attribute's */
    enum { VERTEX_CODE = 4, FRAGMENT_CODE = 7, HEAD = 10, ATTRIBUTE = 3 };

    static const uint32_t attributes[DISC_ATTRIBUTES][ATTRIBUTE] = {
        {0, QX_ATTRIBUTE_FLOAT4, offsetof(GpuVertex, x)},
        {1, QX_ATTRIBUTE_FLOAT2, offsetof(GpuVertex, u)},
        {2, QX_ATTRIBUTE_USHORT4, offsetof(GpuVertex, rx)},
        {3, QX_ATTRIBUTE_UBYTE4, offsetof(GpuVertex, r)},
        {4, QX_ATTRIBUTE_UBYTE4, offsetof(GpuVertex, row)},
        {5, QX_ATTRIBUTE_FLOAT4, offsetof(GpuDisc, disc)},
        {6, QX_ATTRIBUTE_FLOAT4, offsetof(GpuDisc, sweep)},
    };

    static const struct {
        uint32_t stride, attributes;
    } kinds[KINDS] = {
        {sizeof(GpuVertex), PLAIN_ATTRIBUTES},
        {sizeof(GpuDisc), DISC_ATTRIBUTES},
    };

    const uint32_t id = pipeline_of(k, blend, depth), bit = 1u << (id - 1);
    const Code *const c = &codes[g->shader];
    uint32_t args[HEAD + sizeof attributes / sizeof(uint32_t)] = {
        id,
        blend | depth << 8,
        kinds[k].stride,
        LIST_TEXTURES,
    };

    if (g->made & bit)
        return true;

    args[VERTEX_CODE] = c->vertex_bytes[k];
    args[FRAGMENT_CODE] = c->fragment_bytes;
    list_pointer(args + VERTEX_CODE + 1, c->vertex[k]);
    list_pointer(args + FRAGMENT_CODE + 1, c->fragment);
    mem_copy(args + HEAD, attributes, kinds[k].attributes * sizeof *attributes);

    if (!list_op(&g->list, QX_LIST_PIPELINE, args, HEAD + kinds[k].attributes * ATTRIBUTE))
        return false;

    g->made |= bit;
    return true;
}

bool gpu_open(Gpu *const g, const QxScreen *const s, const bool eight)
{
    mem_fill(g->textures, 0, g->ntextures * sizeof *g->textures);

    for (uint32_t i = 0; i <= PIC_FONT; i++)
        g->own[i] = (GpuTexture){};

    g->npages = 0;

    g->passing = false;
    g->behind = 0;
    g->behind_w = g->behind_h = 0;
    g->ndirty = 0;
    g->call = NONE;
    g->palette = 0;
    g->mapped = 0;
    g->eight = eight;
    g->screen_w = (int32_t)s->w;
    g->screen_h = (int32_t)s->h;
    g->white = 0xFFFFFFFF;
    g->down = s->y_down ? 1 : -1;
    g->count = (GpuCount){};

    g->shader = s->shader;
    g->made = 0;

    GpuTexture *const canvas = entry(g, SURF_SCREEN);
    const uint32_t made[][4] = {
        {SURF_SCREEN, s->w, s->h, QX_TEXTURE_RGBA},
        {GPU_PALETTE, PAL_COLOURS, GPU_PALETTE_ROWS, QX_TEXTURE_RGBA},
        {GPU_SHOWN, s->w, s->h, QX_TEXTURE_RGBA},
    };

    const Code *const code = s->shader < sizeof codes / sizeof *codes ? &codes[s->shader] : nullptr;

    if (!canvas || !code || !code->fragment || !list_init(&g->list) ||
        !list_op(&g->list, QX_LIST_TEXTURE, made[0], 4) ||
        !list_op(&g->list, QX_LIST_TEXTURE, made[1], 4) ||
        (eight && !list_op(&g->list, QX_LIST_TEXTURE, made[2], 4)))
        return false;

    *canvas = (GpuTexture){.handle = SURF_SCREEN, .format = QX_TEXTURE_RGBA};

    return true;
}

void gpu_close(Gpu *const g)
{
    list_done(&g->list);
    mem_free(g->textures);
    mem_free(g->pages);
    g->textures = nullptr;
    g->ntextures = 0;
    g->pages = nullptr;
    g->npages = g->cappages = 0;
}

/* e at the page's next place, a row begun when it would not fit the last: false, no room. */
static bool fit(GpuPage *const page, GpuTexture *const e, const uint16_t w, const uint16_t h)
{
    if (page->x + w > GPU_PAGE) {
        page->x = 0;
        page->y += page->row;
        page->row = 0;
    }

    if (page->y + h > GPU_PAGE)
        return false;

    e->x = page->x;
    e->y = page->y;
    page->x += w;
    page->row = h > page->row ? h : page->row;
    page->live++;
    return true;
}

/* e, w by h, packed into a page of format, one made when none has room: false, no memory. */
static bool pack(
    Gpu *const g,
    GpuTexture *const e,
    const QxTextureFormat format,
    const uint16_t w,
    const uint16_t h
)
{
    for (uint32_t i = 0; i < g->npages; i++)
        if (g->pages[i].format == format && fit(&g->pages[i], e, w, h)) {
            e->page = GPU_PAGE_FIRST + i;
            return true;
        }

    const uint32_t made[] = {GPU_PAGE_FIRST + g->npages, GPU_PAGE, GPU_PAGE, format};

    if (!mem_grow((void **)&g->pages, &g->cappages, g->npages + 1, sizeof *g->pages) ||
        !list_op(&g->list, QX_LIST_TEXTURE, made, 4))
        return false;

    GpuPage *const page = &g->pages[g->npages++];
    *page = (GpuPage){.format = format};

    e->page = made[0];
    return fit(page, e, w, h);
}

/* e out of its page, which starts over when it empties. */
static void unpack(Gpu *const g, GpuTexture *const e)
{
    GpuPage *const page = &g->pages[e->page - GPU_PAGE_FIRST];

    if (!--page->live)
        *page = (GpuPage){.format = page->format};

    e->page = 0;
}

bool gpu_texture(Gpu *const g, const Picture *const p, const bool into, GpuSource *const out)
{
    GpuTexture *const e = entry(g, p->texture);
    const uint32_t name = p->texture;
    const Pixels *const px = &p->root;
    const QxTextureFormat want = into || px->format != PX_8 ? QX_TEXTURE_RGBA : QX_TEXTURE_INDEX;

    if (!e)
        return false;

    if (e->handle != p->texture)
        *e = (GpuTexture){.handle = p->texture, .bottom = px->h};

    if (e->page && into) {
        unpack(g, e);
        e->format = 0;
    }

    if (!e->format && !into && px->w <= GPU_PAGE / 2 && px->h <= GPU_PAGE / 2) {
        if (!pack(g, e, want, (uint16_t)px->w, (uint16_t)px->h))
            return false;

        e->format = want;
        e->version = ~p->version; /* the place may hold another's */
        e->top = 0;
        e->bottom = px->h;
    } else if (!e->format || (want == QX_TEXTURE_RGBA && e->format == QX_TEXTURE_INDEX)) {
        const bool had = e->format;
        const uint32_t args[] = {name, (uint32_t)px->w, (uint32_t)px->h, want};

        if (had && !list_op(&g->list, QX_LIST_FREE, &name, 1))
            return false;

        if (!list_op(&g->list, QX_LIST_TEXTURE, args, 4))
            return false;

        e->format = want;
        e->version = had ? ~p->version : 0;
        e->top = 0;
        e->bottom = px->h;
    }

    const uint32_t id = e->page ? e->page : name;
    const int32_t top = e->top, h = e->bottom - e->top;

    if (e->version != p->version && h > 0) {
        const Pixels cut = px_cut(px, (Rect){0, top, px->w, h});

        uint32_t pitch, args[8] = {id, e->x, e->y + (uint32_t)top, (uint32_t)px->w, (uint32_t)h};

        const uint8_t *const bytes = texels(&g->list, &cut, e->format, &pitch);

        if (!bytes)
            return false;

        args[5] = pitch;
        list_pointer(args + 6, bytes);

        if (!list_op(&g->list, QX_LIST_UPLOAD, args, 8))
            return false;

        g->behind = id == g->behind ? 0 : g->behind;
    }

    if (e->version != p->version)
        e->top = e->bottom = 0;

    e->version = p->version;
    e->drawn = e->drawn || into;

    *out = (GpuSource){id, e->x, e->y, px->format == PX_8, e->depth ? depth_of(name) : 0};

    return true;
}

bool gpu_depth(Gpu *const g, const Picture *const p, uint32_t *const id, bool *const fresh)
{
    GpuTexture *const e = entry(g, p->texture);
    const uint32_t made[] =
        {depth_of(p->texture), (uint32_t)p->root.w, (uint32_t)p->root.h, QX_TEXTURE_DEPTH};

    if (!e)
        return false;

    *id = made[0];
    *fresh = !e->depth;

    if (e->depth)
        return true;

    e->depth = list_op(&g->list, QX_LIST_TEXTURE, made, 4);
    return e->depth;
}

static bool same(const Rect a, const Rect b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static GpuBox joined(const GpuBox a, const GpuBox b)
{
    if (!(a.x0 < a.x1))
        return b;

    if (!(b.x0 < b.x1))
        return a;

    return (GpuBox){a.x0 < b.x0 ? a.x0 : b.x0,
                    a.y0 < b.y0 ? a.y0 : b.y0,
                    a.x1 > b.x1 ? a.x1 : b.x1,
                    a.y1 > b.y1 ? a.y1 : b.y1};
}

static bool overlap(const GpuBox a, const GpuBox b)
{
    return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

/* This call's box among what the copy of behind lacks, when the pass is what it copies. */
static void called(Gpu *const g)
{
    if (g->passing && g->pass == g->behind && g->call.x0 < g->call.x1) {
        if (g->ndirty < GPU_DIRTY)
            g->dirty[g->ndirty] = g->call;

        g->ndirty++;
    }

    g->call = NONE;
}

/* The copy of behind lacks some of b. */
static bool stale(const Gpu *const g, const GpuBox b)
{
    if (g->behind != g->pass || g->ndirty > GPU_DIRTY)
        return true;

    for (uint32_t i = 0; i < g->ndirty; i++)
        if (overlap(b, g->dirty[i]))
            return true;

    return false;
}

/* gpu_target's pass and clip. */
static bool aim(
    Gpu *const g,
    const uint32_t id,
    const uint32_t deep,
    const int32_t w,
    const int32_t h,
    const Rect clip
)
{
    if (!g->passing || g->pass != id || g->deep != deep) {
        float *const size = list_keep(&g->list, 4 * sizeof(float));
        const uint32_t pass[] = {id, deep};

        uint32_t args[3] = {4 * sizeof(float)};

        if (!size || !list_op(&g->list, QX_LIST_PASS, pass, deep ? 2 : 1))
            return false;

        size[0] = (float)w;
        size[1] = (float)h;
        size[2] = g->down;
        size[3] = 0;
        list_pointer(args + 1, size);

        if (!list_op(&g->list, QX_LIST_UNIFORMS, args, 3))
            return false;

        g->passing = true;
        g->pass = id;
        g->deep = deep;
        g->w = w;
        g->h = h;
        g->clip = (Rect){.w = -1};
    }

    if (same(clip, g->clip))
        return true;

    const uint32_t args[] =
        {(uint32_t)clip.x, (uint32_t)clip.y, (uint32_t)clip.w, (uint32_t)clip.h};

    g->clip = clip;
    return list_op(&g->list, QX_LIST_CLIP, args, 4);
}

bool gpu_target(
    Gpu *const g,
    const uint32_t id,
    const uint32_t deep,
    const int32_t w,
    const int32_t h,
    const Rect clip
)
{
    called(g);
    return aim(g, id, deep, w, h, clip);
}

/* The palette's rows sent: false, no memory. */
static bool send_rows(Gpu *const g, const Palette *const pal)
{
    uint8_t *const rows = list_keep(&g->list, PAL_ROWS * PAL_COLOURS * RGBA);
    uint32_t args[8] = {GPU_PALETTE, 0, 0, PAL_COLOURS, PAL_ROWS, PAL_COLOURS * RGBA};

    if (!rows)
        return false;

    for (uint32_t r = 0; r < PAL_ROWS; r++)
        for (uint32_t i = 0; i < PAL_COLOURS; i++) {
            const uint32_t c = pal->rows[r][i];
            mem_copy(
                rows + (r * PAL_COLOURS + i) * RGBA,
                (const uint8_t[]){c >> 16, c >> 8, c, OPAQUE},
                RGBA
            );
        }

    list_pointer(args + 6, rows);
    return list_op(&g->list, QX_LIST_UPLOAD, args, 8);
}

/* Row 0's map sent, below the rows: false, no memory. */
static bool send_map(Gpu *const g, Palette *const pal)
{
    const uint8_t *const map = pal_map(pal);
    uint8_t *const cells = map ? list_keep(&g->list, PAL_CELLS) : nullptr;
    uint32_t args[8] = {GPU_PALETTE, 0, PAL_ROWS, PAL_COLOURS, GPU_MAP_ROWS, PAL_COLOURS * RGBA};

    if (!cells)
        return false;

    mem_copy(cells, map, PAL_CELLS);
    list_pointer(args + 6, cells);
    return list_op(&g->list, QX_LIST_UPLOAD, args, 8);
}

bool gpu_palette(Gpu *const g, Palette *const pal, const bool map)
{
    const uint32_t row0 = pal->changed[0] + 1;
    uint32_t changes = 1;

    for (uint32_t r = 0; r < PAL_ROWS; r++)
        changes += pal->changed[r];

    if (changes != g->palette && !send_rows(g, pal))
        return false;

    g->palette = changes;

    if (!map || row0 == g->mapped)
        return true;

    g->mapped = send_map(g, pal) ? row0 : 0;
    return g->mapped;
}

/* v's colour is the tint's alone, never keyed: its texel does not matter. */
static bool any(const GpuVertex *const v)
{
    return (v->stages & (GPU_STAGE_SOLID | GPU_STAGE_KEYED)) == GPU_STAGE_SOLID;
}

/* The box of n vertices, stride bytes apart. */
static GpuBox box_of(const void *const v, const uint32_t stride, const uint32_t n)
{
    GpuBox b = {1e30f, 1e30f, -1e30f, -1e30f};

    for (uint32_t i = 0; i < n; i++) {
        const GpuVertex *const p = (const GpuVertex *)((const uint8_t *)v + i * stride);
        b.x0 = p->x < b.x0 ? p->x : b.x0;
        b.y0 = p->y < b.y0 ? p->y : b.y0;
        b.x1 = p->x > b.x1 ? p->x : b.x1;
        b.y1 = p->y > b.y1 ? p->y : b.y1;
    }

    return b;
}

/* The box b of texture id, w by h, drawn in the pass with stages: false, no memory. */
static bool copy_box(
    Gpu *const g,
    const uint32_t id,
    const int32_t w,
    const int32_t h,
    const GpuBox b,
    const uint16_t stages
)
{
    const GpuVertex corner = {
        .w = 1,
        .rw = (uint16_t)w,
        .rh = (uint16_t)h,
        .r = 255,
        .g = 255,
        .b = 255,
        .light = PAL_FULL,
        .stages = stages,
    };

    const uint32_t textures[LIST_TEXTURES] = {id, GPU_PALETTE, GPU_PALETTE};

    GpuVertex v[4] = {corner, corner, corner, corner};

    for (uint32_t i = 0; i < 4; i++) {
        const bool right = i == 1 || i == 2, low = i >= 2;
        v[i].x = right ? b.x1 : b.x0;
        v[i].y = low ? b.y1 : b.y0;
        v[i].u = v[i].x + 0.5f;
        v[i].v = v[i].y + 0.5f;
    }

    return list_draw(&g->list, v, 4, pipeline_of(PLAIN, QX_BLEND_NONE, 0), textures, OPAQUE, false);
}

/* The pass's texture copied to GPU_BEHIND, made or made larger first; only what the copy lacks,
   when it is of this texture: false, no memory. */
static bool copy_behind(Gpu *const g)
{
    const uint32_t id = g->pass, deep = g->deep;
    const int32_t w = g->w, h = g->h;
    const Rect clip = g->clip;
    const GpuBox all = {-0.5f, -0.5f, (float)w - 0.5f, (float)h - 0.5f};

    const bool some = g->behind == id && g->ndirty <= GPU_DIRTY;

    if (w > g->behind_w || h > g->behind_h) {
        const uint32_t name = GPU_BEHIND;
        const uint32_t made[] = {
            name,
            (uint32_t)(w > g->behind_w ? w : g->behind_w),
            (uint32_t)(h > g->behind_h ? h : g->behind_h),
            QX_TEXTURE_RGBA
        };

        if ((g->behind_w && !list_op(&g->list, QX_LIST_FREE, &name, 1)) ||
            !list_op(&g->list, QX_LIST_TEXTURE, made, 4))
            return false;

        g->behind_w = (int32_t)made[1];
        g->behind_h = (int32_t)made[2];
    }

    g->passing = false;

    if (!aim(g, GPU_BEHIND, 0, g->behind_w, g->behind_h, (Rect){0, 0, w, h}) ||
        !pipeline(g, PLAIN, QX_BLEND_NONE, 0))
        return false;

    for (uint32_t i = 0; some && i < g->ndirty; i++)
        if (!copy_box(g, id, w, h, g->dirty[i], 0))
            return false;

    if (!some && !copy_box(g, id, w, h, all, 0))
        return false;

    g->passing = false;
    g->behind = id;
    g->ndirty = 0;
    return aim(g, id, deep, w, h, clip);
}

/* t's draw over box b: what is behind it copied first when it reads that and the copy lacks some.
   False, no memory. */
static bool drawing(Gpu *const g, const GpuTarget *const t, const GpuBox b)
{
    if (t->behind && stale(g, b) && !copy_behind(g))
        return false;

    g->call = joined(g->call, b);
    return true;
}

bool gpu_draw(Gpu *const g, const GpuTarget *const t, const GpuVertex *const v, const uint32_t n)
{
    const uint32_t textures[LIST_TEXTURES] =
        {t->texture, GPU_PALETTE, t->behind ? GPU_BEHIND : GPU_PALETTE};

    return drawing(g, t, box_of(v, sizeof *v, n)) && pipeline(g, PLAIN, t->blend, t->depth) &&
           list_draw(
               &g->list,
               v,
               n,
               pipeline_of(PLAIN, t->blend, t->depth),
               textures,
               t->alpha,
               any(v)
           );
}

bool gpu_discs(Gpu *const g, const GpuTarget *const t, const GpuDisc *const v, const uint32_t n)
{
    const uint32_t textures[LIST_TEXTURES] =
        {t->texture, GPU_PALETTE, t->behind ? GPU_BEHIND : GPU_PALETTE};

    return drawing(g, t, box_of(v, sizeof *v, n)) && pipeline(g, ROUND, t->blend, 0) &&
           list_discs(
               &g->list,
               v,
               n,
               pipeline_of(ROUND, t->blend, 0),
               textures,
               t->alpha,
               any(&v->v)
           );
}

/* An 8-bit canvas's indices through palette row 0 into GPU_SHOWN: false, no memory. */
static bool present(Gpu *const g)
{
    const int32_t w = g->screen_w, h = g->screen_h;
    const GpuBox all = {-0.5f, -0.5f, (float)w - 0.5f, (float)h - 0.5f};

    return aim(g, GPU_SHOWN, 0, w, h, (Rect){0, 0, w, h}) && pipeline(g, PLAIN, QX_BLEND_NONE, 0) &&
           copy_box(g, SURF_SCREEN, w, h, all, GPU_STAGE_INDEXED);
}

void gpu_play(Gpu *const g, const bool show)
{
    const uint32_t shown = g->eight ? GPU_SHOWN : SURF_SCREEN;
    uint32_t n;

    called(g);

    const bool ready =
        !show || ((!g->eight || present(g)) && list_op(&g->list, QX_LIST_SHOW, &shown, 1));
    const uint32_t *const words = ready ? list_end(&g->list, &n) : nullptr;

    if (words)
        qx_host->gpu_play(words, n);

    g->count.batches += g->list.batches;
    g->count.uploads += g->list.uploads;

    list_reset(&g->list);
    g->passing = false;
}

/* h's texture: null, none made. */
static GpuTexture *made(Gpu *const g, const uint32_t h)
{
    const uint32_t slot = hdl_slot(h);
    GpuTexture *const e = slot < g->ntextures ? &g->textures[slot] : nullptr;

    return e && e->handle == h ? e : nullptr;
}

void gpu_wrote(Gpu *const g, const uint32_t h, const int32_t top, const int32_t bottom)
{
    GpuTexture *const e = made(g, h);
    const bool none = e && e->top >= e->bottom;

    if (!e)
        return;

    e->top = none || top < e->top ? top : e->top;
    e->bottom = none || bottom > e->bottom ? bottom : e->bottom;
}

bool gpu_sync(Gpu *const g, const Pixels px, const uint32_t h)
{
    GpuTexture *const e = made(g, h);

    if (!e || !e->drawn)
        return true;

    gpu_play(g, false);
    uint8_t *const rgba = mem_alloc((uint32_t)(px.w * px.h) * RGBA);

    if (!rgba)
        return false;

    qx_host->gpu_read(h, 0, 0, px.w, px.h, rgba);
    g->count.waits++;

    for (int32_t y = 0; y < px.h; y++)
        for (int32_t x = 0; x < px.w; x++) {
            const uint8_t *const c = rgba + (y * px.w + x) * RGBA;
            const uint32_t rgb = (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];

            const uint32_t v = px.format == PX_8    ? c[0]
                               : px.format == PX_32 ? (uint32_t)c[3] << 24 | rgb
                               : c[3] < HALF        ? 0
                                                    : px_pack(px.format, rgb);
            store(&px, x, y, v);
        }

    mem_free(rgba);
    e->drawn = false;
    return true;
}

GpuCount gpu_counted(Gpu *const g)
{
    const GpuCount c = g->count;
    g->count = (GpuCount){};

    return c;
}

void gpu_forget(Gpu *const g, const uint32_t h)
{
    GpuTexture *const e = made(g, h);

    const uint32_t deep = depth_of(h);

    if (e && e->page)
        unpack(g, e);
    else if (e && e->format)
        list_op(&g->list, QX_LIST_FREE, &h, 1);

    if (e && e->depth)
        list_op(&g->list, QX_LIST_FREE, &deep, 1);

    if (e)
        *e = (GpuTexture){};

    g->behind = h == g->behind ? 0 : g->behind;
}
