/* The line map's interface. How it works: linemap.c. */
#ifndef UI_LINEMAP_H
#define UI_LINEMAP_H
#include "ui/ui.h"

/* lm_init: the map's block (main, after the store's, before the symbols'). */
void lmap_init(Session *S);
/* lm_build: the map from the store; uimn.text.all its lines. Growing it empties the heap first
   (symb_clear). Errors: Out of memory. */
void lmap_build(Session *S);
/* lm_off: text line line's record; past the last, prog.end. */
uint32_t lmap_off(Session *S, uint32_t line);
/* lm_of: the text line whose record is at rec, or the last before it (an include's records are
   its line's); false: none, rec before the first. */
bool lmap_of(Session *S, uint32_t rec, uint32_t *line);
#endif
