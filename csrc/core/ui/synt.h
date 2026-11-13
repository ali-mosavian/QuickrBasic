/* Syntax highlighting: a line's keywords, strings, numbers, comments and names in the theme's
   colours (src/ui/syntax.asm). The line is tokenized as the compiler does (lexr_line with
   lexr.col); each item of the body gives its slot to the columns from its column to the next
   item's. After AS or TYPE a type; after SUB, FUNCTION, DEF a name being declared, then its
   parameters in the ( ) after it. */
#ifndef UI_SYNT_H
#define UI_SYNT_H
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
void synt_row(uint8_t row, uint8_t col, const uint8_t *text, uint32_t len);
#endif
