/* The views's interface. How it works: view.c. */
#ifndef UI_VIEW_H
#define UI_VIEW_H
#include "ui/dialog.h"
#include "ui/ui.h"

/* TVIEW */
typedef struct {
    uint32_t first;  /* vfirst: its first line in the text */
    uint32_t n;      /* vn: its lines */
    uint8_t kind;    /* vkind: 0 its file's module, PK_SUB, PK_FUNC */
    uint32_t module; /* its file's (mem/module.h) */
    uint32_t line;  /* vline, vtop, vcol: the cursor's place left there */
    uint32_t top;
    uint16_t col;
    uint32_t name; /* vname: as typed, "" the module's; its offset in the names (view_name) */
} View;

/* vw_reset: a new or loaded text: each file's module's lines first, the views, both windows on
   the main's module, uimn.text.hook set. */
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
/* View v's file's module, 0 the main's; its file's name, the main's as uimn.file (Untitled). */
uint32_t view_module(uint32_t v);
const char *view_file(Session *S, uint32_t v);
/* The views of the modules read since they were made (repl_open, repl_relink): their lines
   ordered, the views again, the active view kept; true: there were. The line map made. */
bool view_modules(Session *S);
/* The SUBs box's list (view_subs's), filled; null: no memory. */
const DlogList *view_list(Session *S);
/* View v's procedure: its name; 0 the module's. Uses comp_cbuf. */
uint32_t view_proc(Session *S, uint32_t v);
/* vw_next, vw_prev: Shift+F2, Ctrl+F2: the next view, the one before, in the active window. */
void view_next(Session *S);
void view_prev(Session *S);
/* vw_commit: the line just stored: a SUB or FUNCTION line typed becomes a procedure of its own,
   shown, as QBasic does; a procedure's own first line changed: the names again; an IMPORT or FROM:
   the modules it names loaded (one not found errors at the run). */
void view_commit(Session *S);
/* vw_newsub, vw_newfn: Edit, New SUB, New FUNCTION: its name asked, then its view (one of that
   name's in the active view's file, else a new procedure at the file's end). */
void view_newsub(Session *S);
void view_newfn(Session *S);
/* vw_subs: View, SUBs, F2: each file and its procedures listed; Edit in Active, Edit in Split,
   Delete. */
void view_subs(Session *S);
#endif
