/* The tokenizer (comp/lex.asm): a typed line becomes the body of a stored line. In the body:
     M_VAR len name nam:2  a name as typed, its suffix too; nam its entry in the name table
     M_INT n:4             a whole number under 2^31, &H, &O
     a token               80h..FEh, or TK_X and a byte (comp/dfa.h)
   Spaces go, but in strings, REM and DATA. A number with a point, an exponent or 2^31 and over
   stays text. A keyword is a whole word (the scanner's automaton), any other word a name.
   lexr.col: an item's column in the text, by its body offset. */
#ifndef COMP_LEXR_H
#define COMP_LEXR_H
#include "defs.h"

/* Streamed text's reader: the window used up, its bytes from *keep on slid to its start and more
   read after them; *keep moved. False: the line has no more, nothing moved. */
typedef bool LexrMore(const uint8_t **keep);

typedef struct {
    const uint8_t *src;    /* lxsrc: lexr_line's text, body: for the columns */
    uint8_t *dst;          /* lxdst */
    uint8_t *end;          /* g_lxend: the body's limit, a line's record */
    LexrMore *more;        /* g_lxfill: srcr_more, null none; lx_ahead reads srcr.end */
    bool all;              /* g_lxall: every byte's column, for lexr_colof */
    int32_t last;          /* g_lxlst: the last item's body offset, -1 none */
    uint16_t lastcol;      /* g_lxlc: its column */
    uint16_t col[LXCMAX];  /* g_lxcol: an item's column by its body offset */
} Lexr;
extern Lexr lexr;

uint8_t *lexr_line(const uint8_t *text, uint8_t *body); /* text, 0-ended, to body; past its 0 */
uint16_t lexr_keyword(const uint8_t **at); /* lx_kw: the keyword the word at *at spells, stepped
                                              past: its token (100h + n for TK_X n), 0 a name */
#if IDE
uint16_t lexr_colof(uint32_t off); /* the text's column of the body's byte off, lexr.all set */
#endif
#endif
