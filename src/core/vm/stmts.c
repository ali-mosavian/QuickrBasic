/* The statements' CALL routines: PRINT, INPUT, READ and DATA, DIM, RUN, LIST, CONT, END, STOP,
 * POKE, WAIT, INP, OUT, SYS and ^.
 *
 * # How it works
 * PRINT is one routine per item kind (stmt_ipr, stmt_fpr, stmt_spr) plus the separators. INPUT
 * reads a line (stmt_inbeg, stmt_infile), each field then takes its next item (stmt_fin, stmt_sin,
 * stmt_din); a bad field redoes the line from its start after the stores (StmtState.redo).
 *
 * READ finds the next DATA statement in the program store (next_data), copies its text to a buffer
 * in the arena and parses an item from it. RESTORE starts again at the first DATA. Each module
 * reads its own file's DATA: its place kept in its ModuMod while another's is in hand.
 *
 * RUN clears both stacks, events, COMMON and ON ERROR, then goes on at its line (stmt_rungo). END
 * and STOP leave CONT a place to go on from, then READY or BREAK.
 *
 * # Example
 *   10 DATA 1, 2
 *   20 READ a, b          a is 1, b is 2
 *
 * # asm structure (retired/asm32/vm/calls.asm)
 * | asm | here | |
 * |---|---|---|
 * | ex_runfirst | stmt_runfirst | MIRROR |
 * | ex_rungo | stmt_rungo | MIRROR |
 * | ex_ready | ready | MIRROR |
 * | rd_first | stmt_restore | MIRROR |
 * | sc_cont | stmt_cont | MIRROR |
 * | sc_end | stmt_end | MIRROR |
 * | sc_stop | stmt_stop | MIRROR |
 * | g_relbit | compared (operands.c) | MOVED |
 * | sc_ipr, sc_fpr, sc_spr, sc_pnl, sc_pzone, sc_ptab | stmt_ipr, stmt_fpr, stmt_spr, stmt_pnl, stmt_pzone, stmt_ptab | MIRROR |
 * | sc_pspc, sc_fin, sc_sin, sc_din, sc_fget, sc_sget | stmt_pspc, stmt_fin, stmt_sin, stmt_din, stmt_fget, stmt_sget | MIRROR |
 * | sc_inbeg, sc_inpr, sc_infile, sc_inend, sc_inln, sc_fread | stmt_inbeg, stmt_inpr, stmt_infile, stmt_inend, stmt_inln, stmt_fread | MIRROR |
 * | sc_sread, sc_dread, sc_res, sc_resl, sc_dim, sc_erase | stmt_sread, stmt_dread, stmt_res, stmt_resl (op RESTORE_L), stmt_dim, stmt_erase | MIRROR |
 * | sc_run, sc_runl, sc_list, sc_clr, sc_new, sc_nodir | stmt_run, stmt_runl (op RUN_L), stmt_list, stmt_clr, stmt_new, stmt_nodir | MIRROR |
 * | sc_poke, sc_wait, sc_wait2, sc_inp, sc_out, sc_sys | stmt_poke, stmt_wait, stmt_wait2, stmt_inp, stmt_out, stmt_sys | MIRROR |
 * | sc_ipow, sc_fpow, sc_fti, sc_sti, sc_ist | stmt_ipow, stmt_fpow, stmt_fti, stmt_sti, stmt_ist | MIRROR |
 * | io_redo, io_item, rd_item, rd_load, rd_scan, rd_after, rd_sk, rd_seek | redo, read_item, push_item, load_data, scan_data, data_after, skip_stmt, seek_data | MIRROR |
 * | g_dimst, g_dmsk | static | MIRROR |
 * | g_rdbuf, dlp, dataat, dyo, inend, iny0, iredo | StmtState (stmts.h) | D1 |
 *
 * - D1. READ's and INPUT's state is a StmtState in the session, not statics.
 */
#include "vm/stmts.h"
#include <stddef.h>
#include "session.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/link.h"
#include "comp/listing.h"
#include "io/channel.h"
#include "io/console.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/array.h"
#include "mem/copy.h"
#include "mem/program.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "qbrt/qbrt.h"
#include "num/double.h"
#include "num/fpmath.h"
#include "plat/pclk.h"
#include "plat/psys.h"
#include "vm/chain.h"
#include "vm/debug.h"
#include "vm/event.h"
#include "vm/flow.h"
#include "vm/using.h"

