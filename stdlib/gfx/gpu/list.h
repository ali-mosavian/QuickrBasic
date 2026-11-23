/* The list a GPU's host plays (qxlist.h), built as a frame is drawn: its words, the vertices and
 * discs its draws read, their indices, and the memory its uploads point at, all kept until the
 * host has played it.
 *
 * # How it works
 * The vertices, the discs and the indices are a buffer each, made and written by the list's first
 * ops, which are filled in when it ends. A quad is four vertices, its indices 0 1 2 0 2 3; they
 * are written only when a list draws more quads than they cover. A draw with the open draw's
 * pipeline, buffer, textures and alpha, no op between, adds to it: a batch.
 */
#ifndef GFX_GPU_LIST_H
#define GFX_GPU_LIST_H
#include "gpu/vertex.h"
#include "qxlist.h"

/* The list's buffers; a draw's textures. */
enum { LIST_VERTICES = 1, LIST_DISCS, LIST_INDICES, LIST_TEXTURES = 2 };

typedef struct {
    uint32_t *words;
    uint32_t n, cap;
    GpuVertex *vertices;
    uint32_t nvertices, capvertices;
    GpuDisc *discs;
    uint32_t ndiscs, capdiscs;
    uint32_t quads; /* the quads the indices cover */
    void **kept;
    uint32_t nkept, capkept;
    uint32_t draw;    /* the open draw's first word; 0 none */
    uint32_t batches; /* its DRAWs */
    uint32_t uploads; /* its UPLOADs */
} GpuList;

/* An empty list, its buffers none yet: false, no memory. */
bool list_init(GpuList *l);
void list_done(GpuList *l);
/* op and its n arguments appended: false, no memory. */
bool list_op(GpuList *l, QxListOp op, const uint32_t *args, uint32_t n);
/* p's two words into args. */
void list_pointer(uint32_t *args, const void *p);
/* bytes kept until the list is played: null, no memory. */
void *list_keep(GpuList *l, uint32_t bytes);
/* n vertices drawn, four a quad, through pipeline from textures: false, no memory. */
bool list_draw(
    GpuList *l,
    const GpuVertex *v,
    uint32_t n,
    uint32_t pipeline,
    const uint32_t textures[LIST_TEXTURES],
    uint8_t alpha
);
/* n disc vertices drawn, as list_draw's. */
bool list_discs(
    GpuList *l,
    const GpuDisc *v,
    uint32_t n,
    uint32_t pipeline,
    const uint32_t textures[LIST_TEXTURES],
    uint8_t alpha
);
/* END appended, the buffers' ops filled in: the words to play, *n of them; null, no memory. */
const uint32_t *list_end(GpuList *l, uint32_t *n);
/* Played: emptied, its kept memory freed. */
void list_reset(GpuList *l);
#endif
