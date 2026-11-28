/* gfx's pipelines: one for each kind of vertex, blend and depth, made in a list when first drawn
 * with, from gfx's shader in the language the host takes (backend/gpu/shaders.h).
 */
#ifndef GFX_GPU_PIPELINES_H
#define GFX_GPU_PIPELINES_H
#include "backend/gpu/gpu.h"

/* Plain vertices (list_draw's), discs' (list_discs'), and plain vertices of a simple draw: one
   drawing SOLID's colour or index, or texels unlit, as they are into a direct target, indices
   through row 0 into an 8-bit one. The shader for simple draws is the rest's first branch alone:
   the whole one's size cost plain draws a tenth. */
typedef enum { PIPELINE_PLAIN, PIPELINE_ROUND, PIPELINE_SIMPLE, PIPELINE_KINDS } PipelineKind;

/* The kind of a draw of plain vertices like v. */
PipelineKind pipeline_kind(const GpuVertex *v);

/* A pipeline's blends and depths (QxDepth). */
enum {
    PIPELINE_BLENDS = QX_BLEND_KEEP + 1,
    PIPELINE_DEPTHS = (QX_DEPTH_TEST | QX_DEPTH_WRITE) + 1,
};

/* The shader is in language (QxShader), or there is none: FIXED. */
bool pipeline_speaks(uint32_t language);
/* The pipeline for vertices of kind k (a PipelineKind, or fixed.c's) with blend and depth: 1 up to
   GPU_PIPELINES, a bit of Gpu.made each. */
uint32_t pipeline_id(uint32_t k, QxBlend blend, uint32_t depth);

/* Pipeline id is made. */
static inline bool pipeline_known(const Gpu *const g, const uint32_t id)
{
    return g->made[(id - 1) / 64] >> ((id - 1) % 64) & 1;
}

static inline void pipeline_know(Gpu *const g, const uint32_t id)
{
    g->made[(id - 1) / 64] |= 1ull << ((id - 1) % 64);
}
/* That pipeline, made if not yet: false, no memory. */
bool pipeline_made(Gpu *g, PipelineKind k, QxBlend blend, uint32_t depth);
#endif
