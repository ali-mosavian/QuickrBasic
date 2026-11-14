/* The tokenizer's interface. How it works: lexer.c. */
#ifndef COMP_LEXER_H
#define COMP_LEXER_H
#include "defs.h"

/* Streamed text's reader: the window used up, its bytes from *keep on slid to its start and more
   read after them; *keep moved. False: the line has no more, nothing moved. */
typedef bool LexrMore(Session *S, const uint8_t **keep);

typedef struct {
    const uint8_t *src; /* lxsrc: lexr_line's text, body: for the columns */
    uint8_t *dst;       /* lxdst */
    uint8_t *end;       /* g_lxend: the body's limit, a line's record */
    LexrMore *more;     /* g_lxfill: srcr_more, null none; lx_ahead reads srcr.end */
    bool all;           /* g_lxall: every byte's column, for lexr_colof */
    int32_t last;       /* g_lxlst: the last item's body offset, -1 none */
    uint16_t lastcol;   /* g_lxlc: its column */
    uint16_t *col;      /* g_lxcol: an item's column by its body offset, ncol of them; the
                              editor's, nullptr none */
    uint32_t ncol;
    uint8_t *in;         /* (ESI) the text read */
    uint8_t *out;        /* (EDI) the body written */
    const uint8_t *word; /* lx.lxs: a word's start */
} Lexr;

uint8_t *lexr_line(
    Session *S,
    uint8_t *text,
    uint8_t *body
);                                         /* text, 0-ended, to body; past its 0. The text
                                                      is changed: FN joined, E and D upper case */
uint16_t lexr_keyword(const uint8_t **at); /* lx_kw: the keyword the word at *at spells, stepped
                                              past: its token (100h + n for TK_X n), 0 a name */
uint16_t lexr_colof(
    Session *S,
    uint32_t off
); /* the text's column of the body's byte off, lexr.all set */
#endif
