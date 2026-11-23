/* A GPU's recorder: a frame's draws into the list its host plays (gpu/list.h), the textures and
 * pipelines the list names, and the surfaces' pixels kept in step with them.
 *
 * # How it works
 * The first list makes the canvas, the palette's texture and the shader's pipelines (gpu/shaders),
 * one a kind of vertex and blend. A texture is named by its root surface's handle or by a PIC_
 * name (surface.h). It is made, and the root's pixels sent, when first drawn from or into; sent
 * again when the CPU wrote them since (the root's version). An 8-bit root drawn from is an index
 * texture; drawn into, RGBA through palette row 0, as is the canvas. A root drawn into is ahead on
 * the GPU (drawn) until gpu_sync reads it back. A draw reads its picture and the palette.
 */
#ifndef GFX_GPU_GPU_H
#define GFX_GPU_GPU_H
#include "colour.h"
#include "gfx_qx.h"
#include "gpu/list.h"
#include "surface.h"

typedef struct {
    uint32_t handle, version;
    QxTextureFormat format; /* 0, none made */
} GpuTexture;

/* What the GPU was asked: draws (batches), texture uploads, and reads waited for. */
typedef struct {
    uint32_t batches, uploads, waits;
} GpuCount;

typedef struct {
    GpuList list;
    GpuTexture own[PIC_FONT + 1]; /* the PIC_ textures */
    GpuTexture *textures;         /* by a root's slot */
    uint32_t ntextures;
    bool passing;  /* a pass since the list began */
    uint32_t pass; /* its texture */
    Rect clip;
    uint32_t palette; /* the palette's changes when sent, and 1; 0 not yet */
    float down;       /* the shader's: 1 where clip space's y runs down a texture's rows, else -1 */
    uint32_t white;   /* the white texel shapes draw from */
    uint8_t *page;    /* the canvas's pixels, read back */
    GpuCount count;   /* since gpu_counted */
} Gpu;

/* What a call's vertices share. */
typedef struct {
    Gpu *gpu;
    Vec at;        /* the target's place in its texture: a view's */
    GpuVertex ink; /* the tint and light; a picture's rectangle, row and stages */
    uint32_t texture;
    QxBlend blend;
    uint8_t alpha;
    bool eight;          /* the target is 8 bits: a shape's colour is an index */
    const uint32_t *row; /* the palette row an index colours through */
    uint32_t tint;
    uint32_t paint; /* a picture's colour: the tint, or SOLID's */
    bool solid;
} GpuTarget;

/* The canvas, s's size, the palette and the pipelines, in the language s gave: false, no memory. */
bool gpu_open(Gpu *g, const QxScreen *s);
void gpu_close(Gpu *g);
/* p's texture, made and sent as the GPU needs it, RGBA when drawn into: its name into *id, and
   whether it holds indices into *indexed. false, no memory. */
bool gpu_texture(
    Gpu *g,
    const Palette *pal,
    const Picture *p,
    bool into,
    uint32_t *id,
    bool *indexed
);
/* The draws after go into id, w by h, within clip: false, no memory. */
bool gpu_target(Gpu *g, uint32_t id, int32_t w, int32_t h, Rect clip);
/* The palette sent, if it changed: false, no memory. */
bool gpu_palette(Gpu *g, const Palette *pal);
/* n vertices, four a quad, drawn as t's call: false, no memory. */
bool gpu_draw(Gpu *g, const GpuTarget *t, const GpuVertex *v, uint32_t n);
/* n disc vertices, four a quad, drawn as t's call: false, no memory. */
bool gpu_discs(Gpu *g, const GpuTarget *t, const GpuDisc *v, uint32_t n);
/* The list played, the canvas shown after it when show. */
void gpu_play(Gpu *g, bool show);
/* The root at h, which holds root, read back if a GPU drew in it: false, no memory. */
bool gpu_sync(Gpu *g, Palette *pal, Surface *root, Pixels px, uint32_t h);
/* h, a root, freed: its texture with it. */
void gpu_forget(Gpu *g, uint32_t h);
/* The count since the last call, zeroed. */
GpuCount gpu_counted(Gpu *g);
#endif
