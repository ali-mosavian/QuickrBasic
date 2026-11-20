/* The tokenizer's interface. How it works: lexer.c. */
#ifndef COMP_LEXER_H
#define COMP_LEXER_H
#include "defs.h"
#include "fault.h"

/* Streamed text: the window [.., end). more: the window used up, its bytes from *keep on slid to
   its start and more read after them; *keep moved. False: the line has no more, nothing moved. */
typedef struct LexrSource LexrSource;
struct LexrSource {
    bool (*more)(LexrSource *src, const uint8_t **keep);
    uint8_t *end;
};

/* The names a body's words become: intern gives a name's id, made when new. */
typedef struct LexrNames LexrNames;
struct LexrNames {
    uint32_t (*intern)(LexrNames *names, const uint8_t *text, uint8_t len);
};

typedef struct {
    Fault *fault;
    LexrNames *names;
    LexrSource *source; /* g_lxfill: streamed text's, null none */
    const uint8_t *src; /* lxsrc: lexr_line's text, body: for the columns */
    uint8_t *dst;       /* lxdst */
    uint8_t *end;       /* g_lxend: the body's limit, a line's record */
    bool all;           /* g_lxall: every byte's column, for lexr_colof */
    int32_t last;       /* g_lxlst: the last item's body offset, -1 none */
    uint16_t lastcol;   /* g_lxlc: its column */
    bool cols;          /* the editor's: each item's column kept */
    uint16_t *col;      /* g_lxcol: an item's column by its body offset, ncol of them, grown */
    uint32_t ncol;
    uint8_t *in;         /* (ESI) the text read */
    uint8_t *out;        /* (EDI) the body written */
    const uint8_t *word; /* lx.lxs: a word's start */
    bool drained;        /* the source has no more of the line */
    const uint8_t *near; /* lexr_ahead's: an item past it may need the window refilled */
    bool escaped;        /* the line has a TK_X token: DEF SEG, VARSEG, SSEG's warnings need one */
    bool start;          /* the next item starts a statement: the line's, after ':', THEN, ELSE */
    bool from;           /* in a FROM statement: its IMPORT a keyword */
    bool dim;            /* the last item DIM: PUBLIC a keyword */
} Lexr;

/* Errors raise through f; words become names through names. */
void lexr_init(Lexr *lx, Fault *f, LexrNames *names);
/* Text, 0-ended, to body; past its 0. The text is changed: FN joined, E and D upper case. */
uint8_t *lexr_line(Lexr *lx, uint8_t *text, uint8_t *body);
/* lx_kw: the keyword the word at *at spells, stepped past: its token (100h + n for TK_X n), 0 a
   name. */
uint16_t lexr_keyword(const uint8_t **at);
/* The text's column of the body's byte off, lx->all set. */
uint16_t lexr_colof(const Lexr *lx, uint32_t off);
#endif
