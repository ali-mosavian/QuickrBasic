/* gfx's pipelines: one for each kind of vertex, blend and depth, made in a list when first drawn
 * with, from gfx's shader in the language the host takes (backend/gpu/shaders.h).
 */
#ifndef GFX_GPU_PIPELINES_H
#define GFX_GPU_PIPELINES_H
#include "backend/gpu/gpu.h"

/* Plain vertices (list_draw's), discs' (list_discs'), and plain vertices of a simple draw: one
   drawing SOLID's colour or index, or texels unlit, as they are into a 32-bit target, indices
   through row 0 into an 8-bit one. The shader for simple draws is the rest's first branch alone:
   the whole one's size cost plain draws a tenth. */
typedef enum { PIPELINE_PLAIN, PIPELINE_ROUND, PIPELINE_SIMPLE, PIPELINE_KINDS } PipelineKind;

/* The kind of a draw of plain vertices like v. */
PipelineKind pipeline_kind(const GpuVertex *v);

/* The shader is in language (QxShader). */
bool pipeline_speaks(uint32_t language);
/* The pipeline for vertices of kind k with blend and depth (QxDepth): 1 up, a bit of Gpu.made
   each. */
uint32_t pipeline_id(PipelineKind k, QxBlend blend, uint32_t depth);
/* That pipeline, made if not yet: false, no memory. */
bool pipeline_made(Gpu *g, PipelineKind k, QxBlend blend, uint32_t depth);
#endif