enum { DATA_TEXT_SIZE = 256, PRINT_ZONE = 14 };

/* What an INPUT field or a DATA item is read as. */
typedef enum : uint8_t { ITEM_SNGL, ITEM_STRING, ITEM_DOUBLE } ItemKind;

void stmt_init(Session *const S) { S->stmt.data_text = at(allo_carve(&S->allo, DATA_TEXT_SIZE)); }

/* ---- PRINT ---------------------------------------------------------------------------------- */

void stmt_ipr(Session *const S) { cons_int(&S->cons, exec_popi(&S->vm)); }
void stmt_fpr(Session *const S) { sngl_print(&S->cons, exec_pop(&S->vm).u); }
void stmt_spr(Session *const S) { cons_putdesc(&S->cons, strg_get(&S->strg, exec_popa(&S->vm))); }
void stmt_pnl(Session *const S) { cons_nl(&S->cons); }

/* ---- PRINT USING: the VM's operands to the formatter (vm/using.c) ---------------------------- */

static void stmt_usng_put(UsngOut *const o, const uint8_t *const p, const uint32_t n)
{
    Session *const S = container_of(o, Session, usng_out);

    for (uint32_t i = 0; i < n; i++)
        cons_putc(&S->cons, p[i]);
}

static void stmt_usng_nl(UsngOut *const o) { cons_nl(&container_of(o, Session, usng_out)->cons); }

void stmt_usng_init(Session *const S)
{
    S->usng_out = (UsngOut){.put = stmt_usng_put, .nl = stmt_usng_nl};

    usng_init(&S->usng, &S->fault, &S->usng_out);
}

void stmt_usb(Session *const S)
{
    const Desc s = strg_get(&S->strg, exec_popa(&S->vm));
    usng_begin(&S->usng, at(s.ptr), s.len);
}

void stmt_uss(Session *const S)
{
    const Desc s = strg_get(&S->strg, exec_popa(&S->vm));
    usng_string(&S->usng, at(s.ptr), s.len);
}

void stmt_usi(Session *const S)
{
    usng_int(&S->usng, S->vm.sp->i);
    S->vm.sp++;
}

void stmt_usf(Session *const S)
{
    usng_real(&S->usng, S->vm.sp->f, 7);
    S->vm.sp++;
}

void stmt_usd(Session *const S)
{
    usng_real(&S->usng, S->vm.sp->d, 16);
    S->vm.sp++;
}

void stmt_use(Session *const S) { usng_end(&S->usng, true); }
void stmt_usen(Session *const S) { usng_end(&S->usng, false); }

/* PR.ZONE: to the next zone. */
void stmt_pzone(Session *const S)
{
    do
        cons_space(&S->cons);
    while (S->cons.col % PRINT_ZONE);
}

/* TAB(i): to column i, from 1; past it, on the next line. */
void stmt_ptab(Session *const S)
{
    const int32_t i = exec_popi(&S->vm) - 1;
    const uint8_t col = i > 0 ? (uint8_t)i : 0;

    if (col < S->cons.col)
        cons_nl(&S->cons);
    if (col > S->cons.col)
        cons_spaces(&S->cons, col - S->cons.col);
}

void stmt_pspc(Session *const S) { cons_spaces(&S->cons, (uint8_t)exec_popi(&S->vm)); }

/* ---- INPUT ---------------------------------------------------------------------------------- */

/* IN.BEGIN end:2: not direct; REDO comes back here. The end is where an empty line goes on. */
void stmt_inbeg(Session *const S)
{
    flow_nodir(S);
    S->stmt.redo = false;
    S->stmt.input_end = at(S->allo.cache + exec_u32(&S->vm));
    S->stmt.input_start = S->vm.ip;
}

/* ? and a line. */
void stmt_inpr(Session *const S) { fault_check(&S->fault, cons_prompt(&S->cons)); }

