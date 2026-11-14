/* The debugger's UI (src/ui/dbg.asm). */
#include "ui/udbg.h"
#include "session.h"
#include "comp/blok.h"
#include "comp/comp.h"
#include "comp/lexr.h"
#include "comp/link.h"
#include "mem/allo.h"
#include "mem/prog.h"
#include "plat/pclk.h"
#include "plat/pscr.h"
#include "ui/dlog.h"
#include "ui/dpan.h"
#include "ui/edit.h"
#include "ui/lmap.h"
#include "ui/scrn.h"
#include "ui/text.h"
#include "ui/uimn.h"
#include "ui/view.h"
#include "ui/wind.h"
#include "vm/dbug.h"

/* The lit statement: text line line, columns c0 to c1; until's text line + 1, 0 none. */
static struct {
    bool on;
    uint32_t line;
    uint16_t c0, c1;
    uint32_t until;
} hl;

/* ud_hlset: the statement at dbug.wline, stmt_from, stmt_to lit, the cursor at its start. */
/* The statement's end, before c1, past c0: its trailing spaces and ':' left out. */
static uint32_t trim_end(const uint8_t *const s, const uint32_t c0, uint32_t c1)
{
    while (c1 > c0 && (s[c1 - 1] == ' ' || s[c1 - 1] == ':'))
        c1--;
    return c1;
}

static void hlset(Session *const S)
{
    uint32_t tl;
    if (!lmap_of(S, S->dbug.wline, &tl))
        return;
    hl.line = tl;
    const uint32_t len = text_get(S, view_goline(S, tl), uimn.line);
    uimn.line[len] = 0;
    uint8_t *const body = edit_body(uimn.line);
    lexr_line(S, body, S->comp.cbuf);
    const uint32_t bc = (uint32_t)(body - uimn.line);
    const uint32_t c0 = S->lexr.col[S->dbug.stmt_from] + bc;
    const uint32_t c1 = trim_end(uimn.line, c0,
                                 S->dbug.stmt_to == BOFF ? len : S->lexr.col[S->dbug.stmt_to] + bc);
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

/* ud_trace (dbug.hook): the statement lit on the editor's page for two ticks. */
static void trace(Session *const S)
{
    hlset(S);
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
    S->dbug.emit = true;
    S->dbug.hook = trace;
}

void udbg_prep(Session *const S)
{
    dpan_close();
    hl.on = false;
    uimn.stopped = false;
    S->dbug.bpon = true;
    S->dbug.until = false;
    if (hl.until) {
        S->dbug.uline = lmap_off(S, hl.until - 1);
        S->dbug.until = true;
    }
}

void udbg_stop(Session *const S)
{
    hl.until = 0;
    if (!S->vm.cont.line)
        return;
    uimn.stopped = true;
    dpan_open(S);
    dbug_pos(S, S->vm.cont.line, at(S->allo.cache + S->vm.cont.code));
    hlset(S);
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

void udbg_step(Session *const S)
{
    S->dbug.mode = DM_STEP;
    uimn_cont(S);
}

void udbg_pstep(Session *const S)
{
    S->dbug.pdep = uimn_cancont(S) ? dbug_depth(&S->vm) : 0;
    S->dbug.mode = DM_PSTEP;
    uimn_cont(S);
}

void udbg_sout(Session *const S)
{
    const uint32_t d = uimn_cancont(S) ? dbug_depth(&S->vm) : 0;
    S->dbug.pdep = d ? d - 1 : 0;
    S->dbug.mode = DM_PSTEP;
    uimn_cont(S);
}

void udbg_restart(Session *const S)
{
    S->dbug.mode = DM_STEP;
    uimn_run(S);
}

void udbg_tron(Session *const S) { S->dbug.trace = !S->dbug.trace; }

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
    const uint32_t rec = text_rec(S, uimn.ed.line);
    blok_scope(S, rec);
    const uint8_t proc = S->comp.proc;
    blok_scope(S, S->vm.cont.line);
    if (proc != S->comp.proc) {
        dlog_ok(S, "Cannot cross module/procedure boundary");
        return;
    }
    const uint32_t code = prog_line(&S->prog, rec)->code;
    const Addr a = code ? S->allo.cache + code : link_cache(S, rec);
    S->vm.cont.line = rec;
    S->vm.cont.code = a - S->allo.cache;
    udbg_stop(S);
}
