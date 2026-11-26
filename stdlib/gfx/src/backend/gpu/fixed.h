/* gfx on a host with a fixed-function pipeline (qxlist.h's FIXED): no shader, so what gfx's shader
 * works out for a pixel is worked out here for a vertex, or in gpu.c for a texture.
 *
 * # How it works
 * A draw's stages become a pipeline's fixed state: SOLID, the colour alone, else the texel times
 * it; KEYED, the texel's alpha tested; SMOOTH, linear; WRAP, u repeated across the rectangle,
 * TILE, u and v. A vertex's colour is lit here: SOLID's as it is, a texel's by the tint times the
 * light over 63, as the shader's lit does. A root's texture is its own, never a page's, and an
 * 8-bit root's texels are its row's colours (gpu.c). A disc is drawn as its rows (backend.c). An
 * 8-bit target holds colours: an index drawn as its colour through row 0, read back as the index
 * nearest it.
 */
#ifndef GFX_GPU_FIXED_H
#define GFX_GPU_FIXED_H
#include "backend/gpu/gpu.h"

/* n vertices, four a quad, drawn as t's call through the fixed pipeline for their stages, made if
   not yet: false, no memory. */
bool fixed_draw(Gpu *g, const GpuCall *t, const GpuVertex *v, uint32_t n);
#endif