/* INPUT#'s line, empty lines skipped. */
void stmt_infile(Session *const S)
{
    S->cons.from_file = true;

    do
        fault_check(&S->fault, cons_read(&S->cons));
    while (!S->cons.inlen);
}

/* A line, no ?: INPUT "prompt", .. */
void stmt_inln(Session *const S) { fault_check(&S->fault, cons_read(&S->cons)); }

/* After a bad field's store: ?Redo from start, the INPUT again. true: vm->ip moved. */
static bool redo(Session *const S)
{
    if (!S->stmt.redo)
        return false;

    S->stmt.redo = false;
    cons_msg(&S->cons, MSG_REDO);
    S->vm.ip = S->stmt.input_start;
    S->vm.sp = flow_stack_base(&S->vm);
    return true;
}

/* An item at *p, ending at the statement's end or ','; *p at that end. false: it ends elsewhere
   (bad), the value read as far as it went. */
static bool read_item(Session *const S, const uint8_t **const p, const ItemKind kind, Cell *const v)
{
    switch (kind) {
    case ITEM_STRING: {
        Addr pos = addr_of(*p);
        v->a = strg_item(&S->strg, &pos);
        *p = at(pos);
        break;
    }
    case ITEM_DOUBLE:
        v->d = dubl_in(&S->fault, p, nullptr);
        break;

    case ITEM_SNGL:
        v->u = sngl_in(&S->fault, p);
        break;
    }

    return comp_eos(&S->comp, p) || **p == ',';
}

static void push_item(Vm *const vm, const ItemKind kind, const Cell v)
{
    if (kind == ITEM_DOUBLE)
        exec_push(vm, v);
    else
        exec_pusha(vm, v.u);
}

/* The next field. An empty line leaves the rest of the INPUT. File data: a bad number in INPUT#. */
static void input(Session *const S, const ItemKind kind)
{
    if (redo(S))
        return;
    if (!S->cons.inlen) {
        S->vm.sp = flow_stack_base(&S->vm);
        S->vm.ip = S->stmt.input_end;
        return;
    }

    fault_check(&S->fault, cons_field(&S->cons));
    const uint8_t *p = S->cons.inbuf + S->cons.inpos;
    Cell v = {};

    const bool ok = read_item(S, &p, kind, &v);
    S->cons.inpos = (int32_t)(p - S->cons.inbuf);

    if (!ok) {
        if (S->cons.from_file)
            repl_error(S, E_FD);

        S->stmt.redo = true; /* stored as far as it went, then REDO */
    }

    push_item(&S->vm, kind, v);
}
void stmt_fin(Session *const S) { input(S, ITEM_SNGL); }
void stmt_sin(Session *const S) { input(S, ITEM_STRING); }
void stmt_din(Session *const S) { input(S, ITEM_DOUBLE); }

/* IN.END: ?Extra ignored for fields left. */
void stmt_inend(Session *const S)
{
    if (redo(S) || S->cons.from_file || S->cons.inpos == -1)
        return;
    if (S->cons.inbuf[S->cons.inpos])
        cons_msg(&S->cons, MSG_EXTRA);
}

/* GET to a number: the key waiting as a digit, 0 none. Syntax error: not a digit. */
void stmt_fget(Session *const S)
{
    const uint8_t c = cons_getc(&S->cons);
    const uint8_t digit = c ? c - '0' : 0;

    if (digit >= 10)
        repl_error(S, E_SN);

    exec_pusha(&S->vm, fmth_from_i16(digit));
}

/* GET to a string: the key waiting, or none. */
void stmt_sget(Session *const S)
{
    const uint8_t c = cons_getc(&S->cons);

    if (!c) {
        exec_pusha(&S->vm, strg_push(&S->strg, (Desc){}));

        return;
    }

    S->cons.sbuf[0] = c;
    exec_pusha(&S->vm, strg_text(&S->strg, S->cons.sbuf, 1));
}

/* ---- READ and DATA -------------------------------------------------------------------------- */

/* Past a statement: *b past its ':' or its 0, which it returns. */
static uint8_t skip_stmt(Prog *const pg, uint32_t *const b)
{
    const uint8_t *const start = at(prog_addr(pg, *b));
    const uint8_t *p = start;

    while (*p && *p != ':')
        p = decl_item(p);

    *b += (uint32_t)(p - start) + 1;
    return *p;
}

