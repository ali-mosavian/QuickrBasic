/* The arena's layout and the allocator: blocks packed from the heap's start (src/mem/alloc.asm,
   the regions of src/regions.asm and rt_start).

   The arena, [lo, hi) from pmem_init, is carved at start: first the fixed regions (allo_carve:
   the arrays' headers and the control stack, the POKE memory, and any other module's region a
   stored Addr can reach), then the blocks (allo_new: the code cache, the program store, the
   editor's line map, the symbol tables), then the BASIC heap, bot to top: arrays up from bot
   (arry.end), strings down from top (strg.low). A block is reached by its handle, never by a
   stored Addr: it moves when one before it grows. */
#ifndef MEM_ALLOC_H
#define MEM_ALLOC_H
#include "defs.h"

enum { NBLKH = 8 };                        /* blocks at most */
enum { HCACHE = 0, HPROG = 1, HLMAP = 2 }; /* the code cache's, the store's, the editor's map's */
enum {
    HDRSZ = 0x2400, /* the arrays' headers (the asm's g_dsb+BC00h .. HDRLIM) */
    CSSZ = 0x1C00,  /* the control stack, under cs_top (CSLIM .. CSTOP) */
    POKESZ = 0x10000,
};

typedef void (*AlloMoved)(Session *S); /* a block's: called when it moves or grows */

typedef struct {
    Addr base;       /* its first byte */
    uint32_t cap;    /* its bytes */
    AlloMoved moved; /* nullptr none */
} AlloBlock;

typedef struct {
    uint32_t nblocks;
    AlloBlock block[NBLKH]; /* by handle */
    Addr bot, top;      /* the heap (mm.hbot, mm.htop) */
    Addr cache;         /* the code cache's block, which never moves (mm.cachebase) */
    Addr hdr_base;      /* the arrays' headers, to hdr_base + HDRSZ (HDRBASE, HDRLIM) */
    Addr cs_lim;        /* the control stack's floor (CSLIM) */
    Addr cs_top;        /* its top, frames down from it (CSTOP) */
    Addr poke;          /* the POKE memory, POKESZ bytes (g_pokeb) */
} Allo;

/* The arena [lo, hi): the heap all of it; then carves the headers, the control stack and the
   POKE memory. */
void allo_init(Session *S, Addr lo, Addr hi);
/* A fixed region of size bytes, zeroed, 16-aligned, from the heap's bottom. Only before the
   first allo_new. Out of memory past HMIN left. */
Addr allo_carve(Session *S, uint32_t size);
/* A block of size bytes after the last, the heap smaller by it; only while the heap is empty.
   Returns its handle. Out of memory: no room for it and HMIN, or NBLKH made. */
uint32_t allo_new(Session *S, uint32_t size, AlloMoved moved);
/* Block h to size bytes at least, half as many again: the blocks after it move up and each from
   it on is told. false: not grown, the heap not empty or without room. */
bool allo_grow(Session *S, uint32_t h, uint32_t size);

static inline Addr allo_base(const Allo *const al, uint32_t h) { return al->block[h].base; }
static inline uint32_t allo_cap(const Allo *const al, uint32_t h) { return al->block[h].cap; }
#endif
