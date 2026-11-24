/* Shapes: a picture's draw call to its corners on the target, each with the texel there, u and v
 * in texels from the picture's top left, z 1: the polygons a backend fills.
 *
 * # How it works
 * A picture's anchor names a texel's centre, so its edges lie half a texel round it. Its corners
 * are turned and scaled about the anchor, then placed by the camera (command.h). Flips swap u or
 * v; the texels stay affine over each shape, so three corners give the whole map.
 */
#ifndef GFX_SHAPE_H
#define GFX_SHAPE_H
#include "core/command.h"

/* A picture w by h: its anchor, a texel's centre, at a in the world, turned by angle (clockwise
   on the screen) and scaled; flags draw.FLIPX and FLIPY. Its corners, top left round. */
void shape_sprite(
    int32_t w,
    int32_t h,
    Vec anchor,
    Vec a,
    float angle,
    float scale,
    int32_t flags,
    const Place *where,
    Corner out[4]
);
/* A picture w by h onto four corners in the world, from its top left round. */
void shape_quad(
    int32_t w,
    int32_t h,
    const Vec corners[4],
    int32_t flags,
    const Place *where,
    Corner out[4]
);
/* A strip's part from a to b, the picture h high hanging below; u its texel at a. */
void shape_segment(
    int32_t w,
    int32_t h,
    Vec a,
    Vec b,
    float u,
    int32_t flags,
    const Place *where,
    Corner out[4]
);
#endif
