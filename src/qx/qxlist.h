/* gfx's list: a GPU frame's uploads, passes and draws, already batched, which the host's player
 * plays (QxHost's gfx_play, minor 2). Part of the ABI.
 *
 * # How it works
 * The list is words. An op's first word is its code, and above QX_LIST_SIZE its words, itself
 * included; its arguments follow. A pointer is two words, low first: the extension's memory, kept
 * until gfx_play returns. The extension names textures; 0 is the canvas gfx_open made. A texture's
 * rows are top first, and x, y count pixels from its top left.
 *
 * - VERTICES count, pointer: the list's QxVertex vertices, before its first draw.
 * - DISCS count, pointer: its QxDisc vertices, after VERTICES.
 * - TEXTURE id, w, h, format: made, every byte 0.
 * - FREE id.
 * - UPLOAD id, x, y, w, h, pitch, pointer: h rows of w texels of the texture's format, pitch
 *   bytes each, into it.
 * - PALETTE pointer: QX_ROWS rows of QX_COLOURS colours, bytes R, G, B and 255.
 * - PASS id: what the draws after go into; its pixels kept.
 * - CLIP x, y, w, h: the pixels of the pass's texture the draws after may touch.
 * - DRAW first, count, texture, blend, alpha: count vertices from first, four a quad, its corners
 *   in order round (triangles 0 1 2 and 0 2 3), their texels from texture. Drawn, a pixel's colour
 *   c goes in as c (QX_BLEND_NONE), c times alpha over the rest (MIX), or added (ADD); alpha 0 to
 *   255. A pixel drawn is opaque.
 * - DRAW_DISCS first, count, texture, blend, alpha: DRAW's, of the DISCS vertices.
 * - SHOW: the canvas is shown.
 * - END.
 */
#ifndef QX_LIST_H
#define QX_LIST_H
#include <stdint.h>

enum { QX_LIST_SIZE = 8, QX_ROWS = 16, QX_COLOURS = 256, QX_CANVAS = 0 };

typedef enum : uint8_t {
    QX_LIST_END,
    QX_LIST_VERTICES,
    QX_LIST_TEXTURE,
    QX_LIST_FREE,
    QX_LIST_UPLOAD,
    QX_LIST_PALETTE,
    QX_LIST_PASS,
    QX_LIST_CLIP,
    QX_LIST_DRAW,
    QX_LIST_SHOW,
    QX_LIST_DISCS,
    QX_LIST_DRAW_DISCS,
} QxListOp;

/* INDEX a byte a texel; RGBA bytes R, G, B and A. */
typedef enum : uint8_t { QX_TEXTURE_INDEX = 1, QX_TEXTURE_RGBA } QxTextureFormat;

typedef enum : uint8_t { QX_BLEND_NONE, QX_BLEND_MIX, QX_BLEND_ADD } QxBlend;

/* A vertex's stages: what its pixels do. */
enum {
    QX_STAGE_INDEXED = 1, /* the texel is an index, through the palette's row */
    QX_STAGE_KEYED = 2,   /* index 0, or alpha under a half, is not drawn */
    QX_STAGE_SOLID = 4,   /* the colour is the tint alone, where a texel is drawn */
    QX_STAGE_WRAP = 8,    /* u repeats across the rectangle */
    QX_STAGE_DISC = 16,   /* drawn only in the disc's band */
    QX_STAGE_SWEEP = 32,  /* and in the sweep */
    QX_STAGE_WIDE = 64,   /* the sweep is past half a turn */
};

/* x, y a pixel's centre at whole numbers; u, v texels, the one at a pixel's centre drawn, kept
   inside the rectangle rx, ry, rw by rh. Its colour is the texel's times r, g, b and light / 63. */
typedef struct {
    float x, y, u, v;
    uint16_t rx, ry, rw, rh;
    uint8_t r, g, b, light;
    uint8_t row, stages, pad[2];
} QxVertex;

/* A circle's or an arc's vertex. With DISC, a pixel is drawn where its centre p is within disc's
   outer radius of its centre and not within its inner: within r of c when |p - c|^2 < r^2, or
   equal with p left of c, and r > 0. With SWEEP too, where d = p - c is clockwise of sweep's
   start direction and anticlockwise of its finish (the cross products at least 0); WIDE, where it
   is not strictly outside both. Each product and sum is rounded once, as gfx's software rounds
   it. */
typedef struct {
    QxVertex v;
    float disc[4];  /* the centre's x, y; the outer and inner radii */
    float sweep[4]; /* the start's direction x, y; the finish's */
} QxDisc;
#endif
