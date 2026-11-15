/* The editor's and the output's pages. How it works: pages.c. */
#ifndef UI_PAGES_H
#define UI_PAGES_H
#include "session.h"

enum : uint8_t { PAGE_EDIT = 0, PAGE_OUT = 1 };
enum : uint8_t { VIDEO_TEXT = 3 }; /* pscr_mode's 80x25 colour text */

/* The block a program's graphics screen is kept in, after the editor's line map (HLMAP). */
void upag_init(Session *S);
/* ui_in: the editor's page shown; a program's graphics screen kept, the screen in text mode
   (draw_all draws the editor's again). */
void upag_in(Session *S);
/* ui_out: the output's page shown, or the program's graphics screen again. */
void upag_out(Session *S);
/* ui_outclr: the output's page shown and cleared, in text mode: a run's start. */
void upag_clear(Session *S);
#endif
