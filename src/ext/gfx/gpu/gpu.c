/* A GPU's recorder (gpu/gpu.h). */
#include "gpu/gpu.h"
#include "gfx_qx.h"
#include "mem.h"

enum { RGBA = 4, HALF = 0x80, OPAQUE = 0xFF };

/* The list's name of a picture's texture. */
static uint32_t name_of(const uint32_t texture)
{
    return texture == GFX_SURFACE_CANVAS ? QX_CANVAS : texture;
}

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
    List *const l,
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

bool gpu_open(Gpu *const g)
{
    mem_fill(g->textures, 0, g->ntextures * sizeof *g->textures);

    for (uint32_t i = 0; i <= PIC_FONT; i++)
        g->own[i] = (GpuTexture){};

    g->passing = false;
    g->palette = 0;
    g->white = 0xFFFFFFFF;

    GpuTexture *const canvas = entry(g, GFX_SURFACE_CANVAS);

    if (!canvas || !list_init(&g->list))
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
    const uint32_t name = name_of(p->texture);
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

bool gpu_target(Gpu *const g, const uint32_t id, const Rect clip)
{
    if (!g->passing || g->pass != id) {
        if (!list_op(&g->list, QX_LIST_PASS, &id, 1))
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
    uint32_t args[2];

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

    list_pointer(args, rows);
    g->palette = changes;
    return list_op(&g->list, QX_LIST_PALETTE, args, 2);
}

bool gpu_draw(Gpu *const g, const GpuTarget *const t, const QxVertex *const v, const uint32_t n)
{
    return list_draw(&g->list, v, n, t->texture, t->blend, t->alpha);
}

void gpu_play(Gpu *const g, const bool show)
{
    uint32_t n;
    const uint32_t *const words =
        !show || list_op(&g->list, QX_LIST_SHOW, nullptr, 0) ? list_end(&g->list, &n) : nullptr;

    if (words)
        qx_host->gfx_play(words, n);

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

    qx_host->gfx_read(name_of(h), 0, 0, px.w, px.h, rgba);
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

void gpu_forget(Gpu *const g, const uint32_t h)
{
    const uint32_t slot = hdl_slot(h);
    GpuTexture *const e = slot < g->ntextures ? &g->textures[slot] : nullptr;

    if (e && e->handle == h && e->format)
        list_op(&g->list, QX_LIST_FREE, &h, 1);

    if (e && e->handle == h)
        *e = (GpuTexture){};
}
