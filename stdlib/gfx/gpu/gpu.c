/* A GPU's recorder (gpu/gpu.h). */
#include "gpu/gpu.h"
#include "gfx_qx.h"
#include "gpu/shaders.h"
#include "mem.h"

enum {
    RGBA = 4,
    HALF = 0x80,
    OPAQUE = 0xFF,
    PALETTE = PIC_FONT + 1, /* the palette's texture */
    BLENDS = 3,
    PLAIN_ATTRIBUTES = 5,
    DISC_ATTRIBUTES = 7,
};

/* The vertices of list_draw and of list_discs. */
typedef enum { PLAIN, ROUND, KINDS } Kind;

static_assert(offsetof(GpuDisc, v) == 0, "a disc's vertex is a plain one's attributes");

/* The pipeline for vertices of kind k with blend. */
static uint32_t pipeline_of(const Kind k, const QxBlend blend) { return 1 + k * BLENDS + blend; }

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

/* A texel as ARGB, a keyed one's alpha 0: an index through row 0. */
static uint32_t argb(const Palette *const pal, const PxFormat f, const uint32_t v)
{
    if (f == PX_32)
        return v;

    const uint32_t c = f == PX_8 ? PX_OPAQUE | pal->rows[0][v] : px_argb(f, v);
    return px_key(f, v) ? c & PX_WHITE : c;
}

/* px's texels in format, in the list's kept memory, *pitch bytes a row: null, no memory. */
static const uint8_t *texels(
    GpuList *const l,
    const Palette *const pal,
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
            const uint32_t c = argb(pal, px->format, v);

            if (index)
                o[0] = (uint8_t)v;
            else
                mem_copy(o, (const uint8_t[]){c >> 16, c >> 8, c, c >> 24}, RGBA);
        }

    return out;
}

/* A language's code: each kind's vertex stage, the fragment stage. */
typedef struct {
    const char *vertex[KINDS], *fragment;
    uint32_t vertex_bytes[KINDS], fragment_bytes;
} Code;

