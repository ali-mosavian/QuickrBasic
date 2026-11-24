/* What gfx's shader (backend/gpu/shaders) is given a vertex: a plain one, and a circle's. */
#ifndef GFX_GPU_VERTEX_H
#define GFX_GPU_VERTEX_H
#include <stdint.h>

/* A vertex's stages: what its pixels do. */
enum {
    GPU_STAGE_INDEXED = 1,  /* the texel is an index, through the palette's row */
    GPU_STAGE_KEYED = 2,    /* index 0, or alpha under a half, is not drawn */
    GPU_STAGE_SOLID = 4,    /* the colour is the tint alone, where a texel is drawn */
    GPU_STAGE_WRAP = 8,     /* u repeats across the rectangle */
    GPU_STAGE_DISC = 16,    /* drawn only in the disc's band */
    GPU_STAGE_SWEEP = 32,   /* and in the sweep */
    GPU_STAGE_WIDE = 64,    /* the sweep is past half a turn */
    GPU_STAGE_SMOOTH = 128, /* the four texels round the centre mixed */
};

/* x, y a pixel's centre at whole numbers; u, v texels, the one at a pixel's centre drawn, kept
   inside the rectangle rx, ry, rw by rh. Its colour is the texel's times r, g, b and light / 63. */
typedef struct {
    float x, y, u, v;
    uint16_t rx, ry, rw, rh;
    uint8_t r, g, b, light;
    uint8_t row, stages, pad[2];
} GpuVertex;

/* A circle's or an arc's vertex. With DISC, a pixel is drawn where its centre p is within disc's
   outer radius of its centre and not within its inner: within r of c when |p - c|^2 < r^2, or
   equal with p left of c, and r > 0. With SWEEP too, where d = p - c is clockwise of sweep's
   start direction and anticlockwise of its finish (the cross products at least 0); WIDE, where it
   is not strictly outside both. Each product and sum is rounded once, as gfx's software rounds
   it. */
typedef struct {
    GpuVertex v;
    float disc[4];  /* the centre's x, y; the outer and inner radii */
    float sweep[4]; /* the start's direction x, y; the finish's */
} GpuDisc;
#endif
