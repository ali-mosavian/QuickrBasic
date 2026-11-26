/* The editor's auto indent: a line's indent by the blocks it opens and closes; the block keywords
   it reads. How it works: indent.c. */
#ifndef UI_INDENT_H
#define UI_INDENT_H
#include "base.h"
#include "fault.h"
#include "io/dialect.h"

enum { INDT_STEP = 4 }; /* a block's columns in, a Tab's stop */

/* Line i's text to dst, its length; the lines from 0. */
typedef uint16_t IndtGet(void *ctx, uint32_t i, uint8_t *dst);

/* A view's lines: their dialect, each one's text through get with ctx. */
typedef struct {
    const Dialect *d;
    Fault *f;
    IndtGet *get;
    void *ctx;
} IndtLines;

/* The block keywords of the line's n characters of text, in ls's dialect, in order, to ev
   (DIAL_NBLOCK at most); their count. QBasic's are the editor's own: a NEXT closing two FORs is
   two. */
uint32_t indt_blocks(const IndtLines *ls, const uint8_t *text, uint16_t n, DialBlock *ev);
/* The spaces the line's n characters of text start with. */
uint16_t indt_lead(const uint8_t *text, uint16_t n);
/* Enter at the end of line's n characters of text: the indent the line takes (*self, its own
   when no closer starts it) and the new line's after it (*next). The lines above through ls. */
void indt_enter(
    const IndtLines *ls,
    uint32_t line,
    const uint8_t *text,
    uint16_t n,
    uint16_t *self,
    uint16_t *next
);
#endif