/* The next line's body: *line on, *b its first byte; false at the end, the program's or its
   module's (LF_TOP). By the record's length: the line's pre bytes (the editor's indent) follow its
   0. */
static bool next_line(Prog *const pg, uint32_t *const line, uint32_t *const b)
{
    *line += prog_line(pg, *line)->len;
    *b = *line + HDR;
    return prog_line(pg, *line)->len && !(prog_line(pg, *line)->flags & LF_TOP);
}

/* The first DATA at or after the statement starting at *b, in *line; *b its first byte. */
static bool scan_data(Prog *const pg, uint32_t *const line, uint32_t *const b)
{
    for (;;) {
        uint8_t c;

        while ((c = rd8(prog_addr(pg, *b))) == ' ')
            ++*b;
        if (c == TK_DATA) {
            ++*b;
            return true;
        }

        if (c && skip_stmt(pg, b))
            continue;
        if (!next_line(pg, line, b))
            return false;
    }
}

/* The first DATA after the statement *b is in, in *line. */
static bool data_after(Prog *const pg, uint32_t *const line, uint32_t *const b)
{
    if (!skip_stmt(pg, b) && !next_line(pg, line, b))
        return false;

    return scan_data(pg, line, b);
}

/* The text of the DATA at b, in line, to data_text, up to its ':'. */
static void load_data(Session *const S, const uint32_t line, const uint32_t b)
{
    S->stmt.data_line = line;
    S->stmt.data_at = b;
    const uint8_t *const text = at(prog_addr(&S->prog, b));
    const size_t n = (size_t)(comp_skipdata(text) - text);
    memcpy(S->stmt.data_text, text, n);
    S->stmt.data_text[n] = 0;
}

/* The running line's module, as READ and RESTORE take it: its DATA's place in hand, the one
   before kept in its module's ModuMod. */
static void data_module(Session *const S)
{
    StmtState *const st = &S->stmt;
    const uint32_t m = modu_of(&S->modu, prog_line(&S->prog, S->vm.line)->file);

    if (m == st->data_module)
        return;

    modu_mod(&S->modu, st->data_module)->read = (ModuRead){
        .set = st->data_pos,
        .before_first = st->before_first,
        .line = st->data_line,
        .at = st->data_at,
        .pos = st->data_pos ? (uint32_t)(st->data_pos - st->data_text) : 0,
    };

    const ModuRead r = modu_mod(&S->modu, m)->read;
    st->data_module = m;
    st->data_pos = nullptr;

    if (!r.set)
        return;
    if (r.at)
        load_data(S, r.line, r.at);

    st->data_at = r.at;
    st->data_line = r.line;
    st->data_text[0] = r.at ? st->data_text[0] : 0;
    st->data_pos = st->data_text + r.pos;
    st->before_first = r.before_first;
}

/* The running module's first line: past its LF_TOP, the main's PROGBASE. */
static uint32_t data_first(Session *const S)
{
    const uint32_t m = S->stmt.data_module, top = modu_top_of(&S->modu, m);
    return m ? top + prog_line(&S->prog, top)->len : PROGBASE;
}

/* READ goes on at the first DATA at or after line. */
static void seek_data(Session *const S, const uint32_t line)
{
    uint32_t at_line = line, b = line + HDR;
    const Line *const l = prog_line(&S->prog, line); /* LF_TOP: a module with no lines */

    if (l->len && !(l->flags & LF_TOP) && scan_data(&S->prog, &at_line, &b))
        load_data(S, at_line, b);
    else {
        S->stmt.data_at = 0;
        S->stmt.data_text[0] = 0;
    }

    S->stmt.data_pos = S->stmt.data_text;
    S->stmt.before_first = true;
}

