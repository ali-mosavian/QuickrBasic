/* The executor: the dispatch loop, the STOP check, array, record and parameter access, the
 * string ops, CALL.
 *
 * # How it works
 * exec_run reads the op at vm.ip, keeps its place in vm.opip and calls its handler through a table
 * built from ops.def (exec.h). exec_until runs ops to an end address. exec_brk is the STOP check:
 * it counts ticks, runs an event, or raises BREAK.
 *
 * An array element is found by exec_elem1 (one subscript) or exec_elem (n subscripts), straight
 * from its header when it counts from 0 and the subscript is in range. A record is made, zeroed,
 * at its first use (exec_record). A parameter's cell is its argument's.
 *
 * K handlers (ops.def) are called by CALL through a second table.
 *
 * # Example
 *   A(3) = 7      pushes the value, then I_ST_A1 stores it at A's element 3
 *
 * # asm structure (src/vm/exec.asm)
 * | asm | here | |
 * |---|---|---|
 * | ex_lp | exec_run | MIRROR |
 * | ex_brk | exec_brk | MIRROR |
 * | arr_r1, arr_pn | exec_elem1, exec_elem | MIRROR |
 * | rec_get | exec_record | MIRROR |
 * | int_div | exec_idiv | MIRROR |
 * | g_ops, g_calls | ops, calls (tables from ops.def) | MIRROR |
 * | g_bflt | operands.c's tables | MOVED |
 * | g_fntab | funcs.c's fn1 table | MOVED |
 * | op_call | exec_call | MIRROR |
 * | op_sldt | exec_s_ld_txt | MIRROR |
 * | op_raddr, op_raddra, op_pref, op_vaddr, op_vld, op_vst | exec_r_addr, exec_ra_addr, exec_p_ref, exec_v_addr, exec_v_ld, exec_v_st | MIRROR |
 * | op_flld, op_flst, op_ichk16, op_sfld, op_sfst, op_rcopy | exec_fl_ld, exec_fl_st, exec_i_chk16, exec_sf_ld, exec_sf_st, exec_r_copy | MIRROR |
 * | op_ilda1, op_flda1, op_slda1, op_lda, op_slda | exec_i_ld_a1, exec_f_ld_a1, exec_s_ld_a1, exec_i_ld_a, exec_s_ld_a | MIRROR |
 * | op_sstv, op_ista1, op_fsta1, op_ssta1, op_istan, op_fstan, op_sstan | exec_s_st_v, exec_i_st_a1, exec_f_st_a1, exec_s_st_a1, exec_i_st_a, exec_f_st_a, exec_s_st_a | MIRROR |
 * | ex_cmpt | static | MIRROR |
 * | op_scmp, op_sif, op_scat | exec_s_cmp, exec_s_if, exec_s_cat | MIRROR |
 * | g_stkb, STKTOP | carved by exec_init; vm.stktop | D1 |
 *
 * - D1. exec_init carves the data stack from the arena; the asm's is a fixed buffer.
 */
#include "vm/exec.h"
#include "session.h"
#include "io/file.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/array.h"
#include "mem/copy.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "num/double.h"
#include "plat/pclk.h"
#include "plat/pkey.h"
#include "vm/debug.h"
#include "vm/event.h"
#include "vm/flow.h"
#include "vm/stmts.h"

#define OP(name, h, size, fl) void h(Session *S);
#include "vm/ops.def"
#define K(name, h) void h(Session *S);
#include "vm/ops.def"

typedef void Handler(Session *S);

static Handler *const ops[O_NOPS] = {
#define OP(name, h, size, fl) h,
#include "vm/ops.def"
};

static const uint8_t flags[O_NOPS] = {
#define OP(name, h, size, fl) fl,
#include "vm/ops.def"
};

static Handler *const calls[K_N] = {
#define K(name, h) h,
#include "vm/ops.def"
};

