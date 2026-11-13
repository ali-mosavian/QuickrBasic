/* The statements' CALL routines (src/vm/calls.asm): PRINT, INPUT, READ and DATA, DIM, RUN, LIST,
   CONT, END, STOP, POKE, WAIT, INP, OUT, SYS, ^. */
#include "vm/stmt.h"
#include "comp/comp.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "comp/link.h"
#include "comp/list.h"
#include "io/chnl.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/arry.h"
#include "mem/copy.h"
#include "mem/prog.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/dubl.h"
#include "num/fmth.h"
#include "plat/pclk.h"
#include "plat/psys.h"
#include "vm/chai.h"
#include "vm/dbug.h"
#include "vm/evnt.h"
#include "vm/flow.h"

enum { DATA_TEXT_SIZE = 256, PRINT_ZONE = 14 };

/* What an INPUT field or a DATA item is read as. */
typedef enum : uint8_t { ITEM_SNGL, ITEM_STRING, ITEM_DOUBLE } ItemKind;

StmtState stmt;

void stmt_init(void) { stmt.data_text = at(allo_carve(DATA_TEXT_SIZE)); }

/* ---- PRINT ---------------------------------------------------------------------------------- */

void stmt_ipr(Vm *const vm) { cons_int(exec_popi(vm)); }
void stmt_fpr(Vm *const vm) { sngl_print(exec_pop(vm).u); }
void stmt_spr(Vm *const vm) { strg_print(strg_get(exec_popa(vm))); }
void stmt_pnl(Vm *const) { cons_nl(); }

/* PR.ZONE: to the next zone. */
void stmt_pzone(Vm *const)
{
    do
        cons_space();
    while (cons.col % PRINT_ZONE);
}

/* TAB(i): to column i, from 1; past it, on the next line. */
void stmt_ptab(Vm *const vm)
{
    const int32_t i = exec_popi(vm) - 1;
    const uint8_t col = i > 0 ? (uint8_t)i : 0;
    if (col < cons.col)
        cons_nl();
    if (col > cons.col)
        cons_spaces(col - cons.col);
}

void stmt_pspc(Vm *const vm) { cons_spaces((uint8_t)exec_popi(vm)); }

/* ---- INPUT ---------------------------------------------------------------------------------- */

/* IN.BEGIN end:2: not direct; REDO comes back here. The end is where an empty line goes on. */
void stmt_inbeg(Vm *const vm)
{
    flow_nodir(vm);
    stmt.redo = false;
    stmt.input_end = at(allo.cache + exec_u16(vm));
    stmt.input_start = vm->ip;
}

/* ? and a line. */
void stmt_inpr(Vm *const) { cons_prompt(); }

/* INPUT#'s line, empty lines skipped. */
void stmt_infile(Vm *const)
{
    cons.from_file = true;
    do
        cons_read();
    while (!cons.inlen);
}

/* A line, no ?: INPUT "prompt", .. */
void stmt_inln(Vm *const) { cons_read(); }

/* After a bad field's store: ?Redo from start, the INPUT again. true: vm->ip moved. */
static bool redo(Vm *const vm)
{
    if (!stmt.redo)
        return false;
    stmt.redo = false;
    cons_msg(MSG_REDO);
    vm->ip = stmt.input_start;
    vm->sp = vm->rsp;
    return true;
}

/* An item at *p, ending at the statement's end or ','; *p at that end. false: it ends elsewhere
   (bad), the value read as far as it went. */
static bool read_item(const uint8_t **const p, const ItemKind kind, Cell *const v)
{
    switch (kind) {
    case ITEM_STRING: {
        Addr pos = addr_of(*p);
        v->a = strg_item(&pos);
        *p = at(pos);
        break;
    }
    case ITEM_DOUBLE: v->d = dubl_in(p, nullptr); break;
    case ITEM_SNGL: v->u = sngl_in(p); break;
    }
    return comp_eos(p) || **p == ',';
}

static void push_item(Vm *const vm, const ItemKind kind, const Cell v)
{
    if (kind == ITEM_DOUBLE)
        exec_push(vm, v);
    else
        exec_pusha(vm, v.u);
}

