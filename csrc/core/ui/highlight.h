/* Syntax highlighting's interface. How it works: highlight.c. */
#ifndef UI_HIGHLIGHT_H
#define UI_HIGHLIGHT_H
#include "ui/ui.h"

enum { SYNT_KEEP = 256 }; /* a line's colours worked out whole: its characters at most */

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
/* The slot of each of the len (SYNT_KEEP at most) characters at text, to cols. */
void synt_cols(Session *S, const uint8_t *text, uint32_t len, uint8_t cols[static SYNT_KEEP]);
/* synt_row's colours from a line's cols, len characters: those shown from uimn.ed.left. */
void synt_paint(uint8_t row, uint8_t col, const uint8_t *cols, uint32_t len);
#endif
