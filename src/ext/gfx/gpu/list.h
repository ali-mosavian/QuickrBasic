/* The list a GPU's host plays (qxlist.h), built as a frame is drawn: its words, the vertices its
 * draws read, and the memory its uploads point at, all kept until the host has played it.
 *
 * # How it works
 * VERTICES and DISCS are the first ops, their counts and pointers filled in when the list ends. A
 * draw of the open draw's kind, texture, blend and alpha, with no op between, adds to it: a batch.
 */
#ifndef GFX_GPU_LIST_H
#define GFX_GPU_LIST_H
#include "qxlist.h"

typedef struct {
    uint32_t *words;
    uint32_t n, cap;
    QxVertex *vertices;
    uint32_t nvertices, capvertices;
    QxDisc *discs;
    uint32_t ndiscs, capdiscs;
    void **kept;
    uint32_t nkept, capkept;
    uint32_t draw; /* the open draw's first word; 0 none */
} List;

/* An empty list: false, no memory. */
bool list_init(List *l);
void list_done(List *l);
/* op and its n arguments appended: false, no memory. */
bool list_op(List *l, QxListOp op, const uint32_t *args, uint32_t n);
/* p's two words into args. */
void list_pointer(uint32_t *args, const void *p);
/* bytes kept until the list is played: null, no memory. */
void *list_keep(List *l, uint32_t bytes);
/* n vertices drawn, four a quad: false, no memory. */
bool list_draw(
    List *l,
    const QxVertex *v,
    uint32_t n,
    uint32_t texture,
    QxBlend blend,
    uint8_t alpha
);
/* n disc vertices drawn, four a quad: false, no memory. */
bool list_discs(
    List *l,
    const QxDisc *v,
    uint32_t n,
    uint32_t texture,
    QxBlend blend,
    uint8_t alpha
);
/* END appended, VERTICES and DISCS filled in: the words to play, *n of them; null, no memory. */
const uint32_t *list_end(List *l, uint32_t *n);
/* Played: emptied, its kept memory freed. */
void list_reset(List *l);
#endif
