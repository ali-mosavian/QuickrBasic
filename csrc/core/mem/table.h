/* Tables' interface. How it works: table.c. */
#ifndef MEM_TABLE_H
#define MEM_TABLE_H
#include "alloc.h"

enum { TMAXA = 4 }; /* a table's arrays at most */

typedef struct {
    uint32_t block;       /* its block's handle */
    uint32_t cap;         /* its entries */
    uint32_t max;         /*   at most */
    Addr base;            /* the block's base when placed */
    uint32_t narr;        /* its arrays */
    uint32_t size[TMAXA]; /* each one's bytes per entry */
    Addr at[TMAXA];       /* each one's first byte */
} Table;

/* t's block for entries entries; narr, size and max set by the caller. Out of memory. */
void tabl_new(Session *S, Table *t, uint32_t entries, AlloMoved moved);
/* t holds entries entries at least, grown if need be. false: past max, or the heap not empty. */
bool tabl_room(Session *S, Table *t, uint32_t entries);
/* t where its block now is: its arrays moved up to a larger cap, at set. Returns the old base. */
Addr tabl_place(Session *S, Table *t);
#endif
