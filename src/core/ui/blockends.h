/* Enter after a block's opener: its end put in below. How it works: blockends.c. */
#ifndef UI_BLOCKENDS_H
#define UI_BLOCKENDS_H
#include "ui/indent.h"

/* Enter at col of line's n characters of text, the rest past col blanks and closing brackets
   alone: the ends of the blocks the line leaves open that no line below ends, innermost first, to
   ends (DIAL_NBLOCK at most); their count. The lines, count in all, through ls. */
uint32_t bend_missing(
    const IndtLines *ls,
    uint32_t line,
    uint32_t count,
    const uint8_t *text,
    uint16_t n,
    uint16_t col,
    const char **ends
);
#endif
