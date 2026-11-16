/* Tables' interface. How it works: table.c. */
#ifndef MEM_TABLE_H
#define MEM_TABLE_H
#include "alloc.h"

enum { TMAXA = 5 }; /* a table's arrays at most */

typedef struct Table Table;
struct Table {
    AlloOwner owner; /* its block's */
    Allo *allo;      /* the block's allocator */
    void (*moved)(
        Table *t,
        Addr old
    );                    /* after it is placed again, old its base before; null none */
    uint32_t block;       /* its block's handle */
    uint32_t cap;         /* its entries */
    uint32_t max;         /*   at most */
    Addr base;            /* the block's base when placed */
    uint32_t narr;        /* its arrays */
    uint32_t size[TMAXA]; /* each one's bytes per entry */
    Addr at[TMAXA];       /* each one's first byte */
};

/* t's block for entries entries, from al; narr, size, max and moved set by the caller. Out of
   memory. */
void tabl_new(Allo *al, Table *t, uint32_t entries);
/* t holds entries entries at least, grown if need be. false: past max, or the heap not empty. */
bool tabl_room(Table *t, uint32_t entries);
#endif
