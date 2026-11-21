/* The find bar's interface. How it works: findbar.c. */
#ifndef UI_FINDBAR_H
#define UI_FINDBAR_H
#include "ui/ui.h"

/* The bar's widget, a child of screen, hidden till fbar_open. */
void fbar_build(Widget *screen);
/* Search, Find: the bar on the status row, Find What the selection or the word at the cursor,
   its first match from there selected. */
void fbar_open(Session *S);
/* The bar hidden, the match left selected. */
void fbar_close(void);
/* Screen row row from column col shows text, len characters, of the active window: each match
   in it drawn in SL_FIND while the bar is up. */
void fbar_paint(uint8_t row, uint8_t col, const uint8_t *text, uint32_t len);
#endif
