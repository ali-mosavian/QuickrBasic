/* The debugger's UI (src/ui/dbg.asm). */
#include "ui/debugui.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/lexer.h"
#include "plat/pclk.h"
#include "plat/pscr.h"
#include "ui/dialog.h"
#include "ui/debugpane.h"
#include "ui/edit.h"
#include "ui/linemap.h"
#include "ui/screen.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "ui/window.h"

/* The lit statement: text line line, columns c0 to c1; until's text line + 1, 0 none. */
static struct {
    bool on;
    uint32_t line;
    uint16_t c0, c1;
    uint32_t until;
} hl;

/* The statement's end, before c1, past c0: its trailing spaces and ':' left out. */
static uint32_t trim_end(const uint8_t *const s, const uint32_t c0, uint32_t c1)
{
    while (c1 > c0 && (s[c1 - 1] == ' ' || s[c1 - 1] == ':'))
        c1--;

    return c1;
}

/* ud_hlset: the statement at pos lit, the cursor at its start. */
static void hlset(Session *const S, const DebugPos *const pos)
{
    uint32_t tl;

    if (!lmap_of(S, pos->line, &tl))
        return;

    hl.line = tl;
    const uint32_t len = text_get(S, view_goline(S, tl), uimn.line);
    uimn.line[len] = 0;
    uint8_t *const body = edit_body(uimn.line);
    lexr_line(S, body, S->comp.cbuf);
    const uint32_t bc = (uint32_t)(body - uimn.line);
    const uint32_t c0 = S->lexr.col[pos->from] + bc;
    const uint32_t c1 = trim_end(uimn.line, c0, pos->to == BOFF ? len : S->lexr.col[pos->to] + bc);
    hl.c0 = (uint16_t)c0;
    hl.c1 = (uint16_t)c1;
    hl.on = true;
    uimn.focus = FOC_EDIT;
    uimn.ed.sel_on = false;
    edit_load(S, hl.line - uimn.text.first);
    uimn.edline.col = hl.c0;
    edit_ensure();
}

enum { TRACE_TICKS = 2 };

/* ud_trace (DebugHooks.trace): the statement lit on the editor's page for two ticks. */
static void trace(Session *const S, const DebugPos *const at)
{
    hlset(S, at);
    wind_draw(S);
    scrn_flip();
    pscr_page(0);
    const uint32_t until = pclk_ticks() + TRACE_TICKS;

    while (until > pclk_ticks())
        ;

    pscr_page(1);
    hl.on = false;
}

void udbg_init(Session *const S)
{
    static const DebugHooks hooks = {.trace = trace};

    session_debug(S, &hooks);
}

void udbg_prep(Session *const S)
{
    dpan_close();
    hl.on = false;
    uimn.stopped = false;
    session_arm(S);

    if (hl.until)
        session_until(S, lmap_off(S, hl.until - 1));
}

void udbg_stop(Session *const S)
{
    hl.until = 0;
    DebugPos pos;

    if (!session_stopped_at(S, &pos))
        return;

    uimn.stopped = true;
    dpan_open(S);
    hlset(S, &pos);
}

void udbg_row(Session *const S, const uint8_t row, const uint8_t col, const uint32_t line)
{
    if (text_mark(S, line))
        scrn_attr(row, col, uimn.ed.width, SL_BPL);
    if (!hl.on || uimn.text.changed || line + uimn.text.first != hl.line)
        return;
    if (line == uimn.ed.line && uimn.edline.dirty) /* not while it is being changed */
        return;

    const uint16_t left = uimn.ed.left;
    const uint16_t a = hl.c0 > left ? hl.c0 - left : 0;

    if (hl.c1 <= left)
        return;

    const uint16_t end = hl.c1 - left;
    const uint16_t b = end > uimn.ed.width ? uimn.ed.width : end;

    if (a >= b)
        return;

    scrn_attr(row, (uint8_t)(col + a), b - a, SL_CUR);
}

[[noreturn]]
static void step(Session *const S, const StepKind kind)
{
    session_step(S, kind, uimn_cancont(S));
    uimn_cont(S);
}

void udbg_step(Session *const S) { step(S, STEP_INTO); }

void udbg_pstep(Session *const S) { step(S, STEP_OVER); }

void udbg_sout(Session *const S) { step(S, STEP_OUT); }

void udbg_restart(Session *const S)
{
    session_step(S, STEP_INTO, false);
    uimn_run(S);
}

void udbg_tron(Session *const S) { session_trace(S, !session_tracing(S)); }

void udbg_bp(Session *const S)
{
    if (uimn.focus == FOC_EDIT)
        text_toggle(S, uimn.ed.line);
}

void udbg_bpclear(Session *const S) { text_unmark_all(S); }

void udbg_until(Session *const S)
{
    if (uimn.focus != FOC_EDIT)
        return;

    hl.until = uimn.ed.line + uimn.text.first + 1;
    uimn_cont(S);
}

void udbg_setnext(Session *const S)
{
    if (!uimn_cancont(S)) {
        dlog_ok(S, "Cannot continue");
        return;
    }

    if (!session_set_next(S, text_rec(S, uimn.ed.line))) {
        dlog_ok(S, "Cannot cross module/procedure boundary");
        return;
    }

    udbg_stop(S);
}
