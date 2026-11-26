/* gfx on a fixed-function host (backend/gpu/fixed.h). */
#include "backend/gpu/fixed.h"
#include "backend/gpu/pipelines.h"
#include "core/mem.h"

enum { ATTRIBUTES = 4, WRAPS = 3, QUAD = 4 };

/* A pipeline's fixed state as a kind of pipeline_id's: what it draws, then its flags. */
static uint32_t kind_of(const uint32_t stages)
{
    const uint32_t texel = !(stages & GPU_STAGE_SOLID), key = (stages & GPU_STAGE_KEYED) != 0;
    const uint32_t linear = (stages & GPU_STAGE_SMOOTH) != 0;
    const uint32_t wrap = stages & GPU_STAGE_TILE ? 2 : (stages & GPU_STAGE_WRAP) != 0;
    return texel + 2 * (key + 2 * (linear + 2 * wrap));
}

static_assert(2 * 2 * 2 * WRAPS * PIPELINE_BLENDS * PIPELINE_DEPTHS <= GPU_PIPELINES);

/* The pipeline for stages with blend and depth, made if not yet, its id to *id: false, no memory. */
static bool made(
    Gpu *const g,
    const uint32_t stages,
    const QxBlend blend,
    const uint32_t depth,
    uint32_t *const id
)
{
    enum { HEAD = QX_PIPELINE_ATTRIBUTES, ATTRIBUTE = QX_ATTRIBUTE_WORDS };

    static const uint32_t attributes[ATTRIBUTES][ATTRIBUTE] = {
        {QX_FIXED_AT_POSITION, QX_ATTRIBUTE_FLOAT4, offsetof(GpuVertex, x)},
        {QX_FIXED_AT_TEXEL, QX_ATTRIBUTE_FLOAT2, offsetof(GpuVertex, u)},
        {QX_FIXED_AT_COLOUR, QX_ATTRIBUTE_UBYTE4, offsetof(GpuVertex, r)},
        {QX_FIXED_AT_RECT, QX_ATTRIBUTE_USHORT4, offsetof(GpuVertex, rx)},
    };

    const bool tile = stages & GPU_STAGE_TILE;
    uint32_t args[HEAD + ATTRIBUTES * ATTRIBUTE] = {
        pipeline_id(kind_of(stages), blend, depth),
        blend | depth << 8,
        sizeof(GpuVertex),
        LIST_TEXTURES,
    };

    *id = args[0];

    if (pipeline_known(g, *id))
        return true;

    args[QX_FIXED_SOURCE] = stages & GPU_STAGE_SOLID ? QX_FIXED_COLOUR : QX_FIXED_TEXEL;
    args[QX_FIXED_FLAGS] = (stages & GPU_STAGE_KEYED ? QX_FIXED_KEY : 0) |
                           (stages & GPU_STAGE_SMOOTH ? QX_FIXED_LINEAR : 0) |
                           (tile || stages & GPU_STAGE_WRAP ? QX_FIXED_WRAP_U : 0) |
                           (tile ? QX_FIXED_WRAP_V : 0);
    mem_copy(args + HEAD, attributes, sizeof attributes);

    if (!list_op(&g->list, QX_LIST_PIPELINE, args, HEAD + ATTRIBUTES * ATTRIBUTE))
        return false;

    pipeline_know(g, *id);
    return true;
}

/* v's colour lit as the shader lights it: SOLID's as it is, a texel's tint by light over 63; its
   alpha whole. */
static GpuVertex lit(GpuVertex v)
{
    if (!(v.stages & GPU_STAGE_SOLID)) {
        v.r = (uint8_t)(v.r * v.light / PAL_FULL);
        v.g = (uint8_t)(v.g * v.light / PAL_FULL);
        v.b = (uint8_t)(v.b * v.light / PAL_FULL);
    }

    v.light = 0xFF;
    return v;
}

bool fixed_draw(Gpu *const g, const GpuCall *const t, const GpuVertex *const v, const uint32_t n)
{
    const uint32_t textures[LIST_TEXTURES] = {t->texture};

    GpuVertex out[QUAD];
    uint32_t id;

    if (!made(g, v->stages, t->blend, t->depth, &id))
        return false;

    for (uint32_t at = 0; at < n; at += QUAD) {
        const uint32_t m = n - at < QUAD ? n - at : QUAD;

        for (uint32_t i = 0; i < m; i++)
            out[i] = lit(v[at + i]);

        if (!list_draw(&g->list, out, m, id, textures, t->alpha, gpu_shared(v)))
            return false;
    }

    return true;
}