#define CODE(language)                                                                             \
    {                                                                                              \
        {draw_vert_##language, disc_vert_##language},                                              \
        draw_frag_##language,                                                                      \
        {sizeof draw_vert_##language - 1, sizeof disc_vert_##language - 1},                        \
        sizeof draw_frag_##language - 1,                                                           \
    }

static const Code codes[] = {[QX_SHADER_MSL] = CODE(msl), [QX_SHADER_ESSL] = CODE(essl)};

/* The pipelines, a kind of vertex and blend each, the shader's attributes and its code in c. */
static bool pipelines(GpuList *const l, const Code *const c)
{
    /* PIPELINE's words: its code's, its first attribute's, an attribute's */
    enum { VERTEX_CODE = 4, FRAGMENT_CODE = 7, HEAD = 10, ATTRIBUTE = 3 };

    static const uint32_t attributes[DISC_ATTRIBUTES][ATTRIBUTE] = {
        {0, QX_ATTRIBUTE_FLOAT2, offsetof(GpuVertex, x)},
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

    for (Kind k = 0; k < KINDS; k++)
        for (QxBlend b = 0; b < BLENDS; b++) {
            uint32_t args[HEAD + sizeof attributes / sizeof(uint32_t)] = {
                pipeline_of(k, b),
                b,
                kinds[k].stride,
                LIST_TEXTURES,
            };

            args[VERTEX_CODE] = c->vertex_bytes[k];
            args[FRAGMENT_CODE] = c->fragment_bytes;
            list_pointer(args + VERTEX_CODE + 1, c->vertex[k]);
            list_pointer(args + FRAGMENT_CODE + 1, c->fragment);
            mem_copy(args + HEAD, attributes, kinds[k].attributes * sizeof *attributes);

            if (!list_op(l, QX_LIST_PIPELINE, args, HEAD + kinds[k].attributes * ATTRIBUTE))
                return false;
        }

    return true;
}

bool gpu_open(Gpu *const g, const QxScreen *const s)
{
    mem_fill(g->textures, 0, g->ntextures * sizeof *g->textures);

    for (uint32_t i = 0; i <= PIC_FONT; i++)
        g->own[i] = (GpuTexture){};

    g->passing = false;
    g->palette = 0;
    g->white = 0xFFFFFFFF;
    g->down = s->y_down ? 1 : -1;
    g->count = (GpuCount){};

    GpuTexture *const canvas = entry(g, GFX_SURFACE_CANVAS);
    const uint32_t made[][4] = {
        {GFX_SURFACE_CANVAS, s->w, s->h, QX_TEXTURE_RGBA},
        {PALETTE, PAL_COLOURS, PAL_ROWS, QX_TEXTURE_RGBA},
    };

    const Code *const code = s->shader < sizeof codes / sizeof *codes ? &codes[s->shader] : nullptr;

    if (!canvas || !code || !code->fragment || !list_init(&g->list) ||
        !list_op(&g->list, QX_LIST_TEXTURE, made[0], 4) ||
        !list_op(&g->list, QX_LIST_TEXTURE, made[1], 4) || !pipelines(&g->list, code))
        return false;

    *canvas = (GpuTexture){.handle = GFX_SURFACE_CANVAS, .format = QX_TEXTURE_RGBA};

    return true;
}

void gpu_close(Gpu *const g)
{
    list_done(&g->list);
    mem_free(g->textures);
    g->textures = nullptr;
    g->ntextures = 0;
}

bool gpu_texture(
    Gpu *const g,
    const Palette *const pal,
    const Picture *const p,
    const bool into,
    uint32_t *const id,
    bool *const indexed
)
{
    GpuTexture *const e = entry(g, p->texture);
    const uint32_t name = p->texture;
    const Pixels *const px = &p->root;
    const QxTextureFormat want = into || px->format != PX_8 ? QX_TEXTURE_RGBA : QX_TEXTURE_INDEX;

    if (!e)
        return false;

    if (e->handle != p->texture)
        *e = (GpuTexture){.handle = p->texture};

    if (!e->format || (want == QX_TEXTURE_RGBA && e->format == QX_TEXTURE_INDEX)) {
        const bool had = e->format;
        const uint32_t args[] = {name, (uint32_t)px->w, (uint32_t)px->h, want};

        if (had && !list_op(&g->list, QX_LIST_FREE, &name, 1))
            return false;

        if (!list_op(&g->list, QX_LIST_TEXTURE, args, 4))
            return false;

        e->format = want;
        e->version = had ? ~p->version : 0;
    }

    if (e->version != p->version) {
        uint32_t pitch, args[8] = {name, 0, 0, (uint32_t)px->w, (uint32_t)px->h};

        const uint8_t *const bytes = texels(&g->list, pal, px, e->format, &pitch);

        if (!bytes)
            return false;

        args[5] = pitch;
        list_pointer(args + 6, bytes);

        if (!list_op(&g->list, QX_LIST_UPLOAD, args, 8))
            return false;

        e->version = p->version;
    }

    *id = name;
    *indexed = e->format == QX_TEXTURE_INDEX;
    return true;
}

static bool same(const Rect a, const Rect b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool gpu_target(Gpu *const g, const uint32_t id, const int32_t w, const int32_t h, const Rect clip)
{
    if (!g->passing || g->pass != id) {
        float *const size = list_keep(&g->list, 4 * sizeof(float));
        uint32_t args[3] = {4 * sizeof(float)};

        if (!size || !list_op(&g->list, QX_LIST_PASS, &id, 1))
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
        g->clip = (Rect){.w = -1};
    }

    if (same(clip, g->clip))
        return true;

    const uint32_t args[] =
        {(uint32_t)clip.x, (uint32_t)clip.y, (uint32_t)clip.w, (uint32_t)clip.h};

    g->clip = clip;
    return list_op(&g->list, QX_LIST_CLIP, args, 4);
}

bool gpu_palette(Gpu *const g, const Palette *const pal)
{
    uint32_t changes = 1;

    for (uint32_t r = 0; r < PAL_ROWS; r++)
        changes += pal->changed[r];

    if (changes == g->palette)
        return true;

    uint8_t *const rows = list_keep(&g->list, PAL_ROWS * PAL_COLOURS * RGBA);
    uint32_t args[8] = {PALETTE, 0, 0, PAL_COLOURS, PAL_ROWS, PAL_COLOURS * RGBA};

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
    g->palette = changes;
    return list_op(&g->list, QX_LIST_UPLOAD, args, 8);
}

/* v's colour is the tint's alone, never keyed: its texel does not matter. */
static bool any(const GpuVertex *const v)
{
    return (v->stages & (GPU_STAGE_SOLID | GPU_STAGE_KEYED)) == GPU_STAGE_SOLID;
}

bool gpu_draw(Gpu *const g, const GpuTarget *const t, const GpuVertex *const v, const uint32_t n)
{
    const uint32_t textures[LIST_TEXTURES] = {t->texture, PALETTE};

    const uint32_t pipeline = pipeline_of(PLAIN, t->blend);

    return list_draw(&g->list, v, n, pipeline, textures, t->alpha, any(v));
}

bool gpu_discs(Gpu *const g, const GpuTarget *const t, const GpuDisc *const v, const uint32_t n)
{
    const uint32_t textures[LIST_TEXTURES] = {t->texture, PALETTE};

    const uint32_t pipeline = pipeline_of(ROUND, t->blend);

    return list_discs(&g->list, v, n, pipeline, textures, t->alpha, any(&v->v));
}

void gpu_play(Gpu *const g, const bool show)
{
    const uint32_t canvas = GFX_SURFACE_CANVAS;
    uint32_t n;
    const uint32_t *const words =
        !show || list_op(&g->list, QX_LIST_SHOW, &canvas, 1) ? list_end(&g->list, &n) : nullptr;

    if (words)
        qx_host->gpu_play(words, n);

    g->count.batches += g->list.batches;
    g->count.uploads += g->list.uploads;

    list_reset(&g->list);
    g->passing = false;
}

bool gpu_sync(
    Gpu *const g,
    Palette *const pal,
    Surface *const root,
    const Pixels px,
    const uint32_t h
)
{
    if (!root->drawn)
        return true;

    gpu_play(g, false);
    uint8_t *const rgba = mem_alloc((uint32_t)(px.w * px.h) * RGBA);

    if (!rgba)
        return false;

    qx_host->gpu_read(h, 0, 0, px.w, px.h, rgba);
    g->count.waits++;
    uint32_t last = ~0u, index = 0; /* no colour is ~0 */

    for (int32_t y = 0; y < px.h; y++)
        for (int32_t x = 0; x < px.w; x++) {
            const uint8_t *const c = rgba + (y * px.w + x) * RGBA;
            const uint32_t rgb = (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
            const bool keyed = c[3] < HALF;

            if (px.format == PX_8 && !keyed && rgb != last)
                index = pal_nearest(pal->rows[0], last = rgb);

            const uint32_t v = px.format == PX_32  ? (uint32_t)c[3] << 24 | rgb
                               : keyed             ? 0
                               : px.format == PX_8 ? index
                                                   : px_pack(px.format, rgb);
            store(&px, x, y, v);
        }

    mem_free(rgba);
    root->drawn = false;
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
    const uint32_t slot = hdl_slot(h);
    GpuTexture *const e = slot < g->ntextures ? &g->textures[slot] : nullptr;

    if (e && e->handle == h && e->format)
        list_op(&g->list, QX_LIST_FREE, &h, 1);

    if (e && e->handle == h)
        *e = (GpuTexture){};
}
