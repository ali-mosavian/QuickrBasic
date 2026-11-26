/* Surfaces: everything drawn from or into. A view is a surface over part of another's pixels, with
 * its own shape and no memory of its own; the screen's surface is the screen's page.
 *
 * # How it works
 * A surface's pixels are its own, the screen's page (slot 0, SURF_SCREEN), or its parent's at its
 * place: surf_pixels walks up to the one with memory, so a view follows its parent's page from
 * frame to frame and set_view moves it without allocating. Freeing a surface frees its views, and
 * theirs; their handles are then stale. A surface has a format: a view its parent's, the screen's
 * the host's. Each has its own clip, camera and anchor, which drawing into or from it uses.
 *
 * A root, the surface with the pixels, counts the writes to them (version), so a GPU's copy knows
 * when it is behind.
 */
#ifndef GFX_SURFACE_H
#define GFX_SURFACE_H
#include "base/pixels.h"
#include "core/handle.h"

typedef struct {
    uint32_t parent; /* a view's surface; 0 none */
    int32_t x, y;    /* a view's place in its parent */
    int32_t w, h;
    int32_t ax, ay; /* the anchor: the point a sprite's x, y names */
    Rect clip;      /* clipped false, the whole of it */
    bool clipped;
    Camera camera;
    PxFormat format;
    uint8_t *own;      /* its own pixels, w a row; null a view's or the screen's */
    uint32_t *palette; /* an image's 256 colours, &HRRGGBB; null none */
    uint32_t version;  /* a root's: its pixels written, counted; 0 never */
} Surface;

/* The screen's surface. */
enum : uint32_t { SURF_SCREEN = (uint32_t)HDL_SURFACE << HDL_KIND_SHIFT };

/* A source's own texture, with no surface: a handle is never so small. */
enum { SOURCE_WHITE = 1, SOURCE_FONT = 2 };

/* A surface's pixels as drawn from: its root's (the surface with memory, or the screen's page),
   its rectangle in them; the root's handle, or a SOURCE_ texture's, and the root's version. */
typedef struct {
    Pixels root;
    Rect at;
    uint32_t texture, version;
} Source;

static inline Pixels source_pixels(const Source *const p) { return px_cut(&p->root, p->at); }

typedef struct {
    HdlTable t;
    Pixels screen; /* the page; p null, no screen */
} Surfaces;

void surf_init(Surfaces *ss);
/* Every surface freed. */
void surf_done(Surfaces *ss);
/* The screen's surface, the screen's page: false, no memory. */
bool surf_open(Surfaces *ss, Pixels page);
/* The page drawn in from now on: the next frame's. */
void surf_page(Surfaces *ss, Pixels page);
/* The screen's surface and its views freed. */
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
/* h's source, h a surface. */
Source surf_source(const Surfaces *ss, uint32_t h);
/* s's root; *h, s's handle, becomes the root's. */
Surface *surf_root(const Surfaces *ss, Surface *s, uint32_t *h);
/* s's clip, inside it. */
Rect surf_clip(const Surface *s);
/* s's pixels written: its root's version counted. */
void surf_wrote(const Surfaces *ss, Surface *s);
Pixels surf_pixels(const Surfaces *ss, const Surface *s);
/* h and its views freed. */
void surf_free(Surfaces *ss, uint32_t h);
#endif
