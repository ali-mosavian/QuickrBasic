/* The places jumped from: Go To, Go to Definition, F2's list, Find, F3 and Ctrl+F8 keep where
 * the cursor was; Alt+Left goes back there, Alt+Right forward again.
 *
 * # How it works
 * A place is a line of the whole text and a column. Back and forward are stacks of NPLACE; the
 * oldest goes when back is full. A line added or taken out moves the places after it
 * (uimn.text.edits, the hook set before, undo's, told first); a place on a line taken out stays
 * on the line that takes its place.
 */
#include "ui/jumps.h"
#include "plat/pscr.h"
#include "ui/edit.h"
#include "ui/text.h"
#include "ui/view.h"

enum { NPLACE = 32 };

typedef struct {
    uint32_t line;
    uint16_t col;
} Mark;

typedef struct {
    Mark at[NPLACE];
    uint32_t n;
} Places;

static struct {
    Places back, fwd;
    void (*was)(Session *S, uint32_t line, uint8_t e);
} jp;

static void moved(Places *const p, const uint32_t line, const int32_t delta)
{
    for (uint32_t i = 0; i < p->n; i++)
        if (p->at[i].line > line || (delta > 0 && p->at[i].line == line))
            p->at[i].line = (uint32_t)((int32_t)p->at[i].line + delta);
}

static void edited(Session *const S, const uint32_t line, const uint8_t e)
{
    if (jp.was)
        jp.was(S, line, e);

    if (e == TE_INSERTED || e == TE_DELETING) {
        moved(&jp.back, line, e == TE_INSERTED ? 1 : -1);
        moved(&jp.fwd, line, e == TE_INSERTED ? 1 : -1);
    }
}

void jumps_init(void)
{
    jp.was = uimn.text.edits;
    uimn.text.edits = edited;
    jumps_clear();
}

void jumps_clear(void) { jp.back.n = jp.fwd.n = 0; }

static Mark here(void) { return (Mark){uimn.text.first + uimn.ed.line, uimn.edline.col}; }

static void push(Places *const p, const Mark at)
{
    if (p->n && p->at[p->n - 1].line == at.line && p->at[p->n - 1].col == at.col)
        return;

    if (p->n == NPLACE) {
        memmove(p->at, p->at + 1, (NPLACE - 1) * sizeof p->at[0]);
        p->n--;
    }

    p->at[p->n++] = at;
}

void jumps_push(void)
{
    push(&jp.back, here());
    jp.fwd.n = 0;
}

/* The last place of from gone to, the cursor's kept on to. */
static void go(Session *const S, Places *const from, Places *const to)
{
    if (!from->n) {
        pscr_beep();
        return;
    }

    edit_commit(S);
    push(to, here());
    const Mark p = from->at[--from->n];
    const uint32_t line = p.line < uimn.text.all ? p.line : uimn.text.all - 1;
    edit_load(S, view_goline(S, line));
    uimn.edline.col = p.col;
    uimn.ed.sel_on = false;
    edit_ensure();
}

void jumps_back(Session *const S) { go(S, &jp.back, &jp.fwd); }

void jumps_forward(Session *const S) { go(S, &jp.fwd, &jp.back); }
