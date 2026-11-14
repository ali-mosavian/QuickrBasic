/* Listing's interface. How it works: listing.c. */
#ifndef COMP_LISTING_H
#define COMP_LISTING_H
#include "defs.h"
typedef struct Comp Comp;

typedef struct List {
    const uint8_t *at; /* g_lsat: list_text's byte, null none */
    uint32_t col;      /* g_lscol: its column */
    uint8_t last;      /* io.lq: the last item's class (listing.c's LS_*) */
} List;

void list_range(Comp *C, uint16_t from, uint16_t to); /* ls_range: the lines numbered from..to */
void list_body(Comp *C, const uint8_t *body);         /* ls_body: print a token body */
uint32_t list_text(
    Comp *C,
    const uint8_t *body,
    uint32_t at
); /* ls_text: body spelled into io's sbuf,
                                                         0 after; the column of its byte at */
#endif
