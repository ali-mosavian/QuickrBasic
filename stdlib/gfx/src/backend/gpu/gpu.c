/* A GPU's recorder (backend/gpu/gpu.h). */
#include "backend/gpu/gpu.h"
#include "gfx_qx.h"
#include "backend/gpu/atlas.h"
#include "backend/gpu/pipelines.h"
#include "core/mem.h"

enum { RGBA = 4, OPAQUE = 0xFF };

/* A box none. */
static constexpr GpuBox NONE = {0, 0, 0, 0};

/* Root h's depth texture. */
static uint32_t depth_of(const uint32_t h) { return h | (uint32_t)GPU_DEPTH << HDL_KIND_SHIFT; }

/* texture's entry: null, no memory. */
static GpuTexture *entry(Gpu *const g, const uint32_t texture)
{
    if (texture <= SOURCE_FONT)
        return &g->own[texture];

    const uint32_t slot = hdl_slot(texture), had = g->ntextures;

    if (!mem_grow((void **)&g->textures, &g->ntextures, slot + 1, sizeof *g->textures))
        return nullptr;

    mem_fill(g->textures + had, 0, (g->ntextures - had) * sizeof *g->textures);
    return &g->textures[slot];
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
    const uint32_t bytes = px_bytes(px->format);
    *pitch = (uint32_t)px->w * (index ? 1 : RGBA);
    uint8_t *const out = list_keep(l, *pitch * (uint32_t)px->h);

    for (int32_t y = 0; out && y < px->h; y++)
        for (int32_t x = 0; x < px->w; x++) {
            uint8_t *const o = out + y * *pitch + x * (index ? 1 : RGBA);
            const uint32_t v = px_load(px->p + y * px->pitch + (uint32_t)x * bytes, bytes);
            const uint32_t c = argb(px->format, v);

            if (index)
                o[0] = (uint8_t)v;
            else
                mem_copy(o, (const uint8_t[]){c >> 16, c >> 8, c, c >> 24}, RGBA);
        }

    return out;
}

bool gpu_open(Gpu *const g, const QxScreen *const s, const bool eight)
{
    mem_fill(g->textures, 0, g->ntextures * sizeof *g->textures);

    for (uint32_t i = 0; i <= SOURCE_FONT; i++)
        g->own[i] = (GpuTexture){};

    g->npages = 0;

    g->passing = false;
    g->behind = 0;
    g->behind_w = g->behind_h = 0;
    g->ndirty = 0;
    g->call = NONE;
    g->palette = 0;
    g->mapped = 0;
    g->row0 = 1;
    g->coloured = 1; /* all index 0, black in the palette as it starts */
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
    };

    if (!canvas || !pipeline_speaks(s->shader) || !list_init(&g->list) ||
        !list_op(&g->list, QX_LIST_TEXTURE, made[0], 4) ||
        !list_op(&g->list, QX_LIST_TEXTURE, made[1], 4))
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

bool gpu_texture(Gpu *const g, const Source *const p, const bool into, GpuSource *const out)
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
        atlas_unpack(g, e);
        e->format = 0;
    }

    if (!e->format && !into && px->w <= GPU_PAGE / 2 && px->h <= GPU_PAGE / 2) {
        if (!atlas_pack(g, e, want, (uint16_t)px->w, (uint16_t)px->h))
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
        g->coloured = id == SURF_SCREEN ? 0 : g->coloured;
    }

    if (e->version != p->version)
        e->top = e->bottom = 0;

    e->version = p->version;
    e->drawn = e->drawn || into;

    *out = (GpuSource){
        id,
        e->x,
        e->y,
        px->format == PX_8,
        px->format == PX_8 && e->format == QX_TEXTURE_RGBA,
        e->depth ? depth_of(name) : 0,
    };

    return true;
}

bool gpu_depth(Gpu *const g, const Source *const p, uint32_t *const id, bool *const fresh)
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

/* v's colour is the tint's alone, never keyed: its texel does not matter. */
static bool shared(const GpuVertex *const v)
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

    const PipelineKind k = pipeline_kind(v);

    return pipeline_made(g, k, QX_BLEND_NONE, 0) &&
           list_draw(&g->list, v, 4, pipeline_id(k, QX_BLEND_NONE, 0), textures, OPAQUE, false);
}

/* An 8-bit texture's copy: its indices, coloured through row 0. */
static constexpr uint16_t EIGHT_COPY = GPU_STAGE_INDEXED | GPU_STAGE_ALPHA | GPU_STAGE_EIGHT;

/* Texture id, w by h, copied to GPU_BEHIND, made or made larger first; only what the copy lacks,
   when it is of id; indices when eight. The pass ends. False, no memory. */
