/* The views's interface. How it works: view.c. */
#ifndef UI_VIEW_H
#define UI_VIEW_H
#include "ui/ui.h"

/* TVIEW */
typedef struct {
    uint32_t first; /* vfirst: its first line in the text */
    uint32_t n;     /* vn: its lines */
    uint8_t kind;   /* vkind: 0 the module, PK_SUB, PK_FUNC */
    uint32_t line;  /* vline, vtop, vcol: the cursor's place left there */
    uint32_t top;
    uint16_t col;
    uint32_t name; /* vname: as typed, "" the module's; its offset in the names (view_name) */
} View;

/* vw_reset: a new or loaded text: its module's lines first, the views, both windows on the
   module, uimn.text.hook set. */
void view_reset(Session *S);
/* vw_apply: the active window's view (uimn.ed.view, 0 when past the views) the text's. */
void view_apply(void);
/* vw_goline: text line line in the active window, its view shown (the cursor where that view
   left it); its line in the view. */
uint32_t view_goline(Session *S, uint32_t line);
/* vw_of: the view text line line is in: the last starting at or before it. */
uint32_t view_of(uint32_t line);
/* View v, one view_of gave (the asm's EDI out of vw_of). */
const View *view_at(uint32_t v);
/* View v's name, 0-ended; "" the module's. Good until the views are scanned again. */
const char *view_name(uint32_t v);
/* View v's procedure: its name; 0 the module's. Uses comp_cbuf. */
uint32_t view_proc(Session *S, uint32_t v);
/* vw_next, vw_prev: Shift+F2, Ctrl+F2: the next view, the one before, in the active window. */
void view_next(Session *S);
void view_prev(Session *S);
/* vw_commit: the line just stored: a SUB or FUNCTION line typed becomes a procedure of its own,
   shown, as QBasic does; a procedure's own first line changed: the names again. */
void view_commit(Session *S);
/* vw_newsub, vw_newfn: Edit, New SUB, New FUNCTION: its name asked, then its view (one there of
   that name's, else a new procedure at the text's end). */
void view_newsub(Session *S);
void view_newfn(Session *S);
/* vw_subs: View, SUBs, F2: the module and the procedures listed; Edit in Active, Edit in Split,
   Delete. */
void view_subs(Session *S);
#endif
