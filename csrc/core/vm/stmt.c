/* The statements' CALL routines (src/vm/calls.asm): PRINT, INPUT, READ and DATA, DIM, RUN, LIST,
   CONT, END, STOP, POKE, WAIT, INP, OUT, SYS, ^. */
#include "vm/stmt.h"
#include "comp/comp.h"
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

enum { RDSZ = 256 };

/* What an item reads: a FLOAT, a string, a DOUBLE (the asm's rn.lk 0, 2, 3). */
enum : uint8_t { LK_FLT = 0, LK_STR = 2, LK_DBL = 3 };

static struct {
    uint8_t *rdbuf;      /* g_rdbuf: the DATA in hand's text, in the arena */
    const uint8_t *dlp;  /* rn.dlp: READ's place in rdbuf, null before the first */
    uint32_t dataat;     /* rn.dataat: the DATA's first byte in the store, 0 none */
    bool before;         /* rn.dyo: dlp is before a DATA's first item */
    uint8_t *in_end;     /* rn.inend: where an empty INPUT line goes on */
    uint8_t *in_start;   /* rn.iny0: REDO comes back here */
    bool redo;           /* rn.iredo: a bad field: REDO FROM START after the stores */
} st;

void stmt_init(void) { st.rdbuf = at(allo_carve(RDSZ)); }

/* PRINT a value ( x -- ). */
void stmt_ipr(void) { cons_int(exec_popi()); }
void stmt_fpr(void) { sngl_print(exec_pop().u); }
void stmt_spr(void) { strg_print(strg_get(exec_popa())); }
void stmt_pnl(void) { cons_nl(); }

/* PR.ZONE: to the next 14-column zone. */
void stmt_pzone(void)
{
    do
        cons_space();
    while (cons.col % 14);
}

/* PR.TAB ( i -- ): to column i, from 1; past it, on the next line. */
void stmt_ptab(void)
{
    int32_t i = exec_popi() - 1;
    uint8_t c = i > 0 ? (uint8_t)i : 0;
    if (c < cons.col)
        cons_nl();
    if (c > cons.col)
        cons_spaces(c - cons.col);
}

/* PR.SPC ( i -- ). */
void stmt_pspc(void) { cons_spaces((uint8_t)exec_popi()); }

/* IN.BEGIN end:2: not direct; REDO comes back here. The end is where an empty line goes on. */
void stmt_inbeg(void)
{
    flow_nodir();
    st.redo = false;
    st.in_end = at(allo.cache + exec_u16());
    st.in_start = exec.ip;
}

/* IN.PROMPT: ? and a line. */
void stmt_inpr(void) { cons_prompt(); }

/* IN.FILE: INPUT#'s line, empty lines skipped. */
void stmt_infile(void)
{
    cons.from_file = true;
    do
        cons_read();
    while (!cons.inlen);
}

/* IN.LINE: a line, no ?: INPUT "prompt", .. */
void stmt_inln(void) { cons_read(); }

/* io_redo: after a bad field's store: ?Redo from start, the INPUT again. true: exec.ip moved. */
static bool redo(void)
{
    if (!st.redo)
        return false;
    st.redo = false;
    cons_msg(MSG_REDO);
    exec.ip = st.in_start;
    exec.sp = exec.rsp;
    return true;
}

/* io_item: an item at *p of kind lk, ending at the statement's end or ','; *p at that end.
   false: it ends elsewhere (bad), the value read as far as it went. */
static bool item(const uint8_t **p, uint8_t lk, Cell *v)
{
    if (lk == LK_STR) {
        Addr pos = addr_of(*p);
        v->raw = strg_item(&pos);
        *p = at(pos);
    } else if (lk == LK_DBL)
        v->d = dubl_in(p, nullptr);
    else
        v->raw = sngl_in(p);
    return comp_eos(p) || **p == ',';
}

static void push_item(uint8_t lk, Cell v)
{
    if (lk != LK_DBL)
        v.raw = v.u;
    exec_push(v);
}

/* F.INPUT S.INPUT D.INPUT ( -- x ): the next field. An empty line leaves the rest of the INPUT.
   ?File data: a bad number in INPUT#. */