uint8_t exec_flags(const uint8_t op) { return flags[op]; }

void exec_until(Session *const S, const uint8_t *const end)
{
    Vm *const v = &S->vm;

    while (v->ip != end) {
        v->opip = v->ip;
        exec_counted(v);
        ops[*v->ip++](S);
    }
}

void exec_init(Session *const S)
{
    const Addr stack = allo_carve(&S->allo, STKSZ);
    S->vm.stktop = (Cell *)at(stack + STKSZ - 16);
    S->vm.sp = S->vm.rsp = S->vm.stktop;
    S->vm.stklim = S->vm.stktop - STKROOM / sizeof(Cell);
    S->vm.callvars = (CallVar *)at(allo_carve(&S->allo, LSAVSZ));
    pkey_init(&S->vm.stop, SF_TICK);
    pclk_init(&S->vm.stop, SF_TICK);
}

void exec_run(Session *const S)
{
    Vm *const v = &S->vm;
    setjmp(v->loop);
    v->running = true;

    for (;;) {
        v->opip = v->ip;
        exec_counted(v);
        ops[*v->ip++](S);
    }
}

bool exec_brk(Session *const S)
{
    if (S->vm.stop & SF_TICK)
        evnt_poll(S);
    if (!S->vm.stop)
        return false;
    if (!(S->vm.stop & SF_BRK))
        return evnt_run(S);

    pkey_flush(); /* the Ctrl+C, not a key for the editor */
    S->vm.ip = S->vm.opip;

    if (S->dbug.emit && S->vm.line != DLINE) { /* the IDE: on to the next statement's start */
        S->vm.stop = 0;
        S->dbug.mode = DM_STEP;
        return true;
    }

    stmt_stop(S);
}

int32_t exec_idiv(Fault *const f, const int32_t a, const int32_t b, int32_t *const rem)
{
    if (!b)
        fault_error(f, E_DZ);

    if (b == -1) {
        if (a == INT32_MIN)
            fault_error(f, E_OV);

        return -a; /* *rem kept: MOD does -1 itself */
    }

    *rem = a % b;
    return a / b;
}

/* ---- arrays and records --------------------------------------------------------------------- */

/* A subscript past a word is Subscript out of range. */
static uint16_t subscript(Session *const S, const int32_t sub)
{
    if (sub != (int16_t)sub)
        arry_bad_sub(S);

    return (uint16_t)sub;
}

Addr exec_elem1(Session *const S, const uint16_t id, const int32_t sub)
{
    S->arry.subs[0] = subscript(S, sub);
    return arry_ref(S, id, 1);
}

Addr exec_elem(Session *const S)
{
    const uint16_t id = exec_u16(&S->vm);
    const uint8_t n = exec_u8(&S->vm);

    for (uint8_t i = n; i-- > 0;)
        S->arry.subs[i] = subscript(S, exec_popi(&S->vm));

    return arry_ref(S, id, n);
}

/* Element sub of the 1-D array whose id is at vm->ip, size bytes each: straight from its header
   when it counts from 0 and sub is in range, else through arry_ref. */
static Addr elem1(Session *const S, const int32_t sub, const uint32_t size)
{
    const uint16_t id = exec_u16(&S->vm);
    const Addr hdr = symb_var(&S->symb, id)->vt;

    if (hdr) {
        const ArrayHdr *const a = (const ArrayHdr *)at(hdr);

        if (a->dims == 1 && (uint32_t)sub < a->fast)
            return a->data + (uint32_t)sub * size;
    }

    return exec_elem1(S, id, sub);
}

Addr exec_record(Session *const S, const uint16_t id)
{
    const Addr hdr = symb_var(&S->symb, id)->vt;

    if (hdr)
        return hdr;

    S->arry.counts[0] = 1;
    S->arry.lbounds[0] = 0;
    return arry_new(S, id, 1, false);
}