/* The next DATA item. Out of DATA. */
static const uint8_t *next_data(Session *const S)
{
    data_module(S);

    if (!S->stmt.data_pos) {
        seek_data(S, data_first(S));

        if (!S->stmt.data_at)
            repl_error(S, E_OD);
    }

    const uint8_t *p = S->stmt.data_pos;

    if (S->stmt.before_first) {
        S->stmt.before_first = false;
        return p;
    }

    if (comp_peek(&p) == ',')
        return p + 1;

    uint32_t line = S->stmt.data_line, b = S->stmt.data_at;

    if (!b || !data_after(&S->prog, &line, &b))
        repl_error(S, E_OD);

    load_data(S, line, b);
    return S->stmt.data_text;
}

/* The next DATA item. Syntax error, in the DATA's line: a bad item. */
static void read(Session *const S, const ItemKind kind)
{
    const uint8_t *p = next_data(S);
    Cell v = {};

    if (!read_item(S, &p, kind, &v)) {
        S->vm.line = S->stmt.data_line;
        repl_error(S, E_SN);
    }

    S->stmt.data_pos = p;
    push_item(&S->vm, kind, v);
}
void stmt_fread(Session *const S) { read(S, ITEM_SNGL); }
void stmt_sread(Session *const S) { read(S, ITEM_STRING); }
void stmt_dread(Session *const S) { read(S, ITEM_DOUBLE); }

/* RESTORE: the running module's first DATA. */
void stmt_res(Session *const S)
{
    data_module(S);
    S->stmt.data_pos = nullptr;
}

void stmt_restore(Session *const S)
{
    S->stmt.data_pos = nullptr;
    S->stmt.data_module = 0;

    for (uint32_t m = 0; m < modu_count(&S->modu); m++)
        modu_mod(&S->modu, m)->read = (ModuRead){};
}

/* RESTORE line: its line set at link, in the running module. */
void stmt_resl(Session *const S)
{
    data_module(S);
    seek_data(S, flow_target(&S->vm));
}

/* ---- DIM and ERASE -------------------------------------------------------------------------- */

/* DIM ( i1..in mlo mhi id flags -- ): bit i of m: subscript i was written hi, base. As arry_dim,
   REDIM as arry_redim. A record's array field's, ours: ( i1..in mlo mhi cell et flags -- ). */
void stmt_dim(Session *const S)
{
    const uint32_t flags = exec_pop(&S->vm).u;
    const uint8_t dims = flags & DIM_COUNT;
    const uint32_t target = exec_pop(&S->vm).u; /* its id; a field's elements' ET_* */
    const Addr field = flags & DIM_FIELD ? exec_popa(&S->vm) : 0;
    const uint32_t id = target;
    const uint32_t mask_hi = exec_pop(&S->vm).u;
    const uint64_t swapped = (uint64_t)mask_hi << 32 | exec_pop(&S->vm).u;

    for (uint8_t i = dims; i-- > 0;) { /* lo TO hi, or hi and the base */
        const int32_t second = exec_popi(&S->vm);
        const int32_t first = exec_popi(&S->vm);
        const bool swap = swapped >> i & 1;
        const int32_t hi = swap ? first : second;
        const int32_t lo = swap ? second : first;
        const int64_t span = (int64_t)hi - lo; /* past INT32_MAX: Out of memory, by its size */
        S->arry.subs[i] = span > INT32_MAX ? INT32_MAX : (int32_t)span;
        S->arry.lbounds[i] = lo;
    }

    const bool keep = flags & DIM_PRESERVE;

    if (field) {
        arry_redim(&S->arry, (ArrSlot *)at(field), target, dims, keep);
        return;
    }

    ArrSlot *const s = arry_slot(&S->arry, id); /* in the symbols or the frame: it stays */

    if (flags & DIM_REDIM)
        arry_redim(&S->arry, s, arry_var_et(&S->symb, id), dims, keep);
    else if (!(flags & DIM_STATIC) || !s->hdr) /* a STATIC's, dimensioned: kept */
        arry_dim(&S->arry, id, dims);
}

/* DIM a() ( id flags -- ), ours: as arry_empty. */
void stmt_aempty(Session *const S)
{
    const uint32_t flags = exec_pop(&S->vm).u;
    arry_empty(&S->arry, exec_pop(&S->vm).u, flags & DIM_STATIC);
}

