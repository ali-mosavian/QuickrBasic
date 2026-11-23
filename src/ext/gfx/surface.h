/* Surfaces: everything drawn from or into. A view is a surface over part of another's pixels, with
 * its own shape and no memory of its own; the canvas is the screen's page.
 *
 * # How it works
 * A surface's pixels are its own, the canvas's page (slot 0, surface.CANVAS), or its parent's at
 * its place: surf_pixels walks up to the one with memory, so a view follows its parent's page from
 * frame to frame and aim moves it without allocating. Freeing a surface frees its views, and
 * theirs; their handles are then stale. A surface has a format: a view its parent's, the canvas
 * the screen's.
 */
#ifndef GFX_SURFACE_H
#define GFX_SURFACE_H
#include "handle.h"
#include "pixels.h"

typedef struct {
    uint32_t parent; /* a view's surface; 0 none */
    int32_t x, y;    /* a view's place in its parent */
    int32_t w, h;
    int32_t ax, ay; /* the anchor: the point a sprite's x, y names */
    PxFormat format;
    uint8_t *own;      /* its own pixels, w a row; null a view's or the canvas's */
    uint32_t *palette; /* an image's 256 colours, &HRRGGBB; null none */
} Surface;

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
Pixels surf_pixels(const Surfaces *ss, const Surface *s);
/* h and its views freed. */
void surf_free(Surfaces *ss, uint32_t h);
#endif
