/* The executor (src/vm/exec.asm): the dispatch loop, the STOP check, the array and record ops,
   the string ops, CALL. */
#include "vm/exec.h"
#include "io/file.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/arry.h"
#include "mem/copy.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/dubl.h"
#include "plat/pclk.h"
#include "plat/pkey.h"
#include "vm/dbug.h"
#include "vm/evnt.h"
#include "vm/flow.h"
#include "vm/stmt.h"

#define OP(name, h, size, fl) void h(void);
#include "vm/ops.def"
#define K(name, h) void h(void);
#include "vm/ops.def"

typedef void Handler(void);

static Handler *const ops[O_NOPS] = {
#define OP(name, h, size, fl) h,
#include "vm/ops.def"
};

static Handler *const calls[K_N] = {
#define K(name, h) h,
#include "vm/ops.def"
};

Exec exec;

void exec_init(void)
{
    Addr stk = allo_carve(STKSZ);
    exec.stktop = (Cell *)at(stk + STKSZ - 16);
    exec.sp = exec.rsp = exec.stktop;
    exec.stklim = exec.stktop - STKROOM / sizeof(Cell);
    exec.lsav = exec.lsp = allo_carve(LSAVSZ);
    exec.cstk = allo.cs_top;
    pkey_init(&exec.stop, SF_TICK);
    pclk_init(&exec.stop, SF_TICK);
}

void exec_run(void)
{
    setjmp(exec.loop);
    exec.running = true;
    for (;;) {
        exec.opip = exec.ip;
        ops[*exec.ip++]();
    }
}

bool exec_brk(void)
{
    if (exec.stop & SF_TICK)
        evnt_poll();
    if (!exec.stop)
        return false;
    if (!(exec.stop & SF_BRK))
        return evnt_run();
    pkey_flush(); /* the Ctrl+C, not a key for the editor */
    exec.ip = exec.opip;
    if (dbug.emit && exec.line != DLINE) { /* the IDE: on to the next statement's start */
        exec.stop = 0;
        dbug.mode = DM_STEP;
        return true;
    }
    stmt_stop();
}

/* sub as a subscript: Subscript out of range past a word. */
static uint16_t subscript(int32_t sub)
{
    if (sub != (int16_t)sub)
        arry_bad_sub();
    return (uint16_t)sub;
}

Addr exec_elem1(uint16_t id, int32_t sub)
{
    arry.subs[0] = subscript(sub);
    return arry_ref(id, 1);
}

Addr exec_elem(void)
{
    uint16_t id = exec_u16();
    uint8_t n = exec_u8();
    for (int i = n; i > 0; i--)
        arry.subs[i - 1] = subscript(exec_popi());
    return arry_ref(id, n);
}

/* a1get: element sub of the 1-D array whose id is at exec.ip, size bytes each; the header's
   count when it fits, else arry_ref. */
static Addr elem1(int32_t sub, uint32_t size)
{
    uint16_t id = exec_u16();
    Addr h = symb_var(id)->vt;
    const ArrayHdr *a = (const ArrayHdr *)at(h);
    if (h && a->dims == 1 && (uint32_t)sub < a->fast)
        return a->data + (uint32_t)sub * size;
    return exec_elem1(id, sub);
}

Addr exec_record(uint16_t id)
{
    Addr h = symb_var(id)->vt;
    if (!h) {
        arry.counts[0] = 1;
        arry.lbounds[0] = 0;
        h = arry_new(id, 1, false);
    }
    return h;
}

int32_t exec_idiv(int32_t a, int32_t b, int32_t *rem)
{
    if (!b)
        repl_error(E_DZ);
    if (b == -1) {
        if (a == INT32_MIN)
            repl_error(E_OV);
        return -a; /* *rem kept: I.MOD does -1 itself */
    }
    *rem = a % b;
    return a / b;
}

/* S.LD.TXT len ptr:4: a temporary for the text. */
void exec_s_ld_txt(void)
{
    uint8_t len = exec_u8();
    Addr p = exec_u32();
    exec_pusha(strg_push((Desc){.len = len, .ptr = p}));
}

/* R.ADDR id ( -- a ): the record's address, made at its first use. */
void exec_r_addr(void)
{
    exec_pusha(((ArrayHdr *)at(exec_record(exec_u16())))->data);
}

/* RA.ADDR id n ( i1..in -- a ): a record array's element. */
void exec_ra_addr(void) { exec_pusha(exec_elem()); }

/* P.REF off ( a -- 0 c ): an argument by reference: no value, the cell c for PCALL. */
void exec_p_ref(void)
{
    Addr a = exec_popa() + exec_u16();
    exec_pusha(0);
    exec_pusha(a);
}

/* V.ADDR id ( -- c ): scalar id's cell, a parameter's the argument's. */
void exec_v_addr(void) { exec_pusha(symb_var(exec_u16())->vt); }

static Cell cell_at(Addr a)
{
    Cell c;
    memcpy(&c, at(a), 8);
    return c;
}

/* V.LD id k ( -- x ): a parameter's value through its cell; k 0 an INT's or FLOAT's, 1 a
   DOUBLE's, 2 a string's Desc. */