/* a(:) = b(:) ( src dst -- ), ours, their cells: as arry_copy. */
void stmt_acopy(Session *const S)
{
    ArrSlot *const d = (ArrSlot *)at(exec_popa(&S->vm));
    arry_copy(&S->arry, &S->vm, d, (const ArrSlot *)at(exec_popa(&S->vm)));
}

/* a() = b() or f(..) ( src dst -- ), ours, their cells: as arry_move. */
void stmt_amove(Session *const S)
{
    ArrSlot *const d = (ArrSlot *)at(exec_popa(&S->vm));
    arry_move(&S->arry, &S->vm, d, (ArrSlot *)at(exec_popa(&S->vm)));
}

/* A fixed array field's element ( addr s lb count stride -- addr ), ours: s's place in its
   dimension, stride bytes each. Subscript out of range: past it. */
/* An array result's start ( id -- ), ours: as arry_reset. */
void stmt_areset(Session *const S)
{
    arry_reset(&S->arry, &S->vm, arry_slot(&S->arry, exec_pop(&S->vm).u));
}

/* ERASE ( array -- ), an operand (exec_arrayop): REDIM's array freed, another's elements zeroed. */
void stmt_erase(Session *const S)
{
    ArrView v;
    ArrSlot *const s = exec_arrayop(&S->arry, &S->vm, &v);

    if (s && !s->hdr)
        return;
    if (s && s->dyn) {
        arry_free(&S->arry, &S->vm, s);
        return;
    }

    arry_clear(&S->arry, &S->vm, &v);
}

/* FCELL ( place key -- cell ): a fixed array field's cell for an array parameter (arry_field_cell). */
void stmt_fcell(Session *const S)
{
    const Addr key = exec_popa(&S->vm) & ~ARR_FIELD;
    const Addr data = exec_popa(&S->vm);
    exec_pusha(&S->vm, addr_of(arry_field_cell(&S->arry, data, key)));
}

/* ---- RUN, LIST, CONT, END, STOP ------------------------------------------------------------- */

/* Back to READY, the frames kept. */
[[noreturn]]
static void ready(Session *const S)
{
    evnt_clear(S);
    S->vm.stop &= ~SF_EV;
    repl_ready(S);
}

void stmt_rungo(Session *const S, const uint32_t line)
{
    flow_drop_calls(S);
    evnt_clear(S);
    chai_start(&S->chai, &S->symb);
    modu_run_reset(&S->modu);
    natv_start(&S->natv);
    S->vm.err.active = false;
    S->vm.err.code = 0;
    S->vm.err.line = 0;
    S->strg.tmp_floor = 0;
    flow_jump(S, line);
}

/* RUN line. Undefined line number; a label. */
void stmt_runl(Session *const S)
{
    const uint16_t num = ((const LineRef *)S->vm.ip)->num;
    const uint32_t line = flow_target(&S->vm);

    if (num == NONUM)
        repl_error(S, E_US); /* RUN takes a number, not a label */
    session_clear(S);
    music_reset();
    link_start(&S->comp);
    stmt_rungo(S, line);
}

void stmt_run(Session *const S)
{
    session_clear(S);
    music_reset();
    link_start(&S->comp);
    stmt_runfirst(S);
}

void stmt_runfirst(Session *const S)
{
    fault_check(&S->fault, chnl_closeall(&S->chnl));

    if (!prog_line(&S->prog, PROGBASE)->len)
        flow_end(S);

    stmt_rungo(S, PROGBASE);
}

/* LIST ( from to -- ), then READY. */
void stmt_list(Session *const S)
{
    const uint16_t to = (uint16_t)exec_pop(&S->vm).u;
    const uint16_t from = (uint16_t)exec_pop(&S->vm).u;
    list_range(&S->comp, from, to);
    ready(S);
}

void stmt_cont(Session *const S)
{
    if (!S->vm.cont.line)
        repl_error(S, E_CN);
    if (!S->vm.cont.code)
        ready(S);

    S->vm.line = S->vm.cont.line;
    S->vm.ip = flow_place(&S->allo, S->vm.cont);
}

/* CLEAR: the variables, and the frames. */
void stmt_clr(Session *const S)
{
    flow_drop_calls(S);
    session_clear(S);
}

void stmt_new(Session *const S)
{
    session_new(S);
    flow_end(S);
}

