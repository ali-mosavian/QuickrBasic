/* gfx on a fixed-function host (backend/gpu/fixed.h). */
#include "backend/gpu/fixed.h"
#include "backend/gpu/pipelines.h"
#include "core/mem.h"
#include "core/palette.h"

enum { ATTRIBUTES = 4, WRAPS = 3 };

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

    /* a keyed SOLID draws its texels' shape in the colour: text, a sprite in one colour */
    args[QX_FIXED_SOURCE] = !(stages & GPU_STAGE_SOLID) ? QX_FIXED_TEXEL
                            : stages & GPU_STAGE_KEYED  ? QX_FIXED_MASK
                                                        : QX_FIXED_COLOUR;
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

/* The last colour and light lit, and the word they made: a draw's corners share their stages and
   mostly their colour and light. */
typedef struct {
    bool known;
    uint32_t from, to;
} Lit;

/* Word rgb's channel at shift lit by light over 63, as the vertex's byte holds it. */
static uint32_t channel_lit(const uint32_t rgb, const uint32_t shift, const uint32_t light)
{
    return ((rgb >> shift & 0xFF) * light / PAL_FULL & 0xFF) << shift;
}

/* v's colour lit as the shader lights it: SOLID's as it is, a texel's tint by light over 63, an
   index's white (LOOKUP's lit); its alpha whole. r, g, b and light read and written as one word,
   not four bytes. */
static void lit(GpuVertex *const v, Lit *const k)
{
    static_assert(offsetof(GpuVertex, light) == offsetof(GpuVertex, r) + 3);

    enum : uint32_t { WHOLE = 0xFFu << 24, RGB = 0xFFFFFF };

    uint32_t from;

    mem_copy(&from, &v->r, sizeof from);

    if (!k->known || from != k->from) {
        const uint32_t light = from >> 24;
        const bool solid = v->stages & GPU_STAGE_SOLID, index = v->stages & GPU_STAGE_INDEXED;
        const uint32_t tint = channel_lit(from, 0, light) | channel_lit(from, 8, light) |
                              channel_lit(from, 16, light);

        k->known = true;
        k->from = from;
        k->to = (solid ? from & RGB : index ? RGB : tint) | WHOLE;
    }

    mem_copy(&v->r, &k->to, sizeof k->to);
}

/* LOOKUP for t's index texels sent, if they draw as other words than it last gave: into an 8-bit
   target, the lut's indices; else the row's colours lit and tinted. False, no memory. */
static bool looked_up(Gpu *const g, const GpuCall *const t)
{
    const int32_t row = t->template.row, light = t->template.light;
    const PalKey k = pal_key(g->pal, row, light, t->tint);

    if (g->looked_up && g->looked_eight == t->eight && pal_same(g->looked, k))
        return true;

    const uint32_t *const argb = t->eight ? nullptr : pal_argb(g->pal, row, light, t->tint);
    uint32_t *const words = list_keep(&g->list, QX_LIST_INDICES * sizeof *words);
    uint32_t args[2];

    if (!words || (!t->eight && !argb))
        return false;

    for (uint32_t i = 0; i < QX_LIST_INDICES; i++)
        words[i] = !t->eight ? argb[i] & PAL_WHITE : t->lut ? t->lut[i] : i;

    list_pointer(args, words);

    if (!list_op(&g->list, QX_LIST_LOOKUP, args, 2))
        return false;

    g->looked = k;
    g->looked_eight = t->eight;
    g->looked_up = true;
    return true;
}

bool fixed_draw(Gpu *const g, const GpuCall *const t, const GpuVertex *const v, const uint32_t n)
{
    const uint32_t textures[LIST_TEXTURES] = {t->texture};
    const bool index = (v->stages & (GPU_STAGE_INDEXED | GPU_STAGE_SOLID)) == GPU_STAGE_INDEXED;

    Lit k = {};
    uint32_t id;

    if ((index && !looked_up(g, t)) || !made(g, v->stages, t->blend, t->depth, &id) ||
        !list_draw(&g->list, v, n, id, textures, t->alpha, gpu_shared(v)))
        return false;

    /* lit where the list holds them: not copied once more */
    GpuVertex *const in = list_last(&g->list, n);

    for (uint32_t i = 0; i < n; i++)
        lit(&in[i], &k);

    return true;
}
