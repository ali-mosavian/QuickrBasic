/* The line map: each editor line's record in the program store (src/ui/lmap.asm). A block
   (allo's HLMAP) of uint32_t: text line i's record's offset, uimn.text.all of them, rising. An
   include's records (LF_INC) are no line's. */
#ifndef UI_LMAP_H
#define UI_LMAP_H
#include "ui/ui.h"

/* lm_init: the map's block (main, after the store's, before the symbols'). */
void lmap_init(void);
/* lm_build: the map from the store; uimn.text.all its lines. Growing it empties the heap first
   (symb_clear). Errors: Out of memory. */
void lmap_build(void);
/* lm_off: text line line's record; past the last, prog.end. */
uint32_t lmap_off(uint32_t line);
/* lm_of: the text line whose record is at rec, or the last before it (an include's records are
   its line's); false: none, rec before the first. */
bool lmap_of(uint32_t rec, uint32_t *line);
#endif
