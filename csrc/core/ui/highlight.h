/* Syntax highlighting's interface. How it works: highlight.c. */
#ifndef UI_HIGHLIGHT_H
#define UI_HIGHLIGHT_H
#include "ui/ui.h"

typedef struct {
    bool on; /* g_hlon: Options, Syntax Highlighting; starts on */
} Synt;
extern Synt synt;

/* hl_toggle: Options, Syntax Highlighting: on, off. */
void synt_toggle(void);
/* hl_row: the edit window's row whose text starts at row, col: its line, len characters of text,
   coloured from column uimn.ed.left, uimn.ed.width of them; SL_ED's columns left as drawn. Off:
   nothing. The text is copied (to uimn.line): the lexer changes it. */
void synt_row(Session *S, uint8_t row, uint8_t col, const uint8_t *text, uint32_t len);
#endif
