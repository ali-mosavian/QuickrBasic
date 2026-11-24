/* Surfaces: everything drawn from or into. A view is a surface over part of another's pixels, with
 * its own shape and no memory of its own; the canvas is the screen's page.
 *
 * # How it works
 * A surface's pixels are its own, the canvas's page (slot 0, surface.CANVAS), or its parent's at
 * its place: surf_pixels walks up to the one with memory, so a view follows its parent's page from
 * frame to frame and aim moves it without allocating. Freeing a surface frees its views, and
 * theirs; their handles are then stale. A surface has a format: a view its parent's, the canvas
 * the screen's.
 *
 * A root, the surface with the pixels, counts the writes to them (version), so a GPU's copy knows
 * when it is behind, and keeps the rows written since the GPU took them, so it sends only those;
 * drawn says the GPU's copy is ahead, until read back.
 */
#ifndef GFX_SURFACE_H
#define GFX_SURFACE_H
#include "core/handle.h"
#include "base/depth.h"
#include "base/pixels.h"

typedef struct {
    uint32_t parent; /* a view's surface; 0 none */
    int32_t x, y;    /* a view's place in its parent */
    int32_t w, h;
    int32_t ax, ay; /* the anchor: the point a sprite's x, y names */
    PxFormat format;
    uint8_t *own;      /* its own pixels, w a row; null a view's or the canvas's */
    uint32_t *palette; /* an image's 256 colours, &HRRGGBB; null none */
    uint32_t version;  /* a root's: its pixels written, counted; 0 never */
    uint32_t taken;    /* a root's: the version a GPU took, rows top to bottom written since */
    int32_t top, bottom;
    bool drawn;      /* a root's: a GPU drew in it since the pixels were read back */
    uint16_t *depth; /* a root's: w by h, 1/z by DEPTH_NEAR, 0 far; null none asked yet */
} Surface;

/* A picture's own texture, with no surface: a handle is never so small. */
enum { PIC_WHITE = 1, PIC_FONT = 2 };

/* A surface's pixels as drawn from: its root's (the surface with memory, or the canvas's page),
   its rectangle in them; the root's handle, or a PIC_ texture's, and the root's version; the
   root's record, null a PIC_ texture's. */
typedef struct {
    Pixels root;
    Rect at;
    uint32_t texture, version;
    Surface *surface;
} Picture;

static inline Pixels pic_pixels(const Picture *const p) { return px_cut(&p->root, p->at); }

typedef struct {
    HdlTable t;
    Pixels canvas; /* the page; p null, no screen */
} Surfaces;

void surf_init(Surfaces *ss);
/* Every surface freed. */
void surf_done(Surfaces *ss);
/* The canvas, the screen's page w by h: false, no memory. */
bool surf_open(Surfaces *ss, Pixels page);
/* The page drawn in from now on: the next frame's. */
void surf_page(Surfaces *ss, Pixels page);
/* The canvas and its views freed. */
void surf_close(Surfaces *ss);
/* A surface w by h of format with own's pixels (null: its own, zeroed) and palette (null none),
   both then its: its handle, 0 no memory. */
uint32_t surf_new(
    Surfaces *ss,
    int32_t w,
    int32_t h,
    PxFormat format,
    uint8_t *own,
    uint32_t *palette
);
/* A view w by h of parent at x, y, which lie inside it: its handle, 0 no memory. */
uint32_t surf_view(Surfaces *ss, uint32_t parent, int32_t x, int32_t y, int32_t w, int32_t h);
/* h's surface: null, none (freed, or not a surface). */
Surface *surf_get(const Surfaces *ss, uint32_t h);
/* h's picture, h a surface. */
Picture surf_picture(const Surfaces *ss, uint32_t h);
/* s's root; *h, s's handle, becomes the root's. */
Surface *surf_root(const Surfaces *ss, Surface *s, uint32_t *h);
/* r of s's pixels written: its root's version counted, its rows kept. */
void surf_wrote(const Surfaces *ss, Surface *s, Rect r);
Pixels surf_pixels(const Surfaces *ss, const Surface *s);
/* s's depth, its root's made all far on first ask: p null, no memory. */
Depth surf_depth(const Surfaces *ss, Surface *s);
/* h and its views freed. */
void surf_free(Surfaces *ss, uint32_t h);
#endif
