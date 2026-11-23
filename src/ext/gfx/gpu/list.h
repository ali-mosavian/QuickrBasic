/* The list a GPU's host plays (qxlist.h), built as a frame is drawn: its words, the vertices its
 * draws read, and the memory its uploads point at, all kept until the host has played it.
 *
 * # How it works
 * VERTICES is the first op, its count and pointer filled in when the list ends. A draw with the
 * open draw's texture, blend and alpha, and no op between, adds to it: a batch.
 */
#ifndef GFX_GPU_LIST_H
#define GFX_GPU_LIST_H
#include "qxlist.h"

typedef struct {
    uint32_t *words;
    uint32_t n, cap;
    QxVertex *vertices;
    uint32_t nvertices, capvertices;
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
/* n vertices drawn, three a triangle: false, no memory. */
bool list_draw(
    List *l,
    const QxVertex *v,
    uint32_t n,
    uint32_t texture,
    QxBlend blend,
    uint8_t alpha
);
/* END appended, VERTICES filled in: the words to play, *n of them; null, no memory. */
const uint32_t *list_end(List *l, uint32_t *n);
/* Played: emptied, its kept memory freed. */
void list_reset(List *l);
#endif
