/* gfx's pipelines: one for each kind of vertex, blend and depth, made in a list when first drawn
 * with, from gfx's shader in the language the host takes (backend/gpu/shaders.h).
 */
#ifndef GFX_GPU_PIPELINES_H
#define GFX_GPU_PIPELINES_H
#include "backend/gpu/gpu.h"

/* The vertices of list_draw and of list_discs. */
typedef enum { PIPELINE_PLAIN, PIPELINE_ROUND, PIPELINE_KINDS } PipelineKind;

/* The shader is in language (QxShader). */
bool pipeline_speaks(uint32_t language);
/* The pipeline for vertices of kind k with blend and depth (QxDepth): 1 up, a bit of Gpu.made
   each. */
uint32_t pipeline_id(PipelineKind k, QxBlend blend, uint32_t depth);
/* That pipeline, made if not yet: false, no memory. */
bool pipeline_made(Gpu *g, PipelineKind k, QxBlend blend, uint32_t depth);
#endif