void exec_i_ld_a1(Session *const S) { exec_pusha(&S->vm, rd32(elem1(S, exec_popi(&S->vm), VS))); }
void exec_f_ld_a1(Session *const S) { exec_i_ld_a1(S); }
void exec_s_ld_a1(Session *const S) { exec_pusha(&S->vm, elem1(S, exec_popi(&S->vm), DSZ)); }
void exec_i_ld_a(Session *const S) { exec_pusha(&S->vm, rd32(exec_elem(S))); }
void exec_s_ld_a(Session *const S) { exec_pusha(&S->vm, exec_elem(S)); }

void exec_i_st_a1(Session *const S)
{
    const uint32_t x = exec_pop(&S->vm).u;
    wr32(elem1(S, exec_popi(&S->vm), VS), x);
}
void exec_f_st_a1(Session *const S) { exec_i_st_a1(S); }

void exec_s_st_a1(Session *const S)
{
    const Addr s = exec_popa(&S->vm);
    strg_set(&S->strg, s, elem1(S, exec_popi(&S->vm), DSZ));
}

void exec_i_st_a(Session *const S)
{
    const uint32_t x = exec_pop(&S->vm).u;
    wr32(exec_elem(S), x);
}
void exec_f_st_a(Session *const S) { exec_i_st_a(S); }

void exec_s_st_a(Session *const S)
{
    const Addr s = exec_popa(&S->vm);
    strg_set(&S->strg, s, exec_elem(S));
}

/* A record's address, the record made at its first use. */
void exec_r_addr(Session *const S)
{
    exec_pusha(&S->vm, ((const ArrayHdr *)at(exec_record(S, exec_u16(&S->vm))))->data);
}

/* A record array's element. */
void exec_ra_addr(Session *const S) { exec_pusha(&S->vm, exec_elem(S)); }

/* Record b's n bytes over record a's. */
void exec_r_copy(Session *const S)
{
    const Addr b = exec_popa(&S->vm);
    const Addr a = exec_popa(&S->vm);
    copy_mem(a, b, exec_u16(&S->vm));
}

/* ---- record fields -------------------------------------------------------------------------- */

static Cell cell_at(const Addr a) { return load_cell(at(a)); }

/* A record's number field: a SHORT, a DOUBLE, else 4 bytes. */
void exec_fl_ld(Session *const S)
{
    const Addr a = exec_popa(&S->vm) + exec_u16(&S->vm);
    const uint8_t type = exec_u8(&S->vm);

    if (type == ET_SH)
        exec_pushi(&S->vm, (int16_t)rd16(a));
    else if (type == ET_R8)
        exec_push(&S->vm, cell_at(a));
    else
        exec_pusha(&S->vm, rd32(a));
}

/* Overflow: a SHORT's past 16 bits. */
void exec_fl_st(Session *const S)
{
    const uint16_t off = exec_u16(&S->vm);
    const uint8_t type = exec_u8(&S->vm);
    const Cell x = exec_pop(&S->vm);
    const Addr a = exec_popa(&S->vm) + off;

    if (type == ET_R8)
        store_cell(at(a), x);
    else if (type == ET_SH) {
        if (x.i != (int16_t)x.i)
            repl_error(S, E_OV);

        wr16(a, (uint16_t)x.u);
    } else
        wr32(a, x.u);
}

/* A SHORT's value. Overflow past 16 bits. */
void exec_i_chk16(Session *const S)
{
    if (S->vm.sp->i != (int16_t)S->vm.sp->i)
        repl_error(S, E_OV);
}

/* A STRING * n field, as a temporary. */
void exec_sf_ld(Session *const S)
{
    const uint16_t off = exec_u16(&S->vm);
    const uint16_t n = exec_u16(&S->vm);
    const Addr chars = strg_alloc(&S->strg, n); /* the record does not move */
    const Addr a = exec_popa(&S->vm) + off;
    memcpy(at(chars), at(a), n);
    exec_pusha(&S->vm, strg_new(&S->strg, chars, n));
}