/* Illegal direct in the direct line. */
void stmt_nodir(Session *const S) { flow_nodir(S); }

void stmt_end(Session *const S)
{
    if (S->dbug.probing)
        dbug_done(S);

    fault_check(&S->fault, chnl_closeall(&S->chnl));
    natv_stop(&S->natv);
    flow_setcont(&S->vm, &S->allo);
    ready(S);
}

void stmt_stop(Session *const S)
{
    S->vm.stop = 0;
    flow_setcont(&S->vm, &S->allo);
    repl_error(S, E_BRK);
}

/* ---- the machine ---------------------------------------------------------------------------- */

/* POKE ( addr byte -- ). */
void stmt_poke(Session *const S)
{
    const uint8_t b = (uint8_t)exec_pop(&S->vm).u;
    wr8(exec_popa(&S->vm), b);
}

/* WAIT ( port mask -- ): until (INP XOR flip) AND mask. */
static void wait(Vm *const vm, const uint8_t flip)
{
    const uint8_t mask = (uint8_t)exec_pop(vm).u;
    const uint16_t port = (uint16_t)exec_pop(vm).u;

    while (!((psys_inp(port) ^ flip) & mask))
        ;
}
void stmt_wait(Session *const S) { wait(&S->vm, (uint8_t)exec_pop(&S->vm).u); }
void stmt_wait2(Session *const S) { wait(&S->vm, 0); }

/* INP ( port -- b ); OUT ( port b -- ). */
void stmt_inp(Session *const S) { exec_pushi(&S->vm, psys_inp((uint16_t)exec_pop(&S->vm).u)); }
void stmt_out(Session *const S)
{
    const uint8_t b = (uint8_t)exec_pop(&S->vm).u;
    psys_outp((uint16_t)exec_pop(&S->vm).u, b);
}

/* SYS ( addr -- ): a call into the POKE memory; nothing poked there: a no-op. */
void stmt_sys(Session *const S)
{
    if (!S->allo.poke)
        S->allo.poke = allo_carve(&S->allo, POKESZ);

    const Addr a = S->allo.poke + (uint16_t)exec_pop(&S->vm).u;

    if (rd8(a))
        psys_call(a);
}

/* ---- ^ and TI ------------------------------------------------------------------------------- */

/* INT ^ INT: 0 for a negative power past |base| 1. Overflow past an INT. */
static int32_t int_pow(Fault *const f, const int32_t base, const int32_t power)
{
    if (!power || base == 1)
        return 1;
    if (!base)
        return 0;
    if (base == -1)
        return power & 1 ? -1 : 1;
    if (power < 0)
        return 0;

    int32_t r = 1;

    for (int32_t i = 0; i < power; i++)
        if (__builtin_mul_overflow(r, base, &r))
            fault_error(f, E_OV);

    return r;
}

void stmt_ipow(Session *const S)
{
    const int32_t power = exec_popi(&S->vm);
    const int32_t base = exec_popi(&S->vm);
    exec_pushi(&S->vm, int_pow(&S->fault, base, power));
}

void stmt_fpow(Session *const S)
{
    const Sngl power = exec_pop(&S->vm).u;
    const Sngl base = exec_pop(&S->vm).u;
    exec_pusha(&S->vm, fmth_pow(&S->fault, base, power));
}

/* TI: the jiffies. */
void stmt_fti(Session *const S)
{
    const uint32_t j = pclk_jiffies();
    exec_pusha(&S->vm, j ? sngl_norm(j, false) : 0);
}

/* TI$: "HHMMSS" from the hundredths. */
void stmt_sti(Session *const S)
{
    uint32_t t = pclk_jiffies() / 100;

    for (int i = 4; i >= 0; i -= 2) {
        const uint32_t d = t % 60;
        t /= 60;
        S->cons.sbuf[i] = '0' + d / 10;
        S->cons.sbuf[i + 1] = '0' + d % 10;
    }

    exec_pusha(&S->vm, strg_text(&S->strg, S->cons.sbuf, 6));
}

/* ST: the I/O status. */
void stmt_ist(Session *const S) { exec_pushi(&S->vm, S->cons.status); }