static void input(uint8_t lk)
{
    if (redo())
        return;
    if (!cons.inlen) {
        exec.sp = exec.rsp;
        exec.ip = st.in_end;
        return;
    }
    cons_field();
    const uint8_t *p = cons.inbuf + cons.inpos;
    Cell v = {};
    bool ok = item(&p, lk, &v);
    cons.inpos = (int32_t)(p - cons.inbuf);
    if (!ok) {
        if (cons.from_file)
            repl_error(E_FD);
        st.redo = true; /* stored as far as it went, then REDO */
    }
    push_item(lk, v);
}
void stmt_fin(void) { input(LK_FLT); }
void stmt_sin(void) { input(LK_STR); }
void stmt_din(void) { input(LK_DBL); }

/* IN.END: ?Extra ignored for fields left. */
void stmt_inend(void)
{
    if (redo() || cons.from_file || cons.inpos == -1)
        return;
    if (cons.inbuf[cons.inpos])
        cons_msg(MSG_EXTRA);
}

/* F.GET ( -- f ): the key waiting as a digit, 0 none. Syntax error: not a digit. */
void stmt_fget(void)
{
    uint8_t c = cons_getc();
    if (c) {
        c ^= '0';
        if (c >= 10)
            repl_error(E_SN);
    }
    exec_pusha(fmth_from_i16(c));
}

/* S.GET ( -- s ): the key waiting, or none. */
void stmt_sget(void)
{
    uint8_t c = cons_getc();
    if (!c) {
        exec_pusha(strg_push((Desc){}));
        return;
    }
    cons.sbuf[0] = c;
    exec_pusha(strg_buftmp(1));
}

/* rd_sk: past a statement: *b past its ':' or its 0, which it returns. */
static uint8_t skip_stmt(uint32_t *b)
{
    bool quoted = false;
    for (;;) {
        uint8_t c = rd8(prog_addr((*b)++));
        if (!c)
            return 0;
        if (c == '"')
            quoted = !quoted;
        else if (quoted)
            continue;
        else if (c == ':')
            return c;
        else if (c == M_INT)
            *b += 4;
        else if (c == M_VAR)
            *b += rd8(prog_addr(*b)) + 3u;
    }
}

/* rd_scan: the first DATA at or after the statement starting at *b; *b its first byte. */
static bool scan_data(uint32_t *b)
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

/* rd_after: the first DATA after the statement *b is in. */
static bool data_after(uint32_t *b)
{
    if (!skip_stmt(b)) {
        if (!prog_line(*b)->len)
            return false;
        *b += HDR;
    }
    return scan_data(b);
}

/* rd_load: the text of the DATA at b to rdbuf, up to a ':' outside quotes. */
static void load_data(uint32_t b)
{
    st.dataat = b;
    bool quoted = false;
    uint8_t *d = st.rdbuf;
    for (uint8_t c; (c = rd8(prog_addr(b++))) && (c != ':' || quoted); *d++ = c)
        if (c == '"')
            quoted = !quoted;
    *d = 0;
}

/* rd_seek: READ goes on at the first DATA at or after line. */
static void seek_data(uint32_t line)
{
    uint32_t b = line + HDR;
    if (prog_line(line)->len && scan_data(&b))
        load_data(b);
    else {
        st.dataat = 0;
        st.rdbuf[0] = 0;
    }
    st.dlp = st.rdbuf;
    st.before = true;
}

/* rd_item: the next DATA item. Out of DATA. */
static const uint8_t *data_item(void)
{
    const uint8_t *p = st.dlp;
    if (!p) {
        seek_data(PROGBASE);
        if (!st.dataat)
            repl_error(E_OD);
        p = st.dlp;
    }
    if (st.before) {
        st.before = false;
        return p;
    }
    if (comp_peek(&p) == ',')
        return p + 1;
    uint32_t b = st.dataat;
    if (!b || !data_after(&b))
        repl_error(E_OD);
    load_data(b);
    return st.rdbuf;
}

/* F.READ S.READ D.READ ( -- x ): the next DATA item. Syntax error, in the DATA's line: a bad
   item. */
