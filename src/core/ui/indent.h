/* The editor's auto indent: a line's indent by the blocks it opens and closes. How it works:
   indent.c. */
#ifndef UI_INDENT_H
#define UI_INDENT_H
#include "base.h"

enum { INDT_STEP = 4 }; /* a block's columns in, a Tab's stop */

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
