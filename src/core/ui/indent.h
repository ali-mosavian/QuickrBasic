/* The editor's auto indent: a line's indent by the blocks it opens and closes; the block keywords
   it reads. How it works: indent.c. */
#ifndef UI_INDENT_H
#define UI_INDENT_H
#include "base.h"

enum { INDT_STEP = 4 }; /* a block's columns in, a Tab's stop */

/* A block's keyword: its block's kind, whether it opens, is a middle (ELSE, CASE) or ends it;
   in the line's first statement; its columns (END IF's both words). */
typedef enum : uint8_t { K_FOR, K_DO, K_WHILE, K_IF, K_SEL, K_SUB, K_FUNC, K_TYPE, K_DEF } IndtKind;
typedef enum : int8_t { EV_END = -1, EV_MID = 0, EV_OPEN = 1 } IndtWhat;
typedef struct {
    IndtKind kind;
    IndtWhat what;
    bool lead;
    uint16_t col, len;
} IndtEv;

enum { INDT_NEV = 32 };

/* The block keywords of the line's n characters of text, in order, to ev (INDT_NEV at most);
   their count. A NEXT closing two FORs is two. */
uint32_t indt_events(const uint8_t *text, uint16_t n, IndtEv *ev);
/* Line i's text to dst, its length; the lines from 0. */
typedef uint16_t IndtGet(void *ctx, uint32_t i, uint8_t *dst);

/* Enter at the end of line's n characters of text: the indent the line takes (*self, its own
   when no closer starts it) and the new line's after it (*next). The lines above through get. */
void indt_enter(
    IndtGet *get,
    void *ctx,
    uint32_t line,
    const uint8_t *text,
    uint16_t n,
    uint16_t *self,
    uint16_t *next
);
#endif
