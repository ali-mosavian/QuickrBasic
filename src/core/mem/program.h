/* The program store's interface. How it works: program.c. */
#ifndef MEM_PROGRAM_H
#define MEM_PROGRAM_H
#include "defs.h"
#include "fault.h"
#include "mem/alloc.h"

/* What the store tells its user (the session). */
typedef struct ProgUser ProgUser;
struct ProgUser {
    void (*edited)(ProgUser *u);  /* a line stored: what points into the store drops */
    void (*growing)(ProgUser *u); /* the store grows next: the heap emptied first */
};

typedef struct {
    AlloOwner owner; /* its block's */
    Allo *allo;
    Fault *fault; /* Out of memory */
    ProgUser *user;
    Addr base;          /* the block's base (mm.progbase) */
    uint32_t end;       /* the terminator's offset (mm.progend) */
    uint32_t entering;  /* the stored line being entered (mm.enterat) */
    const uint8_t *pre; /* the next line's text before its body; used up by a store */
    uint32_t pre_len;
    uint8_t add_flags; /* the lines stored: their LF_* */
    uint32_t add_at;   /* prog_add's place, 0 the end */
} Prog;

static inline Addr prog_addr(const Prog *const pg, const uint32_t line) { return pg->base + line; }
static inline Line *prog_line(const Prog *const pg, const uint32_t line)
{
    return (Line *)at(pg->base + line);
}

/* Records move: dst, src offsets in the store. */
void prog_copy(Prog *pg, uint32_t dst, uint32_t src, uint32_t len);
/* The terminator at end. */
void prog_terminate(Prog *pg);
/* The line numbered num: any so numbered, else the place by number, from cursor (the link's, asm
   lnk.cursor) when that is not above it. *line: the line, else the first numbered more, or the
   terminator. true: *line has that number. NONUM is never found. */
bool prog_find(const Prog *pg, uint32_t cursor, uint16_t num, uint32_t *line);
/* Store, replace or delete line num (len, the body's with its 0; 1 deletes), then the user's
   edited. A replaced line goes before the room check. Out of memory. */
void prog_put(Prog *pg, uint32_t cursor, uint16_t num, const uint8_t *body, uint32_t len);
/* A loaded line, at prog_at; add_at moves past it. */
void prog_add(Prog *pg, uint16_t num, const uint8_t *body, uint32_t len);
/* Where prog_add stores: add_at, else the end. */
uint32_t prog_at(const Prog *pg);
/* Position pos: its line, its body offset *off. */
uint32_t prog_lineof(const Prog *pg, uint32_t pos, uint32_t *off);
/* Line's body copied to dst; returns past it. */
uint8_t *prog_body(const Prog *pg, uint32_t line, uint8_t *dst);
/* The record at line removed; returns its size. */
uint32_t prog_cut(Prog *pg, uint32_t line);
/* A line (num, body, pre, add_flags) stored before the record at line, after the user's edited:
   the store grown should it not fit. Returns its size. Out of memory. */
uint32_t prog_ins(Prog *pg, uint32_t line, uint16_t num, const uint8_t *body, uint32_t len);
/* The store size bytes at least: grown, the user's growing first. Out of memory. */
void prog_room(Prog *pg, uint32_t size);
/* The store's block, 64 KB from al, after the cache's. */
void prog_new(Prog *pg, Allo *al, Fault *f, ProgUser *user);
#endif
