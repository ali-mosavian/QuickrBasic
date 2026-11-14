/* The statements' CALL routines (src/vm/calls.asm): PRINT, INPUT, READ and DATA, DIM, RUN, LIST,
   CONT, END, STOP, POKE, WAIT, INP, OUT, SYS, ^. */
#include "vm/stmts.h"
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
#include "num/double.h"
#include "num/fpmath.h"
#include "plat/pclk.h"
#include "plat/psys.h"
#include "vm/chain.h"
#include "vm/debug.h"
#include "vm/event.h"
#include "vm/flow.h"

enum { DATA_TEXT_SIZE = 256, PRINT_ZONE = 14 };

/* What an INPUT field or a DATA item is read as. */
typedef enum : uint8_t { ITEM_SNGL, ITEM_STRING, ITEM_DOUBLE } ItemKind;


void stmt_init(Session *const S) { S->stmt.data_text = at(allo_carve(S, DATA_TEXT_SIZE)); }

/* ---- PRINT ---------------------------------------------------------------------------------- */

void stmt_ipr(Session *const S) { cons_int(S, exec_popi(&S->vm)); }
void stmt_fpr(Session *const S) { sngl_print(S, exec_pop(&S->vm).u); }
void stmt_spr(Session *const S) { strg_print(S, strg_get(S, exec_popa(&S->vm))); }
void stmt_pnl(Session *const S) { cons_nl(S); }

/* PR.ZONE: to the next zone. */
void stmt_pzone(Session *const S)
{
    do
        cons_space(S);
    while (S->cons.col % PRINT_ZONE);
}

/* TAB(i): to column i, from 1; past it, on the next line. */
void stmt_ptab(Session *const S)
{
    const int32_t i = exec_popi(&S->vm) - 1;
    const uint8_t col = i > 0 ? (uint8_t)i : 0;
    if (col < S->cons.col)
        cons_nl(S);
    if (col > S->cons.col)
        cons_spaces(S, col - S->cons.col);
}

void stmt_pspc(Session *const S) { cons_spaces(S, (uint8_t)exec_popi(&S->vm)); }

/* ---- INPUT ---------------------------------------------------------------------------------- */

/* IN.BEGIN end:2: not direct; REDO comes back here. The end is where an empty line goes on. */
void stmt_inbeg(Session *const S)
{
    flow_nodir(S);
    S->stmt.redo = false;
    S->stmt.input_end = at(S->allo.cache + exec_u16(&S->vm));
    S->stmt.input_start = S->vm.ip;
}

/* ? and a line. */
void stmt_inpr(Session *const S) { cons_prompt(S); }

/* INPUT#'s line, empty lines skipped. */
void stmt_infile(Session *const S)
{
    S->cons.from_file = true;
    do
        cons_read(S);
    while (!S->cons.inlen);
}

/* A line, no ?: INPUT "prompt", .. */
void stmt_inln(Session *const S) { cons_read(S); }

/* After a bad field's store: ?Redo from start, the INPUT again. true: vm->ip moved. */
static bool redo(Session *const S)
{
    if (!S->stmt.redo)
        return false;
    S->stmt.redo = false;
    cons_msg(S, MSG_REDO);
    S->vm.ip = S->stmt.input_start;
    S->vm.sp = S->vm.rsp;
    return true;
}

/* An item at *p, ending at the statement's end or ','; *p at that end. false: it ends elsewhere
   (bad), the value read as far as it went. */
static bool read_item(Session *const S, const uint8_t **const p, const ItemKind kind, Cell *const v)
{
    switch (kind) {
    case ITEM_STRING: {
        Addr pos = addr_of(*p);
        v->a = strg_item(S, &pos);
        *p = at(pos);
        break;
    }
    case ITEM_DOUBLE: v->d = dubl_in(S, p, nullptr); break;
    case ITEM_SNGL: v->u = sngl_in(S, p); break;
    }
    return comp_eos(S, p) || **p == ',';
}

static void push_item(Session *const S, const ItemKind kind, const Cell v)
{
    if (kind == ITEM_DOUBLE)
        exec_push(&S->vm, v);
    else
        exec_pusha(&S->vm, v.u);
}

