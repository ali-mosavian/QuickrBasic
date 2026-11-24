/* gfx's pipelines (backend/gpu/pipelines.h). */
#include "backend/gpu/pipelines.h"
#include "backend/gpu/shaders.h"
#include "core/mem.h"

enum {
    BLENDS = QX_BLEND_KEEP + 1,
    DEPTHS = (QX_DEPTH_TEST | QX_DEPTH_WRITE) + 1,
    PLAIN_ATTRIBUTES = 5,
    DISC_ATTRIBUTES = 7,
};

static_assert(offsetof(GpuDisc, v) == 0, "a disc's vertex is a plain one's attributes");
static_assert(PIPELINE_KINDS * BLENDS * DEPTHS <= 64, "a pipeline a bit of Gpu.made");

PipelineKind pipeline_kind(const GpuVertex *const v)
{
    const uint32_t plain =
        v->stages & ~(GPU_STAGE_KEYED | GPU_STAGE_WRAP | GPU_STAGE_EIGHT | GPU_STAGE_ALPHA);
    const bool unlit = v->r == 255 && v->g == 255 && v->b == 255 && v->light == PAL_FULL;
    const bool eight = v->stages & GPU_STAGE_EIGHT;

    return plain == GPU_STAGE_SOLID ||
                   (unlit && (eight ? plain == GPU_STAGE_INDEXED && !v->row : !plain))
               ? PIPELINE_SIMPLE
               : PIPELINE_PLAIN;
}

uint32_t pipeline_id(const PipelineKind k, const QxBlend blend, const uint32_t depth)
{
    return 1 + (k * BLENDS + blend) * DEPTHS + depth;
}

/* A language's code: each kind's vertex and fragment stages. */
typedef struct {
    const void *vertex[PIPELINE_KINDS], *fragment[PIPELINE_KINDS];
    uint32_t vertex_bytes[PIPELINE_KINDS], fragment_bytes[PIPELINE_KINDS];
} Code;

/* A language's code, its text's NUL left out (text 1) or its bytes whole (0). */
#define CODE(language, text)                                                                       \
    {                                                                                              \
        {draw_vert_##language, disc_vert_##language, draw_vert_##language},                        \
        {draw_frag_##language, draw_frag_##language, simple_frag_##language},                      \
        {sizeof draw_vert_##language - (text),                                                     \
         sizeof disc_vert_##language - (text),                                                     \
         sizeof draw_vert_##language - (text)},                                                    \
        {sizeof draw_frag_##language - (text),                                                     \
         sizeof draw_frag_##language - (text),                                                     \
         sizeof simple_frag_##language - (text)},                                                  \
    }

static const Code codes[] = {
    [QX_SHADER_MSL] = CODE(msl, 1),
    [QX_SHADER_ESSL] = CODE(essl, 1),
    [QX_SHADER_SPIRV] = CODE(spirv, 0),
};

bool pipeline_made(Gpu *const g, const PipelineKind k, const QxBlend blend, const uint32_t depth)
{
    enum {
        VERTEX_CODE = QX_PIPELINE_CODE,
        FRAGMENT_CODE = QX_PIPELINE_CODE + QX_CODE_WORDS,
        HEAD = QX_PIPELINE_ATTRIBUTES,
        ATTRIBUTE = QX_ATTRIBUTE_WORDS,
    };

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
    } kinds[PIPELINE_KINDS] = {
        {sizeof(GpuVertex), PLAIN_ATTRIBUTES},
        {sizeof(GpuDisc), DISC_ATTRIBUTES},
        {sizeof(GpuVertex), PLAIN_ATTRIBUTES},
    };

    const uint32_t id = pipeline_id(k, blend, depth);
    const uint64_t bit = 1ull << (id - 1);
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
    args[FRAGMENT_CODE] = c->fragment_bytes[k];
    list_pointer(args + VERTEX_CODE + 1, c->vertex[k]);
    list_pointer(args + FRAGMENT_CODE + 1, c->fragment[k]);
    mem_copy(args + HEAD, attributes, kinds[k].attributes * sizeof *attributes);

    if (!list_op(&g->list, QX_LIST_PIPELINE, args, HEAD + kinds[k].attributes * ATTRIBUTE))
        return false;

    g->made |= bit;
    return true;
}

bool pipeline_speaks(const uint32_t language)
{
    return language < sizeof codes / sizeof *codes && codes[language].fragment[0];
}
