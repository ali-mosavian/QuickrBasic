/* A stop's values beside the code. How it works: debuginline.c. */
#ifndef UI_DEBUGINLINE_H
#define UI_DEBUGINLINE_H
#include "io/dialect.h"
#include "ui/ui.h"

/* A line on screen row row from column col, len characters of text, dialect d's, cols its
   characters' slots or null: at a stop in its view, the values of the numbers and strings it
   names after its end, in SL_HINT. */
void dinl_paint(
    Session *S,
    const Dialect *d,
    uint8_t row,
    uint8_t col,
    const uint8_t *text,
    uint32_t len,
    const uint8_t *cols
);
#endif