void exec_v_ld(void)
{
    Addr c = symb_var(exec_u16())->vt;
    uint8_t k = exec_u8();
    if (k == 1)
        exec_push(cell_at(c));
    else if (k > 1)
        exec_pusha(c);
    else
        exec_pusha(rd32(c));
}

/* V.ST id k ( x -- ). */
void exec_v_st(void)
{
    Addr c = symb_var(exec_u16())->vt;
    uint8_t k = exec_u8();
    Cell x = exec_pop();
    if (k == 1)
        memcpy(at(c), &x, 8);
    else if (k > 1)
        strg_set(x.a, c);
    else
        wr32(c, x.u);
}

/* FL.LD off et ( a -- x ): a record's number field. */
void exec_fl_ld(void)
{
    Addr a = exec_popa() + exec_u16();
    uint8_t et = exec_u8();
    if (et == ET_SH)
        exec_pushi((int16_t)rd16(a));
    else if (et == ET_R8)
        exec_push(cell_at(a));
    else
        exec_pusha(rd32(a));
}

/* FL.ST off et ( a x -- ). Overflow: a SHORT's past 16 bits. */
void exec_fl_st(void)
{
    uint16_t off = exec_u16();
    uint8_t et = exec_u8();
    Cell x = exec_pop();
    Addr a = exec_popa() + off;
    if (et == ET_R8)
        memcpy(at(a), &x, 8);
    else if (et == ET_SH) {
        if (x.i != (int16_t)x.i)
            repl_error(E_OV);
        wr16(a, (uint16_t)x.u);
    } else
        wr32(a, x.u);
}

/* I.CHK16 ( x -- x ): a SHORT's value. Overflow past 16 bits. */
void exec_i_chk16(void)
{
    if (exec.sp->i != (int16_t)exec.sp->i)
        repl_error(E_OV);
}

/* SF.LD off n ( a -- s ): a STRING * n field, a temporary. */
void exec_sf_ld(void)
{
    uint16_t off = exec_u16();
    uint16_t n = exec_u16();
    Addr chars = strg_alloc(n); /* the record does not move */
    Addr a = exec_popa() + off;
    memcpy(at(chars), at(a), n);
    exec_pusha(strg_new(chars, n));
}

/* SF.ST off n ( a s -- ): cut or padded with spaces. */
void exec_sf_st(void)
{
    Desc s = strg_get(exec_popa());
    Addr a = exec_popa() + exec_u16();
    uint16_t n = exec_u16();
    uint16_t k = s.len < n ? s.len : n;
    memcpy(at(a), at(s.ptr), k);
    memset(at(a + k), ' ', n - k);
}

/* R.COPY n ( a b -- ): record b's n bytes over a's. */
void exec_r_copy(void)
{
    Addr b = exec_popa();
    Addr a = exec_popa();
    copy_mem(a, b, exec_u16());
}

/* X.LD.A1 id ( i -- x ). */
void exec_i_ld_a1(void) { exec_pusha(rd32(elem1(exec_popi(), VS))); }
void exec_f_ld_a1(void) { exec_pusha(rd32(elem1(exec_popi(), VS))); }
void exec_s_ld_a1(void) { exec_pusha(elem1(exec_popi(), DSZ)); }

/* X.LD.A id n ( i1..in -- x ). */
void exec_i_ld_a(void) { exec_pusha(rd32(exec_elem())); }
void exec_s_ld_a(void) { exec_pusha(exec_elem()); }

/* S.ST.V id ( s -- ). */
void exec_s_st_v(void)
{
    Addr cell = symb_cell(exec_u16());
    strg_set(exec_popa(), cell);
}

/* X.ST.A1 id ( i x -- ). */
void exec_i_st_a1(void)
{
    uint32_t x = exec_pop().u;
    wr32(elem1(exec_popi(), VS), x);
}
void exec_f_st_a1(void) { exec_i_st_a1(); }
void exec_s_st_a1(void)
{
    Addr x = exec_popa();
    strg_set(x, elem1(exec_popi(), DSZ));
}

/* X.ST.A id n ( i1..in x -- ). */
void exec_i_st_a(void)
{
    uint32_t x = exec_pop().u;
    wr32(exec_elem(), x);
}
void exec_f_st_a(void) { exec_i_st_a(); }
void exec_s_st_a(void)
{
    Addr x = exec_popa();
    strg_set(x, exec_elem());
}

/* S.CMP mask, S.CMPIF mask ( s t -- ): a compare pushes -1 or 0, a CMPIF's false ends the
   line. */
void exec_s_cmp(void)
{
    uint8_t op = *exec.opip;
    Addr right = exec_popa();
    Addr left = exec_popa();
    uint8_t r = strg_cmp(left, right) & exec_u8();
    if (op < O_I_CMPIF)
        exec_pushi(r ? -1 : 0);
    else if (!r)
        flow_nextln();
}

/* S.IF ( s -- ): empty ends the line. */
void exec_s_if(void)
{
    if (!strg_get(exec_popa()).len)
        flow_nextln();
}

/* S.CAT ( s t -- s+t ). String too long. */
void exec_s_cat(void)
{
    Addr right = exec_popa();
    Addr left = exec_popa();
    exec_pusha(strg_cat(left, right));
}

/* CALL k: routine k. */
void exec_call(void) { calls[exec_u8()](); }
