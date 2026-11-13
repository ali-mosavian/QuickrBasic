/* The executor: the dispatch loop, the STOP check, array, record and parameter access, the string
   ops, CALL. */
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

#define OP(name, h, size, fl) void h(Vm *);
#include "vm/ops.def"
#define K(name, h) void h(Vm *);
#include "vm/ops.def"

typedef void Handler(Vm *);

static Handler *const ops[O_NOPS] = {
#define OP(name, h, size, fl) h,
#include "vm/ops.def"
};

static Handler *const calls[K_N] = {
#define K(name, h) h,
#include "vm/ops.def"
};

Vm vm;

void exec_init(void)
{
    const Addr stack = allo_carve(STKSZ);
    vm.stktop = (Cell *)at(stack + STKSZ - 16);
    vm.sp = vm.rsp = vm.stktop;
    vm.stklim = vm.stktop - STKROOM / sizeof(Cell);
    vm.callvars = (CallVar *)at(allo_carve(LSAVSZ));
    pkey_init(&vm.stop, SF_TICK);
    pclk_init(&vm.stop, SF_TICK);
}

void exec_run(void)
{
    Vm *const v = &vm;
    setjmp(v->loop);
    v->running = true;
    for (;;) {
        v->opip = v->ip;
        ops[*v->ip++](v);
    }
}

bool exec_brk(Vm *const vm)
{
    if (vm->stop & SF_TICK)
        evnt_poll(vm);
    if (!vm->stop)
        return false;
    if (!(vm->stop & SF_BRK))
        return evnt_run(vm);
    pkey_flush(); /* the Ctrl+C, not a key for the editor */
    vm->ip = vm->opip;
    if (dbug.emit && vm->line != DLINE) { /* the IDE: on to the next statement's start */
        vm->stop = 0;
        dbug.mode = DM_STEP;
        return true;
    }
    stmt_stop(vm);
}

int32_t exec_idiv(const int32_t a, const int32_t b, int32_t *const rem)
{
    if (!b)
        repl_error(E_DZ);
    if (b == -1) {
        if (a == INT32_MIN)
            repl_error(E_OV);
        return -a; /* *rem kept: MOD does -1 itself */
    }
    *rem = a % b;
    return a / b;
}

/* ---- arrays and records --------------------------------------------------------------------- */

/* A subscript past a word is Subscript out of range. */
static uint16_t subscript(const int32_t sub)
{
    if (sub != (int16_t)sub)
        arry_bad_sub();
    return (uint16_t)sub;
}

Addr exec_elem1(const uint16_t id, const int32_t sub)
{
    arry.subs[0] = subscript(sub);
    return arry_ref(id, 1);
}

Addr exec_elem(Vm *const vm)
{
    const uint16_t id = exec_u16(vm);
    const uint8_t n = exec_u8(vm);
    for (uint8_t i = n; i-- > 0;)
        arry.subs[i] = subscript(exec_popi(vm));
    return arry_ref(id, n);
}

/* Element sub of the 1-D array whose id is at vm->ip, size bytes each: straight from its header
   when it counts from 0 and sub is in range, else through arry_ref. */
static Addr elem1(Vm *const vm, const int32_t sub, const uint32_t size)
{
    const uint16_t id = exec_u16(vm);
    const Addr hdr = symb_var(id)->vt;
    if (hdr) {
        const ArrayHdr *const a = (const ArrayHdr *)at(hdr);
        if (a->dims == 1 && (uint32_t)sub < a->fast)
            return a->data + (uint32_t)sub * size;
    }
    return exec_elem1(id, sub);
}

Addr exec_record(const uint16_t id)
{
    const Addr hdr = symb_var(id)->vt;
    if (hdr)
        return hdr;
    arry.counts[0] = 1;
    arry.lbounds[0] = 0;
    return arry_new(id, 1, false);
}

void exec_i_ld_a1(Vm *const vm) { exec_pusha(vm, rd32(elem1(vm, exec_popi(vm), VS))); }
void exec_f_ld_a1(Vm *const vm) { exec_i_ld_a1(vm); }
void exec_s_ld_a1(Vm *const vm) { exec_pusha(vm, elem1(vm, exec_popi(vm), DSZ)); }
void exec_i_ld_a(Vm *const vm) { exec_pusha(vm, rd32(exec_elem(vm))); }
void exec_s_ld_a(Vm *const vm) { exec_pusha(vm, exec_elem(vm)); }

void exec_i_st_a1(Vm *const vm)
{
    const uint32_t x = exec_pop(vm).u;
    wr32(elem1(vm, exec_popi(vm), VS), x);
}
void exec_f_st_a1(Vm *const vm) { exec_i_st_a1(vm); }

void exec_s_st_a1(Vm *const vm)
{
    const Addr s = exec_popa(vm);
    strg_set(s, elem1(vm, exec_popi(vm), DSZ));
}

void exec_i_st_a(Vm *const vm)
{
    const uint32_t x = exec_pop(vm).u;
    wr32(exec_elem(vm), x);
}
void exec_f_st_a(Vm *const vm) { exec_i_st_a(vm); }

void exec_s_st_a(Vm *const vm)
{
    const Addr s = exec_popa(vm);
    strg_set(s, exec_elem(vm));
}

/* A record's address, the record made at its first use. */
void exec_r_addr(Vm *const vm)
{
    exec_pusha(vm, ((const ArrayHdr *)at(exec_record(exec_u16(vm))))->data);
}

