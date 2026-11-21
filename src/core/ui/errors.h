/* The lines with syntax errors' interface. How it works: errors.c. */
#ifndef UI_ERRORS_H
#define UI_ERRORS_H
#include "ui/ui.h"

/* Text line line (the whole text's) has a syntax error: its column to *col. */
bool errs_at(Session *S, uint32_t line, uint16_t *col);
/* The lines with errors. */
uint32_t errs_count(Session *S);
/* The cursor's line's error's message, one line; null: none, or the line is being changed. */
const char *errs_message(Session *S);
/* Text line line's error's word, on screen row row from column col: drawn in SL_ERR. text, len
   characters, is the line as drawn. */
void errs_paint(
    Session *S,
    uint8_t row,
    uint8_t col,
    uint32_t line,
    const uint8_t *text,
    uint32_t len
);
/* Ctrl+F8: the cursor to the next line with an error, at the error; a beep for none. */
void errs_next(Session *S);
#endif
