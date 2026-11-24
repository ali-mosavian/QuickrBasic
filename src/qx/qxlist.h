/* The GPU list: a frame's work for a host's GPU, which the host plays (QxHost's gpu_play, minor
 * 2). It knows nothing of what is drawn: the extension brings its shaders, vertices and textures.
 * Part of the ABI.
 *
 * # How it works
 * The list is words. An op's first word is its code, and above QX_LIST_SIZE its words, itself
 * included; its arguments follow. A pointer is two words, low first: the extension's memory, kept
 * until gpu_play returns. The extension numbers its buffers, textures and pipelines, each kind
 * its own numbers; each lasts until gfx_close. A texture's rows are top first, x, y pixels from
 * its top left. A write never changes what a list played before reads.
 *
 * - BUFFER id, bytes, kind: at least bytes of vertices or of 32-bit indices; made, or made again,
 *   when there is none that large, its bytes then undefined.
 * - WRITE id, bytes, pointer: the buffer's first bytes; its others undefined after.
 * - TEXTURE id, w, h, format: made, every byte 0. An RGBA one can be drawn into; a DEPTH one is
 *   a pass's depth target.
 * - FREE id: the texture.
 * - UPLOAD id, x, y, w, h, pitch, pointer: h rows of w texels of the texture's format, pitch bytes
 *   each, into it.
 * - PIPELINE id, blend, stride, samplers, vertex code, fragment code, attributes: how a DRAW with
 *   it draws. Code is three words, its bytes then a pointer, in the language gfx_open gave
 *   (QxScreen.shader); the vertex code's uniform block is binding 0, the fragment code's
 *   samplers bindings 0 up, nearest. MSL's entry is main0. ESSL has no bindings: its one
 *   uniform block is 0, the sampler at binding n named sn. SPIR-V's entry is main, its uniform
 *   block in set 1 and its samplers in set 2, as SDL_GPU's. A vertex is stride bytes, each
 *   attribute three words: its location, format and offset. Drawn, a pixel's colour c goes in as c
 *   (QX_BLEND_NONE), c times the draw's alpha over the rest (MIX), or c times alpha added (ADD);
 *   KEEP writes no colour. A pixel drawn is opaque. Blend's second byte is its depth (QxDepth),
 *   0 none: a pixel's depth is its z over w in clip space, 0 to 1. A pipeline with depth draws
 *   only in a pass with a depth target; one without, in any.
 * - PASS id, depth: the RGBA texture the draws after go into, its pixels kept; depth, if given and
 *   not 0, the DEPTH texture of its size they test and set.
 * - CLIP x, y, w, h: the pass's pixels the draws after may touch.
 * - UNIFORMS bytes, pointer: the vertex code's uniform block, for the draws after.
 * - DRAW pipeline, vertices, indices, first, count, alpha, textures: count indices from first of
 *   buffer indices, triangles of buffer vertices' vertices, through the pipeline; the textures,
 *   a word each, its samplers'; alpha 0 to 255.
 * - SHOW id: the texture is shown.
 * - END.
 */
#ifndef QX_LIST_H
#define QX_LIST_H
#include <stdint.h>

enum { QX_LIST_SIZE = 8 };

typedef enum : uint8_t {
    QX_LIST_END,
    QX_LIST_BUFFER,
    QX_LIST_WRITE,
    QX_LIST_TEXTURE,
    QX_LIST_FREE,
    QX_LIST_UPLOAD,
    QX_LIST_PIPELINE,
    QX_LIST_PASS,
    QX_LIST_CLIP,
    QX_LIST_UNIFORMS,
    QX_LIST_DRAW,
    QX_LIST_SHOW,
} QxListOp;

typedef enum : uint8_t { QX_BUFFER_VERTEX = 1, QX_BUFFER_INDEX } QxBuffer;

/* INDEX a byte a texel; RGBA bytes R, G, B and A; DEPTH (minor 4) 16 bits a texel. */
typedef enum : uint8_t { QX_TEXTURE_INDEX = 1, QX_TEXTURE_RGBA, QX_TEXTURE_DEPTH } QxTextureFormat;

/* KEEP: minor 4. */
typedef enum : uint8_t { QX_BLEND_NONE, QX_BLEND_MIX, QX_BLEND_ADD, QX_BLEND_KEEP } QxBlend;

/* A pipeline's depth, minor 4: TEST draws a pixel only where it is greater than the target's
   depth, WRITE sets the target's depth where it draws (ALWAYS, without TEST). */
typedef enum : uint8_t { QX_DEPTH_TEST = 1, QX_DEPTH_WRITE = 2 } QxDepth;

/* An attribute's format: floats, or unsigned integers as they are. */
typedef enum : uint8_t {
    QX_ATTRIBUTE_FLOAT2 = 1,
    QX_ATTRIBUTE_FLOAT4,
    QX_ATTRIBUTE_USHORT4,
    QX_ATTRIBUTE_UBYTE4,
} QxAttribute;
#endif
