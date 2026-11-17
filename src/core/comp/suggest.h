/* What fits at a place in a line's text, ranked. How it works: suggest.c. */
#ifndef COMP_SUGGEST_H
#define COMP_SUGGEST_H
#include "base.h"

enum {
    SUGG_LEN = 40,  /* a name's characters at most */
    SUGG_MAX = 192, /* suggestions kept */
    SUGG_NEED = 85, /* the score of what the statement needs next */
};

/* What a suggestion is: a keyword, a function ('(' goes after it), a variable, a SUB, a line
   number, punctuation after the word (the word and it). */
typedef enum : uint8_t { SG_KW, SG_FN, SG_VAR, SG_SUB, SG_LABEL, SG_PUNCT } SuggKind;

typedef struct {
    uint8_t len;
    uint8_t text[SUGG_LEN + 2]; /* an array's '(' after it; 0 */
    SuggKind kind;
    bool fuzzy;    /* the word's letters in it in order, not its start */
    uint8_t asis;  /* its first characters as written: a name's, the word typed before a mark; the
                      rest in capitals */
    uint16_t tok;  /* a keyword's token: 100h + n for TK_X n */
    uint32_t rec;  /* a line number's line, a procedure's first: its record */
    int16_t score; /* higher first */
    bool need;     /* the line needs it next: FOR's TO, a jump's line, NEXT's variable */
} Sugg;

typedef struct {
    uint16_t start; /* the word's column */
    uint16_t n;
    Sugg *list; /* cap of them, grown to SUGG_MAX */
    uint32_t cap;
} Suggs;

/* The suggestions for the word ending at col of the line text (n characters), in the program's
   line rec (its record; a new line's: the record it goes before). false: none fit there: in a
   string, a REM or DATA, or after a word's start. */
bool sugg_find(Session *S, const uint8_t *text, uint16_t n, uint16_t col, uint32_t rec, Suggs *out);
/* s was taken: it ranks higher from now on. */
/* The likeliest of s, the word wn characters: shown past the cursor, Tab its taking. One the
   word starts, or with no word one the line needs (FOR's TO, a jump's line); null none. */
const Sugg *sugg_guess(const Suggs *s, uint16_t wn);
void sugg_used(const Sugg *s);
#endif