/* The next field. An empty line leaves the rest of the INPUT. File data: a bad number in INPUT#. */
static void input(Vm *const vm, const ItemKind kind)
{
    if (redo(vm))
        return;
    if (!cons.inlen) {
        vm->sp = vm->rsp;
        vm->ip = stmt.input_end;
        return;
    }
    cons_field();
    const uint8_t *p = cons.inbuf + cons.inpos;
    Cell v = {};
    const bool ok = read_item(&p, kind, &v);
    cons.inpos = (int32_t)(p - cons.inbuf);
    if (!ok) {
        if (cons.from_file)
            repl_error(E_FD);
        stmt.redo = true; /* stored as far as it went, then REDO */
    }
    push_item(vm, kind, v);
}
void stmt_fin(Vm *const vm) { input(vm, ITEM_SNGL); }
void stmt_sin(Vm *const vm) { input(vm, ITEM_STRING); }
void stmt_din(Vm *const vm) { input(vm, ITEM_DOUBLE); }

/* IN.END: ?Extra ignored for fields left. */
void stmt_inend(Vm *const vm)
{
    if (redo(vm) || cons.from_file || cons.inpos == -1)
        return;
    if (cons.inbuf[cons.inpos])
        cons_msg(MSG_EXTRA);
}

/* GET to a number: the key waiting as a digit, 0 none. Syntax error: not a digit. */
void stmt_fget(Vm *const vm)
{
    const uint8_t c = cons_getc();
    const uint8_t digit = c ? c - '0' : 0;
    if (digit >= 10)
        repl_error(E_SN);
    exec_pusha(vm, fmth_from_i16(digit));
}

/* GET to a string: the key waiting, or none. */
void stmt_sget(Vm *const vm)
{
    const uint8_t c = cons_getc();
    if (!c) {
        exec_pusha(vm, strg_push((Desc){}));
        return;
    }
    cons.sbuf[0] = c;
    exec_pusha(vm, strg_buftmp(1));
}

/* ---- READ and DATA -------------------------------------------------------------------------- */

/* Past a statement: *b past its ':' or its 0, which it returns. */
static uint8_t skip_stmt(uint32_t *const b)
{
    const uint8_t *const start = at(prog_addr(*b));
    const uint8_t *p = start;
    while (*p && *p != ':')
        p = decl_item(p);
    *b += (uint32_t)(p - start) + 1;
    return *p;
}

/* The first DATA at or after the statement starting at *b; *b its first byte. */
static bool scan_data(uint32_t *const b)
{
    for (;;) {
        uint8_t c;
        while ((c = rd8(prog_addr(*b))) == ' ')
            ++*b;
        if (c == TK_DATA) {
            ++*b;
            return true;
        }
        if (!c)
            ++*b;
        else if (skip_stmt(b))
            continue;
        if (!prog_line(*b)->len) /* the next line, or the end */
            return false;
        *b += HDR;
    }
}

/* The first DATA after the statement *b is in. */
static bool data_after(uint32_t *const b)
{
    if (!skip_stmt(b)) {
        if (!prog_line(*b)->len)
            return false;
        *b += HDR;
    }
    return scan_data(b);
}

/* The text of the DATA at b to data_text, up to its ':'. */
static void load_data(const uint32_t b)
{
    stmt.data_at = b;
    const uint8_t *const text = at(prog_addr(b));
    const size_t n = (size_t)(comp_skipdata(text) - text);
    memcpy(stmt.data_text, text, n);
    stmt.data_text[n] = 0;
}

/* READ goes on at the first DATA at or after line. */
static void seek_data(const uint32_t line)
{
    uint32_t b = line + HDR;
    if (prog_line(line)->len && scan_data(&b))
        load_data(b);
    else {
        stmt.data_at = 0;
        stmt.data_text[0] = 0;
    }
    stmt.data_pos = stmt.data_text;
    stmt.before_first = true;
}

/* The next DATA item. Out of DATA. */
static const uint8_t *next_data(void)
{
    if (!stmt.data_pos) {
        seek_data(PROGBASE);
        if (!stmt.data_at)
            repl_error(E_OD);
    }
    const uint8_t *p = stmt.data_pos;
    if (stmt.before_first) {
        stmt.before_first = false;
        return p;
    }
    if (comp_peek(&p) == ',')
        return p + 1;
    uint32_t b = stmt.data_at;
    if (!b || !data_after(&b))
        repl_error(E_OD);
    load_data(b);
    return stmt.data_text;
}

/* The next DATA item. Syntax error, in the DATA's line: a bad item. */
static void read(Vm *const vm, const ItemKind kind)
{
    const uint8_t *p = next_data();
    Cell v = {};
    if (!read_item(&p, kind, &v)) {
        uint32_t line = PROGBASE;
        while (stmt.data_at >= line + prog_line(line)->len)
            line += prog_line(line)->len;
        vm->line = line;
        repl_error(E_SN);
    }
    stmt.data_pos = p;
    push_item(vm, kind, v);
}
void stmt_fread(Vm *const vm) { read(vm, ITEM_SNGL); }
void stmt_sread(Vm *const vm) { read(vm, ITEM_STRING); }
void stmt_dread(Vm *const vm) { read(vm, ITEM_DOUBLE); }

