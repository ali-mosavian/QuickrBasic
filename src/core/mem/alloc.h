/* The arena allocator's interface. How it works: alloc.c. */
#ifndef MEM_ALLOC_H
#define MEM_ALLOC_H
#include "defs.h"
#include "fault.h"

enum { NBLKH = 8 };                        /* blocks at most */
enum { HCACHE = 0, HPROG = 1, HLMAP = 2 }; /* the code cache's, the store's, the editor's map's */
enum {
    OLDHDRSZ = 0x2400, /* the asm's headers' region, unused: the Addrs after it kept */
    CSSZ = 0x1C00,     /* the control stack, under cs_top (CSLIM .. CSTOP) */
    POKESZ = 0x10000,
};

/* A block's owner: told when the block moves or grows. */
typedef struct AlloOwner AlloOwner;
struct AlloOwner {
    void (*moved)(AlloOwner *o);
};

typedef struct {
    Addr base;        /* its first byte */
    uint32_t cap;     /* its bytes */
    AlloOwner *owner; /* nullptr none */
} AlloBlock;

typedef struct {
    Fault *fault; /* Out of memory */
    uint32_t nblocks;
    AlloBlock block[NBLKH]; /* by handle */
    Addr bot, top;          /* the heap (mm.hbot, mm.htop) */
    Addr arrays;            /* the arrays' data from bot up to here (mm.aend) */
    Addr strings;           /* the strings' from here up to top (mm.sfr) */
    Addr cache;             /* the code cache's block, which never moves (mm.cachebase) */
    Addr cs_lim;            /* the control stack's floor (CSLIM) */
    Addr cs_top;            /* its top, frames down from it (CSTOP) */
    Addr poke;              /* the POKE memory, POKESZ bytes (g_pokeb) */
} Allo;

/* The arena [lo, hi): the heap all of it; then carves the control stack and the POKE memory. */
void allo_init(Allo *al, Fault *f, Addr lo, Addr hi);
/* A fixed region of size bytes, zeroed, 16-aligned, from the heap's bottom. Only before the
   first allo_new. Out of memory past HMIN left. */
Addr allo_carve(Allo *al, uint32_t size);
/* A block of size bytes after the last, the heap smaller by it; only while the heap is empty.
   Returns its handle. Out of memory: no room for it and HMIN, or NBLKH made. */
uint32_t allo_new(Allo *al, uint32_t size, AlloOwner *owner);
/* Block h to size bytes at least, half as many again: the blocks after it move up and each from
   it on is told. false: not grown, the heap not empty or without room. */
bool allo_grow(Allo *al, uint32_t h, uint32_t size);

static inline Addr allo_base(const Allo *const al, uint32_t h) { return al->block[h].base; }
static inline uint32_t allo_cap(const Allo *const al, uint32_t h) { return al->block[h].cap; }
#endif