/* Into a STRING * n field: cut, or padded with spaces. */
void exec_sf_st(Session *const S)
{
    const Desc s = strg_get(&S->strg, exec_popa(&S->vm));
    const Addr a = exec_popa(&S->vm) + exec_u16(&S->vm);
    const uint16_t n = exec_u16(&S->vm);
    const uint16_t k = s.len < n ? s.len : n;
    memcpy(at(a), at(s.ptr), k);
    memset(at(a + k), ' ', n - k);
}

/* ---- parameters ----------------------------------------------------------------------------- */

/* An argument passed by reference: no value, the cell for PCALL. */
void exec_p_ref(Session *const S)
{
    const Addr a = exec_popa(&S->vm) + exec_u16(&S->vm);
    exec_pusha(&S->vm, 0);
    exec_pusha(&S->vm, a);
}

/* A scalar's cell; a parameter's is its argument's. */
void exec_v_addr(Session *const S) { exec_pusha(&S->vm, symb_var(&S->symb, exec_u16(&S->vm))->vt); }

typedef enum : uint8_t { CELL_NUMBER, CELL_DOUBLE, CELL_STRING } CellKind;

/* A parameter's value through its cell. */
void exec_v_ld(Session *const S)
{
    const Addr cell = symb_var(&S->symb, exec_u16(&S->vm))->vt;
    const CellKind kind = exec_u8(&S->vm);

    if (kind == CELL_DOUBLE)
        exec_push(&S->vm, cell_at(cell));
    else if (kind == CELL_STRING)
        exec_pusha(&S->vm, cell);
    else
        exec_pusha(&S->vm, rd32(cell));
}

void exec_v_st(Session *const S)
{
    const Addr cell = symb_var(&S->symb, exec_u16(&S->vm))->vt;
    const CellKind kind = exec_u8(&S->vm);
    const Cell x = exec_pop(&S->vm);

    if (kind == CELL_DOUBLE)
        store_cell(at(cell), x);
    else if (kind == CELL_STRING)
        strg_set(&S->strg, x.a, cell);
    else
        wr32(cell, x.u);
}

/* ---- strings -------------------------------------------------------------------------------- */

/* Text in the program: a temporary pointing at it. */
void exec_s_ld_txt(Session *const S)
{
    const uint8_t len = exec_u8(&S->vm);
    const Addr text = exec_u32(&S->vm);
    exec_pusha(&S->vm, strg_push(&S->strg, (Desc){.len = len, .ptr = text}));
}

void exec_s_st_v(Session *const S)
{
    const Addr cell = symb_cell(&S->symb, exec_u16(&S->vm));
    strg_set(&S->strg, exec_popa(&S->vm), cell);
}

/* A compare pushes -1 or 0; a CMPIF's false ends the line. */
void exec_s_cmp(Session *const S)
{
    const uint8_t op = *S->vm.opip;
    const Addr right = exec_popa(&S->vm);
    const Addr left = exec_popa(&S->vm);
    const bool yes = strg_cmp(&S->strg, left, right) & exec_u8(&S->vm);

    if (op < O_I_CMPIF)
        exec_pushi(&S->vm, yes ? -1 : 0);
    else if (!yes)
        flow_nextln(S);
}

/* IF on a string: empty ends the line. */
void exec_s_if(Session *const S)
{
    if (!strg_get(&S->strg, exec_popa(&S->vm)).len)
        flow_nextln(S);
}

/* String too long. */
void exec_s_cat(Session *const S)
{
    const Addr right = exec_popa(&S->vm);
    const Addr left = exec_popa(&S->vm);
    exec_pusha(&S->vm, strg_cat(&S->strg, left, right));
}

/* CALL k: routine k. */
void exec_call(Session *const S) { calls[exec_u8(&S->vm)](S); }