/* RESTORE: the first DATA. */
void stmt_res(Vm *const) { stmt_restore(); }
void stmt_restore(void) { stmt.data_pos = nullptr; }

/* RESTORE line. Undefined line number. */
void stmt_resl(Vm *const vm)
{
    const uint32_t x = exec_pop(vm).u;
    uint32_t line;
    if (x >= LABEL_LINE) {
        line = x - LABEL_LINE;
        if (!line)
            repl_error(E_US);
    } else {
        vm->lnum = (uint16_t)x;
        if (!prog_find((uint16_t)x, &line))
            repl_error(E_US);
    }
    seek_data(line);
}

/* ---- DIM and ERASE -------------------------------------------------------------------------- */

/* DIM ( i1..in mlo mhi id flags -- ): bit i of m: subscript i was written hi, base. As arry_dim. */
void stmt_dim(Vm *const vm)
{
    const uint32_t flags = exec_pop(vm).u;
    const uint8_t dims = flags & DIM_COUNT;
    const uint16_t id = (uint16_t)exec_pop(vm).u;
    const uint32_t mask_hi = exec_pop(vm).u;
    const uint64_t swapped = (uint64_t)mask_hi << 32 | exec_pop(vm).u;
    for (uint8_t i = dims; i-- > 0;) { /* lo TO hi, or hi and the base */
        const int32_t second = exec_popi(vm);
        const int32_t first = exec_popi(vm);
        const bool swap = swapped >> i & 1;
        const int32_t hi = swap ? first : second;
        const int32_t lo = swap ? second : first;
        arry.subs[i] = (uint16_t)(hi - lo);
        arry.lbounds[i] = (int16_t)lo;
    }
    Var *const v = symb_var(id);
    if (!(flags & DIM_REDIM)) {
        if (!(flags & DIM_STATIC) || !v->vt) /* a STATIC's, dimensioned: kept */
            arry_dim(id, dims, false);
        return;
    }
    const Addr old = v->vt; /* REDIM: the old one goes */
    if (old && flags & DIM_PRESERVE) { /* the new one made, the old one's elements copied */
        if (dims != ((const ArrayHdr *)at(old))->dims)
            arry_bad_sub();
        v->vt = 0;
        arry_dim(id, dims, true);
        const Addr fresh = symb_var(id)->vt;
        arry_keep(id, old, fresh);
        symb_var(id)->vt = old;
        const uint32_t freed = arry_free(id, AREP);
        symb_var(id)->vt = fresh - freed; /* its header was after the old one's */
    } else {
        if (old)
            arry_free(id, AREP);
        arry_dim(id, dims, true);
    }
    arry_rep(id);
}

/* ERASE ( id -- ): REDIM's array freed, another's elements zeroed. */
void stmt_erase(Vm *const vm)
{
    const uint16_t id = (uint16_t)exec_pop(vm).u;
    const Addr hdr = symb_var(id)->vt;
    if (!hdr)
        return;
    if (symb_varx(id)->dyn)
        arry_free(id, 0);
    else
        copy_zero(((const ArrayHdr *)at(hdr))->data, arry_size(id, hdr));
}

/* ---- RUN, LIST, CONT, END, STOP ------------------------------------------------------------- */

/* Back to READY, the frames kept. */
[[noreturn]] static void ready(Vm *const vm)
{
    evnt_clear();
    vm->stop &= ~SF_EV;
    repl_ready();
}

void stmt_rungo(const uint32_t line)
{
    vm.sp = vm.rsp;
    evnt_clear();
    chai_start();
    vm.err.handler = 0;
    vm.err.active = false;
    vm.err.code = 0;
    vm.err.line = 0;
    vm.nframes = 0;
    vm.ncallvars = 0;
    strg.tmp_floor = 0;
    flow_jump(&vm, line);
}

/* RUN line. Undefined line number; a label. */
void stmt_runl(Vm *const vm)
{
    const uint32_t num = exec_pop(vm).u;
    if (num >= LABEL_LINE)
        repl_error(E_US); /* RUN takes a number, not a label */
    symb_clear();
    link_start();
    vm->lnum = (uint16_t)num;
    uint32_t line;
    if (!prog_find((uint16_t)num, &line))
        repl_error(E_US);
    stmt_rungo(line);
}