/* The next field. An empty line leaves the rest of the INPUT. File data: a bad number in INPUT#. */
static void input(Session *const S, const ItemKind kind)
{
    if (redo(S))
        return;
    if (!S->cons.inlen) {
        S->vm.sp = S->vm.rsp;
        S->vm.ip = S->stmt.input_end;
        return;
    }
    cons_field(S);
    const uint8_t *p = S->cons.inbuf + S->cons.inpos;
    Cell v = {};
    const bool ok = read_item(S, &p, kind, &v);
    S->cons.inpos = (int32_t)(p - S->cons.inbuf);
    if (!ok) {
        if (S->cons.from_file)
            repl_error(S, E_FD);
        S->stmt.redo = true; /* stored as far as it went, then REDO */
    }
    push_item(S, kind, v);
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
        cons_msg(S, MSG_EXTRA);
}

/* GET to a number: the key waiting as a digit, 0 none. Syntax error: not a digit. */
void stmt_fget(Session *const S)
{
    const uint8_t c = cons_getc(S);
    const uint8_t digit = c ? c - '0' : 0;
    if (digit >= 10)
        repl_error(S, E_SN);
    exec_pusha(&S->vm, fmth_from_i16(digit));
}

/* GET to a string: the key waiting, or none. */
void stmt_sget(Session *const S)
{
    const uint8_t c = cons_getc(S);
    if (!c) {
        exec_pusha(&S->vm, strg_push(S, (Desc){}));
        return;
    }
    S->cons.sbuf[0] = c;
    exec_pusha(&S->vm, strg_buftmp(S, 1));
}

/* ---- READ and DATA -------------------------------------------------------------------------- */

/* Past a statement: *b past its ':' or its 0, which it returns. */
static uint8_t skip_stmt(Session *const S, uint32_t *const b)
{
    const uint8_t *const start = at(prog_addr(&S->prog, *b));
    const uint8_t *p = start;
    while (*p && *p != ':')
        p = decl_item(p);
    *b += (uint32_t)(p - start) + 1;
    return *p;
}

/* The first DATA at or after the statement starting at *b; *b its first byte. */
static bool scan_data(Session *const S, uint32_t *const b)
{
    for (;;) {
        uint8_t c;
        while ((c = rd8(prog_addr(&S->prog, *b))) == ' ')
            ++*b;
        if (c == TK_DATA) {
            ++*b;
            return true;
        }
        if (!c)
            ++*b;
        else if (skip_stmt(S, b))
            continue;
        if (!prog_line(&S->prog, *b)->len) /* the next line, or the end */
            return false;
        *b += HDR;
    }
}

/* The first DATA after the statement *b is in. */
static bool data_after(Session *const S, uint32_t *const b)
{
    if (!skip_stmt(S, b)) {
        if (!prog_line(&S->prog, *b)->len)
            return false;
        *b += HDR;
    }
    return scan_data(S, b);
}

/* The text of the DATA at b to data_text, up to its ':'. */
static void load_data(Session *const S, const uint32_t b)
{
    S->stmt.data_at = b;
    const uint8_t *const text = at(prog_addr(&S->prog, b));
    const size_t n = (size_t)(comp_skipdata(text) - text);
    memcpy(S->stmt.data_text, text, n);
    S->stmt.data_text[n] = 0;
}

