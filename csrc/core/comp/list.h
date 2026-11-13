/* LIST, SAVE and VERIFY: a stored body back to text (comp/list.asm), spaced as QBasic's editor:
   keywords from their tokens, a name from its M_VAR, a whole number from its M_INT; strings, REM
   and DATA verbatim. A space between words, around an operator but a sign, after , ; : and none
   inside ( ). Output goes through io's current channel. */
#ifndef COMP_LIST_H
#define COMP_LIST_H
#include "defs.h"

typedef struct {
    const uint8_t *at; /* g_lsat: list_text's byte, null none */
    uint32_t col;      /* g_lscol: its column */
    uint8_t last;      /* io.lq: the last item's class (list.c's LS_*) */
} List;
extern List list;

void list_range(uint16_t from, uint16_t to); /* ls_range: the lines numbered from..to */
void list_body(const uint8_t *body);         /* ls_body: print a token body */
uint32_t list_text(const uint8_t *body, uint32_t at); /* ls_text: body spelled into io's sbuf,
                                                         0 after; the column of its byte at */
#endif
