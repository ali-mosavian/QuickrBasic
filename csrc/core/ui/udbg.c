/* The debugger's UI (src/ui/dbg.asm). */
#include "ui/udbg.h"
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

static void hlset(void)
{
    uint32_t tl;
    if (!lmap_of(dbug.wline, &tl))
        return;
    hl.line = tl;
    const uint32_t len = text_get(view_goline(tl), uimn.line);
    uimn.line[len] = 0;
    uint8_t *const body = edit_body(uimn.line);
    lexr_line(body, comp_cbuf);
    const uint32_t bc = (uint32_t)(body - uimn.line);
    const uint32_t c0 = lexr.col[dbug.stmt_from] + bc;
    const uint32_t c1 = trim_end(uimn.line, c0,
                                 dbug.stmt_to == BOFF ? len : lexr.col[dbug.stmt_to] + bc);
    hl.c0 = (uint16_t)c0;
    hl.c1 = (uint16_t)c1;
    hl.on = true;
    uimn.focus = FOC_EDIT;
    uimn.ed.sel_on = false;
    edit_load(hl.line - uimn.text.first);
    uimn.edline.col = hl.c0;
    edit_ensure();
}

enum { TRACE_TICKS = 2 };

/* ud_trace (dbug.hook): the statement lit on the editor's page for two ticks. */
static void trace(void)
{
    hlset();
    wind_draw();
    scrn_flip();
    pscr_page(0);
    const uint32_t until = pclk_ticks() + TRACE_TICKS;
    while (until > pclk_ticks())
        ;
    pscr_page(1);
    hl.on = false;
}

void udbg_init(void)
{
    dbug.emit = true;
    dbug.hook = trace;
}

void udbg_prep(void)
{
    dpan_close();
    hl.on = false;
    uimn.stopped = false;
    dbug.bpon = true;
    dbug.until = false;
    if (hl.until) {
        dbug.uline = lmap_off(hl.until - 1);
        dbug.until = true;
    }
}

void udbg_stop(void)
{
    hl.until = 0;
    if (!vm.cont.line)
        return;
    uimn.stopped = true;
    dpan_open();
    dbug_pos(vm.cont.line, at(allo.cache + vm.cont.code));
    hlset();
}

void udbg_row(const uint8_t row, const uint8_t col, const uint32_t line)
{
    if (text_mark(line))
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

void udbg_step(void)
{
    dbug.mode = DM_STEP;
    uimn_cont();
}

void udbg_pstep(void)
{
    dbug.pdep = uimn_cancont() ? dbug_depth(&vm) : 0;
    dbug.mode = DM_PSTEP;
    uimn_cont();
}

void udbg_sout(void)
{
    const uint32_t d = uimn_cancont() ? dbug_depth(&vm) : 0;
    dbug.pdep = d ? d - 1 : 0;
    dbug.mode = DM_PSTEP;
    uimn_cont();
}

void udbg_restart(void)
{
    dbug.mode = DM_STEP;
    uimn_run();
}

void udbg_tron(void) { dbug.trace = !dbug.trace; }

void udbg_bp(void)
{
    if (uimn.focus == FOC_EDIT)
        text_toggle(uimn.ed.line);
}

void udbg_bpclear(void) { text_unmark_all(); }

void udbg_until(void)
{
    if (uimn.focus != FOC_EDIT)
        return;
    hl.until = uimn.ed.line + uimn.text.first + 1;
    uimn_cont();
}

void udbg_setnext(void)
{
    if (!uimn_cancont()) {
        dlog_ok("Cannot continue");
        return;
    }
    const uint32_t rec = text_rec(uimn.ed.line);
    blok_scope(rec);
    const uint8_t proc = comp.proc;
    blok_scope(vm.cont.line);
    if (proc != comp.proc) {
        dlog_ok("Cannot cross module/procedure boundary");
        return;
    }
    const uint32_t code = prog_line(rec)->code;
    const Addr a = code ? allo.cache + code : link_cache(rec);
    vm.cont.line = rec;
    vm.cont.code = a - allo.cache;
    udbg_stop();
}
