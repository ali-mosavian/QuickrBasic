/* The mouse's interface. How it works: mouse.c. */
#ifndef UI_MOUSE_H
#define UI_MOUSE_H
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
/* ms_main, by widget (WidgetOps.mouse): the screen's, a move: the debug pane's tooltip; the menu
   bar's, a press on a title: its menu; the status bar's: its key; the windows'. */
bool mous_screen(Widget *w, Session *S, uint8_t row, uint8_t col);
bool mous_menubar(Widget *w, Session *S, uint8_t row, uint8_t col);
bool mous_statusbar(Widget *w, Session *S, uint8_t row, uint8_t col);
bool mous_workspace(Widget *w, Session *S, uint8_t row, uint8_t col);
bool mous_window(Widget *w, Session *S, uint8_t row, uint8_t col); /* an edit or immediate one */
bool mous_pane(Widget *w, Session *S, uint8_t row, uint8_t col);
#endif
