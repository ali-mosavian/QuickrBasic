/* Find What's interface. How it works: search.c. */
#ifndef UI_SEARCH_H
#define UI_SEARCH_H
#include "ui/ui.h"

/* A place in the whole text. */
typedef struct {
    uint32_t line;
    uint16_t col;
} SrchAt;

/* c in a word, as Whole Word and the word at the cursor take it. */
bool srch_wordc(uint8_t c);
/* Find What at column col of text, len characters, as Match Case and Whole Word say. */
bool srch_match(const uint8_t *text, uint16_t len, uint16_t col);
/* The first match at or past *at, or with back the last before it, through the whole text and
   round, to *at; false: none. */
bool srch_find(Session *S, SrchAt *at, bool back);
/* Where a search from the cursor starts: forward past the cursor, or at its end with a
   selection; back at the selection's start, else the cursor. */
SrchAt srch_from(bool back);
/* Match at selected in its view, the cursor at its end. */
void srch_show(Session *S, SrchAt at);
/* The matches in the whole text; *nth: at's among them from 1, 0 when at is not one. */
uint32_t srch_count(Session *S, SrchAt at, uint32_t *nth);
/* Find What made the one-line selection, else the word at the cursor or just before it; true:
   it was, its column to *from. Else it stays. */
bool srch_default(uint16_t *from);
#endif
