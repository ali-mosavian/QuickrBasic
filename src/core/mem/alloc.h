/* The arena allocator's interface. How it works: alloc.c. */
#ifndef MEM_ALLOC_H
#define MEM_ALLOC_H
#include "defs.h"
#include "fault.h"

enum { NBLKH = 8 };                        /* blocks at most */
enum { HCACHE = 0, HPROG = 1, HLMAP = 2 }; /* the code cache's, the store's, the editor's map's */
enum {
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
    Addr bot, top;          /* the BASIC heap (mm.hbot, mm.htop): a block of pmem's, grown up */
    Addr arrays;            /* the arrays' data from bot up to here (mm.aend) */
    Addr strings;           /* the strings' from here up to top (mm.sfr) */
    Addr cache;             /* the code cache's block, which never moves (mm.cachebase) */
    Addr cs_lim;            /* the control stack's floor (CSLIM) */
    Addr cs_top;            /* its top, frames down from it (CSTOP) */
    Addr poke;              /* the POKE memory, POKESZ bytes (g_pokeb); 0 till the first SYS */
} Allo;

/* The BASIC heap, HEAP_FIRST bytes at the bottom of pmem's largest room; then carves the
   control stack. Out of memory. */
void allo_init(Allo *al, Fault *f);
/* A fixed region of size bytes, zeroed, 16-aligned: it never moves. Out of memory. */
Addr allo_carve(Allo *al, uint32_t size);
/* A block of size bytes, zeroed. Returns its handle. Out of memory: no room, or NBLKH made. */
uint32_t allo_new(Allo *al, uint32_t size, AlloOwner *owner);
/* Block h to size bytes at least, half as many again where there is room: in place or moved, its
   owner told. false: no room. */
bool allo_grow(Allo *al, uint32_t h, uint32_t size);
/* The BASIC heap's top raised by need bytes at least, a quarter of it more where there is room:
   the bytes added, past top; 0 no room. The caller moves top. */
uint32_t allo_extend(Allo *al, uint32_t need);
/* The strings' room (FRE("")): the BASIC heap's free bytes, and what it can still add. */
uint32_t allo_string_room(const Allo *al);
/* The free bytes (FRE(-1)): the BASIC heap's, and every region's under REF_ADDR. */
uint32_t allo_free(const Allo *al);
/* The BASIC heap, empty, back to HEAP_FIRST bytes: the rest to pmem. */
void allo_shrink(Allo *al);

static inline Addr allo_base(const Allo *const al, uint32_t h) { return al->block[h].base; }
static inline uint32_t allo_cap(const Allo *const al, uint32_t h) { return al->block[h].cap; }
#endif
