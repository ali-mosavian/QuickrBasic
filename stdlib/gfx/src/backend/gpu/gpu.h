/* A GPU's recorder: a frame's draws into the list its host plays (backend/gpu/list.h), the textures and
 * pipelines the list names, and the surfaces' pixels kept in step with them.
 *
 * # How it works
 * The first list makes the canvas, the palette's texture and the shader's pipelines (backend/gpu/shaders),
 * one a kind of vertex and blend. A root surface's pixels, or a PIC_ picture's (surface.h), are
 * sent when first drawn from or into, and again when the CPU wrote them since (the root's
 * version). A root drawn from, half a page or smaller, is packed into an atlas page of its format,
 * so pictures share a texture and their sprites a batch: pages are packed in rows, a place freed
 * with its root, a page empty again started over. Drawn into, a root has a texture of its own,
 * named by its handle; packed, it leaves its page. An 8-bit root drawn from is indices; drawn
 * into, RGBA through palette row 0, as is the canvas. A root drawn into is ahead on the GPU
 * (drawn) until gpu_sync reads it back. A draw reads its picture and the palette. A texture
 * keeps the rows the CPU wrote since it was sent (gpu_wrote), so it sends only those. A root drawn
 * into with depth has a depth texture too (gpu_depth), made on that first draw, which every pass
 * into it keeps after. A pipeline is made when first drawn with.
 *
 * A draw that reads what is behind its pixels (GpuTarget.behind) reads a copy of its target, made
 * before it when the target changed under it since the last: drawn in, by a call before this one,
 * over the draw's box, or written by the CPU. A call's own draws never overlap. The copy is made
 * again only where the target was drawn in since, when it is of the same target.
 */
#ifndef GFX_GPU_GPU_H
#define GFX_GPU_GPU_H
#include "core/palette.h"
#include "gfx_qx.h"
#include "backend/gpu/list.h"
#include "core/surface.h"

/* The palette's texture, the copy of what is behind, the first atlas page's, a page's side; a
   root's depth texture is its handle with GPU_DEPTH's kind. */
enum { GPU_PALETTE = PIC_FONT + 1, GPU_BEHIND, GPU_PAGE_FIRST, GPU_PAGE = 1024, GPU_DEPTH = 8 };

/* The boxes drawn in since the copy kept apart; past them, it is made again before the next read. */
enum { GPU_DIRTY = 16 };

/* A box of a target's pixels, its edges at half pixels: none when x0 is not under x1. */
typedef struct {
    float x0, y0, x1, y1;
} GpuBox;

typedef struct {
    uint32_t handle, version;
    QxTextureFormat format; /* 0, none made */
    uint32_t page;          /* its page's texture; 0 its own */
    uint16_t x, y;          /* its place in the page */
    int32_t top, bottom;    /* the rows the CPU wrote since version was sent */
    bool drawn;             /* the GPU drew in it since it was read back */
    bool depth;             /* its depth texture made */
} GpuTexture;

/* An atlas page: the next place, x along the row at y, row the row's height so far. */
typedef struct {
    QxTextureFormat format;
    uint32_t live; /* the roots in it */
    uint16_t x, y, row;
} GpuPage;

/* Where a picture's root is: its texture, its place in it, whether it holds indices, and its
   depth texture (0 none). */
typedef struct {
    uint32_t id;
    uint16_t x, y;
    bool indexed;
    uint32_t depth;
} GpuSource;

/* What the GPU was asked: draws (batches), texture uploads, and reads waited for. */
typedef struct {
    uint32_t batches, uploads, waits;
} GpuCount;

typedef struct {
    GpuList list;
    GpuTexture own[PIC_FONT + 1]; /* the PIC_ textures */
    GpuTexture *textures;         /* by a root's slot */
    uint32_t ntextures;
    GpuPage *pages; /* the first's texture PAGE_FIRST */
    uint32_t npages, cappages;
    bool passing;        /* a pass since the list began */
    uint32_t pass, deep; /* its texture and depth texture */
    int32_t w, h;        /* its size */
    Rect clip;
    uint32_t behind;            /* the texture GPU_BEHIND copies; 0 none */
    int32_t behind_w, behind_h; /* GPU_BEHIND's size; 0 not made */
    GpuBox dirty[GPU_DIRTY];    /* drawn in behind since the copy, by calls before this one */
    uint32_t ndirty;            /* past GPU_DIRTY, more */
    GpuBox call;                /* drawn in the pass by this call */
    uint32_t palette;           /* the palette's changes when sent, and 1; 0 not yet */
    float down;      /* the shader's: 1 where clip space's y runs down a texture's rows, else -1 */
    uint32_t white;  /* the white texel shapes draw from */
    uint8_t *page;   /* the canvas's pixels, read back */
    uint32_t shader; /* the language (QxShader) */
    uint32_t made;   /* the pipelines made, a bit each */
    GpuCount count;  /* since gpu_counted */
} Gpu;

/* What a call's vertices share. */
typedef struct {
    Gpu *gpu;
    Vec at;        /* the target's place in its texture: a view's */
    Rect clip;     /* the target's */
    GpuVertex ink; /* the tint and light; a picture's rectangle, row and stages */
    uint32_t texture;
    QxBlend blend;
    uint8_t depth; /* QxDepth */
    bool perspective;
    uint8_t alpha;
    bool eight;          /* the target is 8 bits: a shape's colour is an index */
    const uint32_t *row; /* the palette row an index colours through */
    uint32_t tint;
    uint32_t paint; /* a picture's colour: the tint, or SOLID's */
    bool solid;
    bool behind; /* its pixels read what is behind them */
} GpuTarget;

/* The canvas, s's size, the palette and the pipelines, in the language s gave: false, no memory. */
bool gpu_open(Gpu *g, const QxScreen *s);
void gpu_close(Gpu *g);
/* p's root on the GPU, made and sent as it needs it; into, drawn into: RGBA, and ahead of the CPU
   till read back. False, no memory. */
bool gpu_texture(Gpu *g, const Palette *pal, const Picture *p, bool into, GpuSource *out);
/* p's root's depth texture, made, all 0, if it had none (*fresh then): false, no memory. */
bool gpu_depth(Gpu *g, const Picture *p, uint32_t *id, bool *fresh);
/* Rows top to bottom of the root h written by the CPU. */
void gpu_wrote(Gpu *g, uint32_t h, int32_t top, int32_t bottom);
/* The draws after go into id, w by h, within clip, with depth texture deep (0 none): false, no
   memory. */
bool gpu_target(Gpu *g, uint32_t id, uint32_t deep, int32_t w, int32_t h, Rect clip);
/* The palette sent, if it changed: false, no memory. */
bool gpu_palette(Gpu *g, const Palette *pal);
/* n vertices, four a quad, drawn as t's call: false, no memory. */
bool gpu_draw(Gpu *g, const GpuTarget *t, const GpuVertex *v, uint32_t n);
/* n disc vertices, four a quad, drawn as t's call: false, no memory. */
bool gpu_discs(Gpu *g, const GpuTarget *t, const GpuDisc *v, uint32_t n);
/* The list played, the canvas shown after it when show. */
void gpu_play(Gpu *g, bool show);
/* The root h, whose pixels are px, read back if the GPU drew in it: false, no memory. */
bool gpu_sync(Gpu *g, const Palette *pal, Pixels px, uint32_t h);
/* h, a root, freed: its texture with it. */
void gpu_forget(Gpu *g, uint32_t h);
/* The count since the last call, zeroed. */
GpuCount gpu_counted(Gpu *g);
#endif