void stmt_run(Vm *const)
{
    symb_clear();
    link_start();
    stmt_runfirst();
}

void stmt_runfirst(void)
{
    chnl_closeall();
    if (!prog_line(PROGBASE)->len)
        flow_end(&vm);
    stmt_rungo(PROGBASE);
}

/* LIST ( from to -- ), then READY. */
void stmt_list(Vm *const vm)
{
    const uint16_t to = (uint16_t)exec_pop(vm).u;
    const uint16_t from = (uint16_t)exec_pop(vm).u;
    list_range(from, to);
    ready(vm);
}

void stmt_cont(Vm *const vm)
{
    if (!vm->cont.line)
        repl_error(E_CN);
    if (!vm->cont.code)
        ready(vm);
    vm->line = vm->cont.line;
    vm->ip = flow_place(vm->cont);
}

/* CLEAR: the variables, and the frames. */
void stmt_clr(Vm *const vm)
{
    symb_clear();
    vm->nframes = 0;
}

void stmt_new(Vm *const vm)
{
    prog_init();
    flow_end(vm);
}

/* Illegal direct in the direct line. */
void stmt_nodir(Vm *const vm) { flow_nodir(vm); }

void stmt_end(Vm *const vm)
{
    if (dbug.probing)
        dbug_done();
    chnl_closeall();
    flow_setcont(vm);
    ready(vm);
}

void stmt_stop(Vm *const vm)
{
    vm->stop = 0;
    flow_setcont(vm);
    repl_error(E_BRK);
}

/* ---- the machine ---------------------------------------------------------------------------- */

/* POKE ( addr byte -- ). */
void stmt_poke(Vm *const vm)
{
    const uint8_t b = (uint8_t)exec_pop(vm).u;
    wr8(exec_popa(vm), b);
}

/* WAIT ( port mask -- ): until (INP XOR flip) AND mask. */
static void wait(Vm *const vm, const uint8_t flip)
{
    const uint8_t mask = (uint8_t)exec_pop(vm).u;
    const uint16_t port = (uint16_t)exec_pop(vm).u;
    while (!((psys_inp(port) ^ flip) & mask))
        ;
}
void stmt_wait(Vm *const vm) { wait(vm, (uint8_t)exec_pop(vm).u); }
void stmt_wait2(Vm *const vm) { wait(vm, 0); }

/* INP ( port -- b ); OUT ( port b -- ). */
void stmt_inp(Vm *const vm) { exec_pushi(vm, psys_inp((uint16_t)exec_pop(vm).u)); }
void stmt_out(Vm *const vm)
{
    const uint8_t b = (uint8_t)exec_pop(vm).u;
    psys_outp((uint16_t)exec_pop(vm).u, b);
}

/* SYS ( addr -- ): a call into the POKE memory; nothing poked there: a no-op. */
void stmt_sys(Vm *const vm)
{
    const Addr a = allo.poke + (uint16_t)exec_pop(vm).u;
    if (rd8(a))
        psys_call(a);
}

/* ---- ^ and TI ------------------------------------------------------------------------------- */

/* INT ^ INT: 0 for a negative power past |base| 1. Overflow past an INT. */
static int32_t int_pow(const int32_t base, const int32_t power)
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
            repl_error(E_OV);
    return r;
}

void stmt_ipow(Vm *const vm)
{
    const int32_t power = exec_popi(vm);
    const int32_t base = exec_popi(vm);
    exec_pushi(vm, int_pow(base, power));
}

void stmt_fpow(Vm *const vm)
{
    const Sngl power = exec_pop(vm).u;
    const Sngl base = exec_pop(vm).u;
    exec_pusha(vm, fmth_pow(base, power));
}

/* TI: the jiffies. */
void stmt_fti(Vm *const vm)
{
    const uint32_t j = pclk_jiffies();
    exec_pusha(vm, j ? sngl_norm(j, false) : 0);
}

/* TI$: "HHMMSS" from the hundredths. */
void stmt_sti(Vm *const vm)
{
    uint32_t t = pclk_jiffies() / 100;
    for (int i = 4; i >= 0; i -= 2) {
        const uint32_t d = t % 60;
        t /= 60;
        cons.sbuf[i] = '0' + d / 10;
        cons.sbuf[i + 1] = '0' + d % 10;
    }
    exec_pusha(vm, strg_buftmp(6));
}

/* ST: the I/O status. */
void stmt_ist(Vm *const vm) { exec_pushi(vm, cons.status); }
