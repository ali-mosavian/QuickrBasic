/* The menu bar's interface. How it works: menu.c. */
#ifndef UI_MENU_H
#define UI_MENU_H
#include "ui/ui.h"
#include "ui/theme.h"

enum { NMENU = 8 };

/* An item's flags. */
enum : uint8_t {
    MF_GRAY = 1, /* always gray */
    MF_SEP = 2,  /* a separator */
    MF_CHK = 4,  /* a mark when its flag is on (menu.c's table: dbug.trace, uimn.text.check,
                    them.cur, synt.on) */
    MF_SEL = 8,  /* gray without a selection */
    MF_CLP = 16, /* gray with the clipboard empty */
    MF_ED = 32,  /* gray in the immediate window */
    MF_SUB = 64, /* opens a sub-menu */
};

/* The commands (the asm's A_NEW ..), uimn_act's. */
enum : uint8_t {
    ACT_NEW = 1,
    ACT_OPEN,
    ACT_SAVE,
    ACT_SAVEAS,
    ACT_EXIT,
    ACT_CUT,
    ACT_COPY,
    ACT_PASTE,
    ACT_CLEAR,
    ACT_OUT,
    ACT_FIND,
    ACT_FIND3,
    ACT_CHANGE,
    ACT_START,
    ACT_CONT,
    ACT_SYNTAX,
    ACT_ABOUT,
    ACT_STEP,
    ACT_PSTEP,
    ACT_TRACE,
    ACT_BP,
    ACT_BPCLR,
    ACT_SETNX,
    ACT_RESTART,
    ACT_SPLIT,
    ACT_SUBS,
    ACT_NSUB,
    ACT_NFN,
    ACT_THQB,
    ACT_THDK,
    ACT_SYNHL,
    ACT_VSPLIT,
    ACT_TH2, /* themes 2 .. NTHEME-1 */
    ACT_SOUT = ACT_TH2 + NTHEME - 2,
    ACT_NONE,
};
static_assert(ACT_TH2 == 33 && ACT_NONE == 41);

/* mn_x: each title's column on the bar (QBasic's). */
extern const uint8_t menu_x[NMENU];
/* mn_tt: each title, '&' before its key. */
extern const char *const menu_title[NMENU];

/* mn_bar: the menu bar; the open menu's title lit. */
void menu_bar(void);
/* The menus' modal widget, a child of screen, hidden till menu_run. */
void menu_build(Widget *screen);
/* mn_run: the menus from menu m: open, its box open, else the bar lit; until a command (then
   uimn_act) or Esc. The line committed first. */
void menu_run(Session *S, uint8_t m, bool open);
/* mn_alt: the menu whose Alt+letter has scan code scan, opened (menu_run); false: not one. */
bool menu_alt(Session *S, uint8_t scan);
/* mn_altl: the letter of the key with scan code scan (10h .. 32h), as Alt+letter gives no
   character; 0 out of that range, ' ' none there. */
uint8_t menu_altchar(uint8_t scan);
#endif
