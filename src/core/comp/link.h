/* The code cache's interface. How it works: link.c. */
#ifndef COMP_LINK_H
#define COMP_LINK_H
#include "defs.h"
#include "fault.h"
typedef struct Comp Comp;

/* A target link_resolve could not find: its error, at its statement's body offset at in line. */
typedef struct {
    uint32_t line, at;
    uint8_t code;
} LinkMiss;
enum { LINK_MISSES = 32 }; /* kept; more are counted */

/* DIM a(), ours: its array, its line. */
typedef struct {
    uint32_t line;
    uint32_t var;
} LinkEmpty;
enum { LINK_EMPTIES = 32 }; /* kept; more are not */

typedef struct Link {
    Addr end;                   /* cend: the cache's next byte */
    Addr lim;                   /* elim: link_xlate's limit */
    Addr eol;                   /* eeol: the stream's EOL */
    Addr inbeg;                 /* einb: an INPUT's end to fill in, 0 none */
    Addr map;                   /* emap: the stream's map */
    Addr code;                  /* ecode: its code, past the map */
    uint32_t mapnext;           /* the next statement link_xlate maps */
    bool linked;                /* the cache is ready for the program */
    uint32_t cursor;            /* mem/prog's and vm/flow's line cursor; 0 when linked */
    LinkMiss miss[LINK_MISSES]; /* the ledger: targets not found, in program order */
    uint32_t nmiss;
    LinkEmpty empty[LINK_EMPTIES]; /* the DIM a()s, in program order, each array once */
    uint32_t nempty;
    uint32_t *lines; /* the numbered lines by number, hashed, while linked: ref_line's */
    uint32_t lmask;  /* its slots less 1; 0 none */
} Link;

void link_start(Comp *C); /* cm_link: before a run: blocks paired, every line compiled */
typedef void LinkEach(void *ctx, const Failure *f);
/* link_start's work, unlinked: each compile error, then each label not found, to each, raising none
   (a block's pairing still raises its first); how many. */
uint32_t link_report(Comp *C, LinkEach *each, void *ctx);
void link_compile(Comp *C, uint32_t line); /* cm_comp: line compiled to comp_xbuf */
/* comp_xbuf's code (comp.line's) placed at link.end as line's stream, the cache grown should it
   not fit. Out of memory. */
void link_place(Comp *C, uint32_t line);
bool link_xlate(Comp *C, Addr *at); /* comp_xbuf as a stream at *at, to its end; link.code its code;
                                    false: past link.lim */
/* The jumps and line references in line's stream at code (an offset in the cache) set to their
   targets; one not found goes on the ledger. The direct line's (line DLINE) block jumps have none:
   they stay unset, an error if taken. */
void link_resolve(Comp *C, uint32_t line, uint32_t code);
/* The ledger's first error raised, if any: Label not defined, or a block's Syntax error. */
void link_check(Comp *C);
/* DIM a(), ours, on a program line: array var on the ledger of empties. */
void link_empty(Comp *C, uint32_t var);
/* Array var given elements: a DIM's bounds, REDIM, a() = .., passed whole, COMMON. */
void link_given(Comp *C, uint32_t var);
typedef void LinkEmptyEach(void *ctx, const LinkEmpty *e);
/* After a link: each DIM a() whose array no code gives elements, to each; how many. */
uint32_t link_empties(const Comp *C, LinkEmptyEach *each, void *ctx);
/* The size in a cache stream of the op at op: INPUT's start has its end's offset after it. */
uint32_t link_opsize(const uint8_t *op);
/* The Jump of jump op op, a block's, a call's or a loop's; null none. */
Jump *link_jump(uint8_t *op);
void link_walk(const uint8_t *code, void (*each)(uint8_t op, const uint8_t *at)); /* each op */
#endif