static bool copy_behind(
    Gpu *const g,
    const uint32_t id,
    const int32_t w,
    const int32_t h,
    const bool eight
)
{
    const uint16_t stages = eight ? EIGHT_COPY : 0;
    const GpuBox all = {-0.5f, -0.5f, (float)w - 0.5f, (float)h - 0.5f};

    const bool some = g->behind == id && g->ndirty <= GPU_DIRTY;

    if (some && !g->ndirty)
        return true;

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

    if (!aim(g, GPU_BEHIND, 0, g->behind_w, g->behind_h, (Rect){0, 0, w, h}))
        return false;

    for (uint32_t i = 0; some && i < g->ndirty; i++)
        if (!copy_box(g, id, w, h, g->dirty[i], stages))
            return false;

    if (!some && !copy_box(g, id, w, h, all, stages))
        return false;

    g->passing = false;
    g->behind = id;
    g->ndirty = 0;
    return true;
}

/* t's draw over box b: what is behind it copied first when it reads that and the copy lacks some.
   False, no memory. */
static bool drawing(Gpu *const g, const GpuCall *const t, const GpuBox b)
{
    const uint32_t id = g->pass, deep = g->deep;
    const int32_t w = g->w, h = g->h;
    const Rect clip = g->clip;

    if (t->behind && stale(g, b) &&
        (!copy_behind(g, id, w, h, t->eight) || !aim(g, id, deep, w, h, clip)))
        return false;

    g->call = joined(g->call, b);
    return true;
}

bool gpu_draw(Gpu *const g, const GpuCall *const t, const GpuVertex *const v, const uint32_t n)
{
    const uint32_t textures[LIST_TEXTURES] =
        {t->texture, GPU_PALETTE, t->behind ? GPU_BEHIND : GPU_PALETTE};

    const PipelineKind k = pipeline_kind(v);

    return drawing(g, t, box_of(v, sizeof *v, n)) && pipeline_made(g, k, t->blend, t->depth) &&
           list_draw(
               &g->list,
               v,
               n,
               pipeline_id(k, t->blend, t->depth),
               textures,
               t->alpha,
               shared(v)
           );
}

bool gpu_discs(Gpu *const g, const GpuCall *const t, const GpuDisc *const v, const uint32_t n)
{
    const uint32_t textures[LIST_TEXTURES] =
        {t->texture, GPU_PALETTE, t->behind ? GPU_BEHIND : GPU_PALETTE};

    return drawing(g, t, box_of(v, sizeof *v, n)) &&
           pipeline_made(g, PIPELINE_ROUND, t->blend, 0) &&
           list_discs(
               &g->list,
               v,
               n,
               pipeline_id(PIPELINE_ROUND, t->blend, 0),
               textures,
               t->alpha,
               shared(&v->v)
           );
}

/* An 8-bit canvas's colours made again from its indices through row 0, when it changed since they
   were: copied behind, drawn back. False, no memory. */
static bool recolour(Gpu *const g)
{
    const int32_t w = g->screen_w, h = g->screen_h;
    const GpuBox all = {-0.5f, -0.5f, (float)w - 0.5f, (float)h - 0.5f};

    if (g->coloured == g->row0)
        return true;

    if (!copy_behind(g, SURF_SCREEN, w, h, true) ||
        !aim(g, SURF_SCREEN, 0, w, h, (Rect){0, 0, w, h}) ||
        !copy_box(g, GPU_BEHIND, w, h, all, EIGHT_COPY))
        return false;

    g->coloured = g->row0;
    return true;
}

void gpu_play(Gpu *const g, const bool show)
{
    const uint32_t shown = SURF_SCREEN;
    uint32_t n;

    called(g);

    const bool ready =
        !show || ((!g->eight || recolour(g)) && list_op(&g->list, QX_LIST_SHOW, &shown, 1));
    const uint32_t *const words = ready ? list_end(&g->list, &n) : nullptr;

    if (words)
        qx_host->gpu_play(words, n);

    g->count.batches += g->list.batches;
    g->count.uploads += g->list.uploads;

    list_reset(&g->list);
    g->passing = false;
}

void gpu_wrote(Gpu *const g, const uint32_t h, const int32_t top, const int32_t bottom)
{
    GpuTexture *const e = gpu_made(g, h);
    const bool none = e && e->top >= e->bottom;

    if (!e)
        return;

    e->top = none || top < e->top ? top : e->top;
    e->bottom = none || bottom > e->bottom ? bottom : e->bottom;
}

GpuCount gpu_counted(Gpu *const g)
{
    const GpuCount c = g->count;
    g->count = (GpuCount){};

    return c;
}

void gpu_forget(Gpu *const g, const uint32_t h)
{
    GpuTexture *const e = gpu_made(g, h);

    const uint32_t deep = depth_of(h);

    if (e && e->page)
        atlas_unpack(g, e);
    else if (e && e->format)
        list_op(&g->list, QX_LIST_FREE, &h, 1);

    if (e && e->depth)
        list_op(&g->list, QX_LIST_FREE, &deep, 1);

    if (e)
        *e = (GpuTexture){};

    g->behind = h == g->behind ? 0 : g->behind;
}
