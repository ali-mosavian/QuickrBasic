/* A help topic as lines and hot links, and the history of topics shown. How it works: helpdoc.c. */
#ifndef UI_HELPDOC_H
#define UI_HELPDOC_H
#include "base.h"

enum : uint16_t { HDOC_BACK = 0xFFFF, HDOC_NONE = 0xFFFF }; /* a link's topic: Alt+F1's */
enum : uint8_t { HK_TEXT = 0, HK_HEAD = 1, HK_LINK = 2 };    /* a cell's kind */
enum : uint8_t { HIST_MAX = 20 };

typedef struct {
    uint16_t line, col, ecol; /* the body line; its first and last column, the arrows' too */
    uint16_t topic;
    uint8_t letter; /* the label's first character, a capital */
} HelpLink;

typedef struct {
    uint8_t *text; /* the topic's text, a copy */
    uint32_t len, title_len;
    uint32_t *line; /* each body line's offset */
    uint16_t nlines;
    HelpLink *link;
    uint16_t nlinks;
} HelpDoc;

/* text's len bytes made a document; false: out of memory. */
bool hdoc_load(HelpDoc *d, const uint8_t *text, uint32_t len);
void hdoc_free(HelpDoc *d);
/* Body line i's cells: characters in ch, their HK_* in kind, at most max; how many. */
uint16_t hdoc_row(const HelpDoc *d, uint16_t i, uint8_t *ch, uint8_t *kind, uint16_t max);
/* The link by step from cur (-1: none) in order, wrapping; HDOC_NONE if there are none. */
uint16_t hdoc_step(const HelpDoc *d, int32_t cur, int32_t step);
/* The link after cur (before it, backwards) whose label starts with letter, wrapping. */
uint16_t hdoc_letter(const HelpDoc *d, int32_t cur, uint8_t letter, bool backwards);
/* The link under line, col; HDOC_NONE if none. */
uint16_t hdoc_at(const HelpDoc *d, uint16_t line, uint16_t col);
/* The first link on lines first .. first + rows - 1, else the first; HDOC_NONE if none. */
uint16_t hdoc_first(const HelpDoc *d, uint16_t first, uint16_t rows);

/* The word of s's len bytes at column col (a cursor on its end counts), to out, max bytes with the
   0: letters, digits, '.', '_' and a type suffix ($ % & ! #); its length, 0 if none. */
uint16_t hdoc_word(const uint8_t *s, uint16_t len, uint16_t col, char *out, uint16_t max);

/* The topics shown, the last HIST_MAX. */
typedef struct {
    uint16_t id[HIST_MAX];
    uint8_t n;
} HelpHist;
/* id shown: kept, unless it is the one last kept; the oldest goes at HIST_MAX. */
void hhist_push(HelpHist *h, uint16_t id);
/* The last kept, taken off; false: none. */
bool hhist_pop(HelpHist *h, uint16_t *id);
#endif