/* READ goes on at the first DATA at or after line. */
static void seek_data(Session *const S, const uint32_t line)
{
    uint32_t b = line + HDR;
    if (prog_line(&S->prog, line)->len && scan_data(S, &b))
        load_data(S, b);
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
    if (!S->stmt.data_pos) {
        seek_data(S, PROGBASE);
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
    uint32_t b = S->stmt.data_at;
    if (!b || !data_after(S, &b))
        repl_error(S, E_OD);
    load_data(S, b);
    return S->stmt.data_text;
}

/* The next DATA item. Syntax error, in the DATA's line: a bad item. */
static void read(Session *const S, const ItemKind kind)
{
    const uint8_t *p = next_data(S);
    Cell v = {};
    if (!read_item(S, &p, kind, &v)) {
        uint32_t line = PROGBASE;
        while (S->stmt.data_at >= line + prog_line(&S->prog, line)->len)
            line += prog_line(&S->prog, line)->len;
        S->vm.line = line;
        repl_error(S, E_SN);
    }
    S->stmt.data_pos = p;
    push_item(S, kind, v);
}
void stmt_fread(Session *const S) { read(S, ITEM_SNGL); }
void stmt_sread(Session *const S) { read(S, ITEM_STRING); }
void stmt_dread(Session *const S) { read(S, ITEM_DOUBLE); }

/* RESTORE: the first DATA. */
void stmt_res(Session *const S) { stmt_restore(S); }
void stmt_restore(Session *const S) { S->stmt.data_pos = nullptr; }

/* RESTORE line. Undefined line number. */
void stmt_resl(Session *const S)
{
    const uint32_t x = exec_pop(&S->vm).u;
    uint32_t line;
    if (x >= LABEL_LINE) {
        line = x - LABEL_LINE;
        if (!line)
            repl_error(S, E_US);
    } else {
        S->vm.lnum = (uint16_t)x;
        if (!prog_find(S, (uint16_t)x, &line))
            repl_error(S, E_US);
    }
    seek_data(S, line);
}

/* ---- DIM and ERASE -------------------------------------------------------------------------- */

/* DIM ( i1..in mlo mhi id flags -- ): bit i of m: subscript i was written hi, base. As arry_dim. */
void stmt_dim(Session *const S)
{
    const uint32_t flags = exec_pop(&S->vm).u;
    const uint8_t dims = flags & DIM_COUNT;
    const uint16_t id = (uint16_t)exec_pop(&S->vm).u;
    const uint32_t mask_hi = exec_pop(&S->vm).u;
    const uint64_t swapped = (uint64_t)mask_hi << 32 | exec_pop(&S->vm).u;
    for (uint8_t i = dims; i-- > 0;) { /* lo TO hi, or hi and the base */
        const int32_t second = exec_popi(&S->vm);
        const int32_t first = exec_popi(&S->vm);
        const bool swap = swapped >> i & 1;
        const int32_t hi = swap ? first : second;
        const int32_t lo = swap ? second : first;
        S->arry.subs[i] = (uint16_t)(hi - lo);
        S->arry.lbounds[i] = (int16_t)lo;
    }
    Var *const v = symb_var(&S->symb, id);
    if (!(flags & DIM_REDIM)) {
        if (!(flags & DIM_STATIC) || !v->vt) /* a STATIC's, dimensioned: kept */
            arry_dim(S, id, dims, false);
        return;
    }
    const Addr old = v->vt; /* REDIM: the old one goes */
    if (old && flags & DIM_PRESERVE) { /* the new one made, the old one's elements copied */
        if (dims != ((const ArrayHdr *)at(old))->dims)
            arry_bad_sub(S);
        v->vt = 0;
        arry_dim(S, id, dims, true);
        const Addr fresh = symb_var(&S->symb, id)->vt;
        arry_keep(S, id, old, fresh);
        symb_var(&S->symb, id)->vt = old;
        const uint32_t freed = arry_free(S, id, AREP);
        symb_var(&S->symb, id)->vt = fresh - freed; /* its header was after the old one's */
    } else {
        if (old)
            arry_free(S, id, AREP);
        arry_dim(S, id, dims, true);
    }
    arry_rep(S, id);
}

/* ERASE ( id -- ): REDIM's array freed, another's elements zeroed. */
void stmt_erase(Session *const S)
{
    const uint16_t id = (uint16_t)exec_pop(&S->vm).u;
    const Addr hdr = symb_var(&S->symb, id)->vt;
    if (!hdr)
        return;
    if (symb_varx(&S->symb, id)->dyn)
        arry_free(S, id, 0);
    else
        copy_zero(((const ArrayHdr *)at(hdr))->data, arry_size(S, id, hdr));
}

/* ---- RUN, LIST, CONT, END, STOP ------------------------------------------------------------- */

/* Back to READY, the frames kept. */
[[noreturn]] static void ready(Session *const S)
{
    evnt_clear(S);
    S->vm.stop &= ~SF_EV;
    repl_ready(S);
}

void stmt_rungo(Session *const S, const uint32_t line)
{
    S->vm.sp = S->vm.rsp;
    evnt_clear(S);
    chai_start(S);
    S->vm.err.handler = 0;
    S->vm.err.active = false;
    S->vm.err.code = 0;
    S->vm.err.line = 0;
    S->vm.nframes = 0;
    S->vm.ncallvars = 0;
    S->strg.tmp_floor = 0;
    flow_jump(S, line);
}

/* RUN line. Undefined line number; a label. */
void stmt_runl(Session *const S)
{
    const uint32_t num = exec_pop(&S->vm).u;
    if (num >= LABEL_LINE)
        repl_error(S, E_US); /* RUN takes a number, not a label */
    symb_clear(S);
    link_start(S);
    S->vm.lnum = (uint16_t)num;
    uint32_t line;
    if (!prog_find(S, (uint16_t)num, &line))
        repl_error(S, E_US);
    stmt_rungo(S, line);
}

void stmt_run(Session *const S)
{
    symb_clear(S);
    link_start(S);
    stmt_runfirst(S);
}

void stmt_runfirst(Session *const S)
{
    chnl_closeall(S);
    if (!prog_line(&S->prog, PROGBASE)->len)
        flow_end(S);
    stmt_rungo(S, PROGBASE);
}

/* LIST ( from to -- ), then READY. */
void stmt_list(Session *const S)
{
    const uint16_t to = (uint16_t)exec_pop(&S->vm).u;
    const uint16_t from = (uint16_t)exec_pop(&S->vm).u;
    list_range(S, from, to);
    ready(S);
}

void stmt_cont(Session *const S)
{
    if (!S->vm.cont.line)
        repl_error(S, E_CN);
    if (!S->vm.cont.code)
        ready(S);
    S->vm.line = S->vm.cont.line;
    S->vm.ip = flow_place(S, S->vm.cont);
}

/* CLEAR: the variables, and the frames. */
void stmt_clr(Session *const S)
{
    symb_clear(S);
    S->vm.nframes = 0;
}

void stmt_new(Session *const S)
{
    prog_init(S);
    flow_end(S);
}

/* Illegal direct in the direct line. */
void stmt_nodir(Session *const S) { flow_nodir(S); }

void stmt_end(Session *const S)
{
    if (S->dbug.probing)
        dbug_done(S);
    chnl_closeall(S);
    flow_setcont(S);
    ready(S);
}

void stmt_stop(Session *const S)
{
    S->vm.stop = 0;
    flow_setcont(S);
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
static void wait(Session *const S, const uint8_t flip)
{
    const uint8_t mask = (uint8_t)exec_pop(&S->vm).u;
    const uint16_t port = (uint16_t)exec_pop(&S->vm).u;
    while (!((psys_inp(port) ^ flip) & mask))
        ;
}
void stmt_wait(Session *const S) { wait(S, (uint8_t)exec_pop(&S->vm).u); }
void stmt_wait2(Session *const S) { wait(S, 0); }

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
    const Addr a = S->allo.poke + (uint16_t)exec_pop(&S->vm).u;
    if (rd8(a))
        psys_call(a);
}

/* ---- ^ and TI ------------------------------------------------------------------------------- */

/* INT ^ INT: 0 for a negative power past |base| 1. Overflow past an INT. */
static int32_t int_pow(Session *const S, const int32_t base, const int32_t power)
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
            repl_error(S, E_OV);
    return r;
}

void stmt_ipow(Session *const S)
{
    const int32_t power = exec_popi(&S->vm);
    const int32_t base = exec_popi(&S->vm);
    exec_pushi(&S->vm, int_pow(S, base, power));
}

void stmt_fpow(Session *const S)
{
    const Sngl power = exec_pop(&S->vm).u;
    const Sngl base = exec_pop(&S->vm).u;
    exec_pusha(&S->vm, fmth_pow(S, base, power));
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
    exec_pusha(&S->vm, strg_buftmp(S, 6));
}

/* ST: the I/O status. */
void stmt_ist(Session *const S) { exec_pushi(&S->vm, S->cons.status); }