static void read(uint8_t lk)
{
    const uint8_t *p = data_item();
    Cell v = {};
    if (!item(&p, lk, &v)) {
        uint32_t l = PROGBASE;
        while (st.dataat >= l + prog_line(l)->len)
            l += prog_line(l)->len;
        exec.line = l;
        repl_error(E_SN);
    }
    st.dlp = p;
    push_item(lk, v);
}
void stmt_fread(void) { read(LK_FLT); }
void stmt_sread(void) { read(LK_STR); }
void stmt_dread(void) { read(LK_DBL); }

/* RESTORE: the first DATA. */
void stmt_res(void) { stmt_restore(); }
void stmt_restore(void) { st.dlp = nullptr; }

/* RESTORE.L ( line -- ): a number, or 10000h and a label's line. Undefined line number. */
void stmt_resl(void)
{
    uint32_t x = exec_pop().u;
    uint32_t line;
    if (x >= 0x10000) {
        line = x - 0x10000;
        if (!line)
            repl_error(E_US);
    } else {
        exec.lnum = (uint16_t)x;
        if (!prog_find((uint16_t)x, &line))
            repl_error(E_US);
    }
    seek_data(line);
}

/* DIM ( i1..in mlo mhi id n -- ): n bit 6 REDIM, bit 7 a STATIC's, bit 8 PRESERVE; m, bit i:
   subscript i is hi, base. As arry_dim. */
void stmt_dim(void)
{
    uint32_t n = exec_pop().u;
    uint8_t dims = n & 0x3F;
    uint16_t id = (uint16_t)exec_pop().u;
    uint64_t mask = (uint64_t)exec_pop().u << 32;
    mask |= exec_pop().u;
    for (int i = dims; i > 0; i--) { /* lo TO hi, or hi and the base */
        int32_t hi = exec_popi();
        int32_t lo = exec_popi();
        if (mask >> (i - 1) & 1) {
            int32_t t = hi;
            hi = lo;
            lo = t;
        }
        arry.subs[i - 1] = (uint16_t)(hi - lo);
        arry.lbounds[i - 1] = (int16_t)lo;
    }
    Var *v = symb_var(id);
    if (!(n & 0x40)) {
        if (!(n & 0x80) || !v->vt) /* a STATIC's, dimensioned: kept */
            arry_dim(id, dims, false);
        return;
    }
    Addr old = v->vt; /* REDIM: the old one goes */
    if (old && n & 0x100) { /* PRESERVE: the new one made, the old one's elements copied */
        if (dims != ((ArrayHdr *)at(old))->dims)
            arry_bad_sub();
        v->vt = 0;
        arry_dim(id, dims, true);
        Addr fresh = symb_var(id)->vt;
        arry_keep(id, old, fresh);
        symb_var(id)->vt = old;
        uint32_t freed = arry_free(id, AREP);
        symb_var(id)->vt = fresh - freed; /* its header was after the old one's */
    } else {
        if (old)
            arry_free(id, AREP);
        arry_dim(id, dims, true);
    }
    arry_rep(id);
}

/* ERASE ( id -- ): REDIM's array freed, another's elements zeroed. */
void stmt_erase(void)
{
    uint16_t id = (uint16_t)exec_pop().u;
    Addr h = symb_var(id)->vt;
    if (!h)
        return;
    if (symb_varx(id)->dyn)
        arry_free(id, 0);
    else
        copy_zero(((ArrayHdr *)at(h))->data, arry_size(id, h));
}

/* ex_ready: back to READY, the frames kept. */
[[noreturn]] static void ready(void)
{
    evnt_clear();
    exec.stop &= ~SF_EV;
    repl_ready();
}

void stmt_rungo(uint32_t line)
{
    exec.sp = exec.rsp;
    evnt_clear();
    chai_start();
    exec.onerr = 0;
    exec.inerr = false;
    exec.err = 0;
    exec.erl = 0;
    exec.cstk = allo.cs_top;
    exec.lsp = exec.lsav;
    strg.tmp_floor = 0;
    flow_jump(line);
}

/* RUN.L ( line -- ). Undefined line number; a label. */
void stmt_runl(void)
{
    uint32_t num = exec_pop().u;
    if (num >= 0x10000)
        repl_error(E_US); /* RUN takes a number, not a label */
    symb_clear();
    link_start();
    exec.lnum = (uint16_t)num;
    uint32_t line;
    if (!prog_find((uint16_t)num, &line))
        repl_error(E_US);
    stmt_rungo(line);
}

