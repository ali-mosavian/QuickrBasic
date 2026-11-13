/* The mouse in the editor, when a driver is there (src/ui/mouse.asm). mous_poll turns the left
   button into events, uimn.mouse.event at uimn.mouse.row, col; uimn_getkey gives one as
   KEY_MOUSE. Its state is ui.h's uimn.mouse. */
#ifndef UI_MOUS_H
#define UI_MOUS_H
#include "ui/ui.h"

/* uimn.mouse.event */
enum : uint8_t {
    MS_PRESS = 1, /* the left button went down */
    MS_DRAG = 2,  /* moved while down */
    MS_UP = 3,    /* let go */
    MS_MOVE = 4,  /* moved with none down, while uimn.mouse.hover */
};

/* ms_init: uimn.mouse.on when a driver answers (pmou_init). */
void mous_init(void);
/* ms_show, ms_hide: its cursor on; off, before the screen changes under it. */
void mous_show(void);
void mous_hide(void);
/* ms_poll: the left button's change since the last poll; true: an event, in uimn.mouse. */
bool mous_poll(void);
/* ms_title: the menu whose title is at column col of the bar; false: none. */
bool mous_title(uint8_t col, uint8_t *menu);
/* ms_main: the event in the editor: a title opens its menu, a press puts the cursor there, a drag
   selects; the scroll bar; another window's focus; the status bar's keys; a move the debug
   pane's tooltip; the pane's own. */
void mous_main(void);
#endif
