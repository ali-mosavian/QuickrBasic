/* Syntax highlighting's interface. How it works: highlight.c. */
#ifndef UI_HIGHLIGHT_H
#define UI_HIGHLIGHT_H
#include "io/dialect.h"
#include "ui/ui.h"

enum { SYNT_KEEP = 256 }; /* a line's colours worked out whole: its characters at most */

typedef struct {
    bool on; /* g_hlon: Options, Syntax Highlighting; starts on */
} Synt;
extern Synt synt;

/* hl_toggle: Options, Syntax Highlighting: on, off. */
void synt_toggle(void);
/* hl_row: the edit window's row whose text starts at row, col: its line, len characters of text,
   dialect d's, coloured from column uimn.ed.left, uimn.ed.width of them; SL_ED's columns left as
   drawn. Off: nothing. The text is copied (to uimn.line): the lexer changes it. */
void synt_row(
    Session *S,
    const Dialect *d,
    uint8_t row,
    uint8_t col,
    const uint8_t *text,
    uint32_t len
);
/* The slot of each of the len (SYNT_KEEP at most) characters at text, dialect d's, to cols. */
void synt_cols(
    Session *S,
    const Dialect *d,
    const uint8_t *text,
    uint32_t len,
    uint8_t cols[static SYNT_KEEP]
);
/* cols, else the len (SYNT_KEEP at most) characters' slots, dialect d's, worked out: good till
   the next line's. */
const uint8_t *synt_need(
    Session *S,
    const Dialect *d,
    const uint8_t *text,
    uint32_t len,
    const uint8_t *cols
);
/* A name, where cols colour one (a variable's, a parameter's, a procedure's being declared), starts
   at text[i]: its base's length to *n, its suffix (% & ! # $, 0 none) to *sfx. */
bool synt_name(
    const uint8_t *text,
    uint32_t len,
    const uint8_t *cols,
    uint32_t i,
    uint32_t *n,
    uint8_t *sfx
);
/* synt_row's colours from a line's cols, len characters: those shown from uimn.ed.left. */
void synt_paint(uint8_t row, uint8_t col, const uint8_t *cols, uint32_t len);
#endif
