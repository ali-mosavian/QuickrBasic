/* Views: the module and each SUB or FUNCTION on its own (src/ui/view.asm).

   The text keeps the module's lines first, then each procedure from its SUB or FUNCTION line to
   the next one's (view_reset orders it so). A view per part, from first, n lines; a window's
   uimn.ed.view its view, applied to the text (text_view) by view_apply. A line added or removed
   (uimn.text.hook) moves the views after the active window's. The cursor's place in a view is
   kept when another is shown. The views' table is view.c's. */
#ifndef UI_VIEW_H
#define UI_VIEW_H
#include "ui/ui.h"

enum {
    NVIEW = 64, /* views at most */
    VNAME = 40, /* a procedure's name's characters at most */
};

/* TVIEW */
typedef struct {
    uint32_t first; /* vfirst: its first line in the text */
    uint32_t n;     /* vn: its lines */
    uint8_t kind;   /* vkind: 0 the module, PK_SUB, PK_FUNC */
    uint32_t line;  /* vline, vtop, vcol: the cursor's place left there */
    uint32_t top;
    uint16_t col;
    char name[VNAME + 1]; /* vname: as typed, "" the module's */
} View;

/* vw_reset: a new or loaded text: its module's lines first, the views, both windows on the
   module, uimn.text.hook set. */
void view_reset(void);
/* vw_apply: the active window's view (uimn.ed.view, 0 when past the views) the text's. */
void view_apply(void);
/* vw_goline: text line line in the active window, its view shown (the cursor where that view
   left it); its line in the view. */
uint32_t view_goline(uint32_t line);
/* vw_of: the view text line line is in: the last starting at or before it. */
uint8_t view_of(uint32_t line);
/* View v, one view_of gave (the asm's EDI out of vw_of). */
const View *view_at(uint8_t v);
/* vw_title: ":" and the active window's view's name appended to the 0-ended title; the module:
   nothing. */
void view_title(char *title);
/* vw_next, vw_prev: Shift+F2, Ctrl+F2: the next view, the one before, in the active window. */
void view_next(void);
void view_prev(void);
/* vw_commit: the line just stored: a SUB or FUNCTION line typed becomes a procedure of its own,
   shown, as QBasic does; a procedure's own first line changed: the names again. */
void view_commit(void);
/* vw_newsub, vw_newfn: Edit, New SUB, New FUNCTION: its name asked, then its view (one there of
   that name's, else a new procedure at the text's end). */
void view_newsub(void);
void view_newfn(void);
/* vw_subs: View, SUBs, F2: the module and the procedures listed; Edit in Active, Edit in Split,
   Delete. */
void view_subs(void);
#endif
