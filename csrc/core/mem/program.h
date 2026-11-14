/* The program store (src/mem/prog.asm): a block (HPROG) of Line records, each its body (the
   lexer's tokens and their 0) then pre bytes of its text before the body (the editor's indent, the
   number as typed). The direct line at DLINE, the program from PROGBASE; a len of 0 ends it, at
   end. A line is named by its offset in the store, which stays when the block moves. */
#ifndef MEM_PROGRAM_H
#define MEM_PROGRAM_H
#include "defs.h"

typedef struct {
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

/* The line numbered num: any so numbered, else the place by number, from the link's cursor (asm
   lnk.cursor) when that is not above it. *line: the line, else the first numbered more, or the
   terminator. true: *line has that number. NONUM is never found. */
bool prog_find(Session *S, uint16_t num, uint32_t *line);
/* Store, replace or delete line num (len, the body's with its 0; 1 deletes), then prog_drop. A
   replaced line goes before the room check. Out of memory. */
void prog_put(Session *S, uint16_t num, const uint8_t *body, uint32_t len);
/* A loaded line, at prog_at, the program not dropped at the end; add_at moves past it. */
void prog_add(Session *S, uint16_t num, const uint8_t *body, uint32_t len);
/* Where prog_add stores: add_at, else the end. */
uint32_t prog_at(Session *S);
/* Position pos: its line, its body offset *off. */
uint32_t prog_lineof(const Prog *pg, uint32_t pos, uint32_t *off);
/* Line's body copied to dst; returns past it. */
uint8_t *prog_body(Session *S, uint32_t line, uint8_t *dst);
/* The record at line removed; returns its size. */
uint32_t prog_cut(Session *S, uint32_t line);
/* A line (num, body, pre, add_flags) stored before the record at line, after prog_drop: the
   store grown should it not fit. Returns its size. Out of memory. */
uint32_t prog_ins(Session *S, uint32_t line, uint16_t num, const uint8_t *body, uint32_t len);
/* After an edit: CONT and the link's cursor end, the frames drop, the terminator at end, then
   symb_clear. The next link empties the cache. */
void prog_drop(Session *S);
/* NEW: no program, no names, no variables, then prog_drop. */
void prog_init(Session *S);
/* The store size bytes at least: grown, the variables dropped first. Out of memory. */
void prog_room(Session *S, uint32_t size);
/* The store's block where it now is: base. Its block's moved routine; main calls it once. */
void prog_place(Session *S);
#endif
