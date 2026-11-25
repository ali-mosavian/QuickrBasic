/* The uses of the name at the cursor. How it works: uses.c. */
#ifndef UI_USES_H
#define UI_USES_H
#include "io/dialect.h"
#include "ui/ui.h"

/* The name at the cursor found again if it or its line changed: the active window's, before it
   draws. */
void uses_find(Session *S);
/* A line on screen row row from column col, len characters of text, dialect d's, cols its
   characters' slots or null: each use of the cursor's name in it drawn in SL_OCC. */
void uses_paint(
    Session *S,
    const Dialect *d,
    uint8_t row,
    uint8_t col,
    const uint8_t *text,
    uint32_t len,
    const uint8_t *cols
);
#endif
