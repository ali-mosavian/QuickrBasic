/* A GPU's recorder: a frame's draws into the list its host plays (gpu/list.h), the textures the
 * list names, and the surfaces' pixels kept in step with them.
 *
 * # How it works
 * A texture is named by its root surface's handle, the canvas's QX_CANVAS (gfx_open makes it), or
 * by a PIC_ name (surface.h). It is made, and the root's pixels sent, when first drawn from or
 * into; sent again when the CPU wrote them since (the root's version). An 8-bit root drawn from is
 * an index texture; drawn into, RGBA through palette row 0, as is the canvas. A root drawn into is
 * ahead on the GPU (drawn) until gpu_sync reads it back.
 */
#ifndef GFX_GPU_GPU_H
#define GFX_GPU_GPU_H
#include "colour.h"
#include "gpu/list.h"
#include "surface.h"

typedef struct {
    uint32_t handle, version;
    QxTextureFormat format; /* 0, none made */
} GpuTexture;

typedef struct {
    List list;
    GpuTexture own[PIC_FONT + 1]; /* the PIC_ textures */
    GpuTexture *textures;         /* by a root's slot */
    uint32_t ntextures;
    bool passing;  /* a pass since the list began */
    uint32_t pass; /* its texture */
    Rect clip;
    uint32_t palette; /* the palette's changes when sent, and 1; 0 not yet */
    uint32_t white;   /* the white texel shapes draw from */
    uint8_t *page;    /* the canvas's pixels, read back */
} Gpu;

/* What a call's vertices share. */
typedef struct {
    Gpu *gpu;
    Vec at;       /* the target's place in its texture: a view's */
    QxVertex ink; /* the tint and light; a picture's rectangle, row and stages */
    uint32_t texture;
    QxBlend blend;
    uint8_t alpha;
    bool eight;          /* the target is 8 bits: a shape's colour is an index */
    const uint32_t *row; /* the palette row an index colours through */
    uint32_t tint;
    uint32_t paint; /* a picture's colour: the tint, or SOLID's */
    bool solid;
} GpuTarget;

/* The canvas's texture made by the host, w by h: false, no memory. */
bool gpu_open(Gpu *g);
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
/* The draws after go into id, within clip: false, no memory. */
bool gpu_target(Gpu *g, uint32_t id, Rect clip);
/* The palette sent, if it changed: false, no memory. */
bool gpu_palette(Gpu *g, const Palette *pal);
/* n vertices, three a triangle, drawn as t's call: false, no memory. */
bool gpu_draw(Gpu *g, const GpuTarget *t, const QxVertex *v, uint32_t n);
/* The list played, the canvas shown after it when show. */
void gpu_play(Gpu *g, bool show);
/* The root at h, which holds root, read back if a GPU drew in it: false, no memory. */
bool gpu_sync(Gpu *g, Palette *pal, Surface *root, Pixels px, uint32_t h);
/* h, a root, freed: its texture with it. */
void gpu_forget(Gpu *g, uint32_t h);
#endif
