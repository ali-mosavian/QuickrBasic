/* What fits at a place in a line's text, ranked. How it works: suggest.c. */
#ifndef COMP_SUGGEST_H
#define COMP_SUGGEST_H
#include "base.h"

enum {
    SUGG_LEN = 40, /* a name's characters at most */
    SUGG_MAX = 64, /* suggestions kept */
};

/* What a suggestion is: a keyword, a function ('(' goes after it), a variable, a SUB, a line
   number, punctuation after the word (the word and it). */
typedef enum : uint8_t { SG_KW, SG_FN, SG_VAR, SG_SUB, SG_LABEL, SG_PUNCT } SuggKind;

typedef struct {
    uint8_t len;
    uint8_t text[SUGG_LEN + 2]; /* in capitals, an array's '(' after it; 0 */
    SuggKind kind;
    bool fuzzy;    /* the word's letters in it in order, not its start */
    uint16_t tok;  /* a keyword's token: 100h + n for TK_X n */
    uint32_t rec;  /* a line number's line, a procedure's first: its record */
    int16_t score; /* higher first */
} Sugg;

typedef struct {
    uint16_t start; /* the word's column */
    uint16_t n;
    Sugg list[SUGG_MAX];
} Suggs;

/* The suggestions for the word ending at col of the line text (n characters), in the program's
   line rec (its record; a new line's: the record it goes before). false: none fit there: in a
   string, a REM or DATA, or after a word's start. */
bool sugg_find(Session *S, const uint8_t *text, uint16_t n, uint16_t col, uint32_t rec, Suggs *out);
/* s was taken: it ranks higher from now on. */
void sugg_used(const Sugg *s);
#endif