/* RUN. */
void stmt_run(void)
{
    symb_clear();
    link_start();
    stmt_runfirst();
}

void stmt_runfirst(void)
{
    chnl_closeall();
    if (!prog_line(PROGBASE)->len)
        flow_end();
    stmt_rungo(PROGBASE);
}

/* LIST ( from to -- ), then READY. */
void stmt_list(void)
{
    uint16_t to = (uint16_t)exec_pop().u;
    uint16_t from = (uint16_t)exec_pop().u;
    list_range(from, to);
    ready();
}

void stmt_cont(void)
{
    uint32_t line = exec.cont_line;
    if (!line)
        repl_error(E_CN);
    if (!exec.cont_code)
        ready();
    exec.line = line;
    exec.ip = flow_place(line, exec.cont_code);
}

/* CLR: the variables, and the frames. */
void stmt_clr(void)
{
    symb_clear();
    exec.cstk = allo.cs_top;
}

/* NEW. */
void stmt_new(void)
{
    prog_init();
    flow_end();
}

/* NODIR: Illegal direct in the direct line. */
void stmt_nodir(void) { flow_nodir(); }

void stmt_end(void)
{
    if (dbug.probing)
        dbug_done();
    chnl_closeall();
    flow_setcont();
    ready();
}

void stmt_stop(void)
{
    exec.stop = 0;
    flow_setcont();
    repl_error(E_BRK);
}

/* POKE ( addr byte -- ). */
void stmt_poke(void)
{
    uint8_t b = (uint8_t)exec_pop().u;
    wr8(exec_popa(), b);
}

/* WAIT ( port mask xor -- ), WAIT2 ( port mask -- ): until (INP XOR xor) AND mask. */
static void wait(uint8_t x)
{
    uint8_t mask = (uint8_t)exec_pop().u;
    uint16_t port = (uint16_t)exec_pop().u;
    while (!((psys_inp(port) ^ x) & mask))
        ;
}
void stmt_wait(void) { wait((uint8_t)exec_pop().u); }
void stmt_wait2(void) { wait(0); }

/* INP ( port -- b ); OUT ( port b -- ). */
void stmt_inp(void) { exec_pushi(psys_inp((uint16_t)exec_pop().u)); }
void stmt_out(void)
{
    uint8_t b = (uint8_t)exec_pop().u;
    psys_outp((uint16_t)exec_pop().u, b);
}

/* SYS ( addr -- ): a call into the POKE memory; nothing poked there: a no-op. */
void stmt_sys(void)
{
    Addr a = allo.poke + (uint16_t)exec_pop().u;
    if (rd8(a))
        psys_call(a);
}

/* I.POW ( base power -- r ): truncated for a negative power. Overflow past an INT. */
void stmt_ipow(void)
{
    int32_t p = exec_popi();
    int32_t b = exec_popi();
    int32_t r = 1;
    if (p && b != 1) {
        if (!b)
            r = 0;
        else if (b == -1)
            r = p & 1 ? -1 : 1;
        else if (p < 0)
            r = 0; /* |base| over 1, a negative power */
        else
            for (; p; p--)
                if (__builtin_mul_overflow(r, b, &r))
                    repl_error(E_OV);
    }
    exec_pushi(r);
}

/* F.POW ( base power -- r ). */
void stmt_fpow(void)
{
    Sngl p = exec_pop().u;
    Sngl b = exec_pop().u;
    exec_pusha(fmth_pow(b, p));
}

/* F.LD.TI ( -- f ): the jiffies. */
void stmt_fti(void)
{
    uint32_t j = pclk_jiffies();
    exec_pusha(j ? sngl_norm(j, false) : 0);
}

/* S.LD.TI ( -- s ): "HHMMSS". */
void stmt_sti(void)
{
    uint32_t j = pclk_jiffies();
    for (int i = 6; i >= 0; i -= 2) {
        uint32_t base = i ? 60 : 100;
        uint32_t d = j % base;
        j /= base;
        cons.sbuf[i] = '0' + d / 10;
        cons.sbuf[i + 1] = '0' + d % 10;
    }
    exec_pusha(strg_buftmp(6));
}

/* I.LD.ST ( -- i ): the I/O status. */
void stmt_ist(void) { exec_pushi(cons.status); }
