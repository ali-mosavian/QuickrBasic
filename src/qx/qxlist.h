/* gfx's list: a GPU frame's uploads, passes and draws, already batched, which the host's player
 * plays (QxHost's gfx_play, minor 2). Part of the ABI.
 *
 * # How it works
 * The list is words. An op's first word is its code, and above QX_LIST_SIZE its words, itself
 * included; its arguments follow. A pointer is two words, low first: the extension's memory, kept
 * until gfx_play returns. The extension names textures; 0 is the canvas gfx_open made. A texture's
 * rows are top first, and x, y count pixels from its top left.
 *
 * - VERTICES count, pointer: the list's vertices, before its first draw.
 * - TEXTURE id, w, h, format: made, every byte 0.
 * - FREE id.
 * - UPLOAD id, x, y, w, h, pitch, pointer: rows of the texture's format into it.
 * - PALETTE pointer: QX_ROWS rows of QX_COLOURS colours, bytes R, G, B and 255.
 * - PASS id: what the draws after go into; its pixels kept.
 * - CLIP x, y, w, h: the pixels of the pass's texture the draws after may touch.
 * - DRAW first, count, texture, blend, alpha: count vertices from first, three a triangle, their
 *   texels from texture. Drawn, a pixel's colour c goes in as c (QX_BLEND_NONE), c times alpha over
 *   the rest (MIX), or added (ADD); alpha 0 to 255. A pixel drawn is opaque.
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
};

/* x, y a pixel's centre at whole numbers; u, v texels, the one at a pixel's centre drawn, kept
   inside the rectangle rx, ry, rw by rh. Its colour is the texel's times r, g, b and light / 63. */
typedef struct {
    float x, y, u, v;
    uint16_t rx, ry, rw, rh;
    uint8_t r, g, b, light;
    uint8_t row, stages, pad[2];
} QxVertex;
#endif