/* A record array's element. */
void exec_ra_addr(Vm *const vm) { exec_pusha(vm, exec_elem(vm)); }

/* Record b's n bytes over record a's. */
void exec_r_copy(Vm *const vm)
{
    const Addr b = exec_popa(vm);
    const Addr a = exec_popa(vm);
    copy_mem(a, b, exec_u16(vm));
}

/* ---- record fields -------------------------------------------------------------------------- */

static Cell cell_at(const Addr a) { return load_cell(at(a)); }

/* A record's number field: a SHORT, a DOUBLE, else 4 bytes. */
void exec_fl_ld(Vm *const vm)
{
    const Addr a = exec_popa(vm) + exec_u16(vm);
    const uint8_t type = exec_u8(vm);
    if (type == ET_SH)
        exec_pushi(vm, (int16_t)rd16(a));
    else if (type == ET_R8)
        exec_push(vm, cell_at(a));
    else
        exec_pusha(vm, rd32(a));
}

/* Overflow: a SHORT's past 16 bits. */
void exec_fl_st(Vm *const vm)
{
    const uint16_t off = exec_u16(vm);
    const uint8_t type = exec_u8(vm);
    const Cell x = exec_pop(vm);
    const Addr a = exec_popa(vm) + off;
    if (type == ET_R8)
        store_cell(at(a), x);
    else if (type == ET_SH) {
        if (x.i != (int16_t)x.i)
            repl_error(E_OV);
        wr16(a, (uint16_t)x.u);
    } else
        wr32(a, x.u);
}

/* A SHORT's value. Overflow past 16 bits. */
void exec_i_chk16(Vm *const vm)
{
    if (vm->sp->i != (int16_t)vm->sp->i)
        repl_error(E_OV);
}

/* A STRING * n field, as a temporary. */
void exec_sf_ld(Vm *const vm)
{
    const uint16_t off = exec_u16(vm);
    const uint16_t n = exec_u16(vm);
    const Addr chars = strg_alloc(n); /* the record does not move */
    const Addr a = exec_popa(vm) + off;
    memcpy(at(chars), at(a), n);
    exec_pusha(vm, strg_new(chars, n));
}

/* Into a STRING * n field: cut, or padded with spaces. */
void exec_sf_st(Vm *const vm)
{
    const Desc s = strg_get(exec_popa(vm));
    const Addr a = exec_popa(vm) + exec_u16(vm);
    const uint16_t n = exec_u16(vm);
    const uint16_t k = s.len < n ? s.len : n;
    memcpy(at(a), at(s.ptr), k);
    memset(at(a + k), ' ', n - k);
}

/* ---- parameters ----------------------------------------------------------------------------- */

/* An argument passed by reference: no value, the cell for PCALL. */
void exec_p_ref(Vm *const vm)
{
    const Addr a = exec_popa(vm) + exec_u16(vm);
    exec_pusha(vm, 0);
    exec_pusha(vm, a);
}

/* A scalar's cell; a parameter's is its argument's. */
void exec_v_addr(Vm *const vm) { exec_pusha(vm, symb_var(exec_u16(vm))->vt); }

typedef enum : uint8_t { CELL_NUMBER, CELL_DOUBLE, CELL_STRING } CellKind;

/* A parameter's value through its cell. */
void exec_v_ld(Vm *const vm)
{
    const Addr cell = symb_var(exec_u16(vm))->vt;
    const CellKind kind = exec_u8(vm);
    if (kind == CELL_DOUBLE)
        exec_push(vm, cell_at(cell));
    else if (kind == CELL_STRING)
        exec_pusha(vm, cell);
    else
        exec_pusha(vm, rd32(cell));
}

void exec_v_st(Vm *const vm)
{
    const Addr cell = symb_var(exec_u16(vm))->vt;
    const CellKind kind = exec_u8(vm);
    const Cell x = exec_pop(vm);
    if (kind == CELL_DOUBLE)
        store_cell(at(cell), x);
    else if (kind == CELL_STRING)
        strg_set(x.a, cell);
    else
        wr32(cell, x.u);
}

/* ---- strings -------------------------------------------------------------------------------- */

/* Text in the program: a temporary pointing at it. */
void exec_s_ld_txt(Vm *const vm)
{
    const uint8_t len = exec_u8(vm);
    const Addr text = exec_u32(vm);
    exec_pusha(vm, strg_push((Desc){.len = len, .ptr = text}));
}

void exec_s_st_v(Vm *const vm)
{
    const Addr cell = symb_cell(exec_u16(vm));
    strg_set(exec_popa(vm), cell);
}

/* A compare pushes -1 or 0; a CMPIF's false ends the line. */
void exec_s_cmp(Vm *const vm)
{
    const uint8_t op = *vm->opip;
    const Addr right = exec_popa(vm);
    const Addr left = exec_popa(vm);
    const bool yes = strg_cmp(left, right) & exec_u8(vm);
    if (op < O_I_CMPIF)
        exec_pushi(vm, yes ? -1 : 0);
    else if (!yes)
        flow_nextln(vm);
}

/* IF on a string: empty ends the line. */
void exec_s_if(Vm *const vm)
{
    if (!strg_get(exec_popa(vm)).len)
        flow_nextln(vm);
}

/* String too long. */
void exec_s_cat(Vm *const vm)
{
    const Addr right = exec_popa(vm);
    const Addr left = exec_popa(vm);
    exec_pusha(vm, strg_cat(left, right));
}

/* CALL k: routine k. */
void exec_call(Vm *const vm) { calls[exec_u8(vm)](vm); }
