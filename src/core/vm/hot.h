/* The ops exec_run keeps in registers: each written once, on the op's ip and sp alone.
 *
 * # How it works
 * A hot op reads its operands at r->ip and the data stack at r->sp, touches only the arena, and
 * returns its error as a Status instead of raising: exec_run inlines it with ip and sp in
 * registers, and stores them only around the rest. Its out-of-line handler (hot_op) runs it on
 * vm.ip and vm.sp, for exec_until and the ops table.
 */
#ifndef CORE_VM_HOT_H
#define CORE_VM_HOT_H
#include "session.h"
#include "num/fpmath.h"
#include "vm/operands.h"

typedef struct {
    uint8_t *ip;
    Cell *sp;
    Addr fp; /* the running procedure's frame (vm.fp) */
} Regs;

static inline uint8_t hot_u8(Regs *const r) { return *r->ip++; }

static inline uint32_t hot_u32(Regs *const r)
{
    const uint32_t v = load32(r->ip);
    r->ip += 4;
    return v;
}

/* A variable op's cell: its operand the cell, or a local's offset from the frame. */
static inline Addr hot_cell(Regs *const r, const bool local)
{
    const uint32_t c = hot_u32(r);
    return local ? r->fp + c : c;
}

/* a / b truncated, *q, *rem its remainder (b -1: kept). Division by zero; Overflow -2^31 / -1. */
static inline Status hot_idiv(
    const int32_t a,
    const int32_t b,
    int32_t *const q,
    int32_t *const rem
)
{
    if (!b)
        return E_DZ;

    if (b == -1) {
        if (a == INT32_MIN)
            return E_OV;

        *q = -a; /* *rem kept: MOD does -1 itself */
        return ST_OK;
    }

    *rem = a % b;
    *q = a / b;
    return ST_OK;
}

static inline Status hot_ld(Regs *const r, const bool local)
{
    const Addr cell = hot_cell(r, local);
    (--r->sp)->a = rd32(cell);
    return ST_OK;
}

static inline Status hot_st(Regs *const r, const bool local)
{
    const Addr cell = hot_cell(r, local);
    wr32(cell, r->sp++->u);
    return ST_OK;
}

static inline Status hot_i_ld_v(Regs *const r) { return hot_ld(r, false); }
static inline Status hot_i_st_v(Regs *const r) { return hot_st(r, false); }
static inline Status hot_i_ld_l(Regs *const r) { return hot_ld(r, true); }
static inline Status hot_i_st_l(Regs *const r) { return hot_st(r, true); }

static inline Status hot_i_ld_u8(Regs *const r)
{
    const uint8_t v = hot_u8(r);
    (--r->sp)->a = v;
    return ST_OK;
}

static inline Status hot_i_ld_i16(Regs *const r) /* unsigned */
{
    const uint16_t v = load16(r->ip);
    r->ip += 2;
    (--r->sp)->a = v;
    return ST_OK;
}

static inline Status hot_i_ld_i32(Regs *const r)
{
    const uint32_t v = hot_u32(r);
    (--r->sp)->a = v;
    return ST_OK;
}

/* Where an INT op's right operand comes from, what the op does. */
typedef enum { HOT_POPPED, HOT_VAR, HOT_LOC, HOT_U8, HOT_I16 } HotSrc;
typedef enum {
    HOT_ADD,
    HOT_SUB,
    HOT_MUL,
    HOT_DIV,
    HOT_AND,
    HOT_OR,
    HOT_STEP,
    HOT_MOD,
    HOT_XOR,
    HOT_EQV,
    HOT_IMP,
} HotOp;

/* An INT op's bytes, by its right operand. */
static inline uint32_t hot_size(const HotSrc src)
{
    return src == HOT_VAR || src == HOT_LOC ? 1 + VCELL
           : src == HOT_U8                  ? 2
           : src == HOT_I16                 ? 3
                                            : 1;
}

static inline int32_t hot_operand(Regs *const r, const HotSrc src)
{
    switch (src) {
    case HOT_POPPED:
        return r->sp++->i;

    case HOT_VAR:
    case HOT_LOC:
        return (int32_t)rd32(hot_cell(r, src == HOT_LOC));

    case HOT_U8:
        return hot_u8(r);

    case HOT_I16: { /* unsigned, as I.LD.I16's */
        const uint16_t v = load16(r->ip);
        r->ip += 2;
        return v;
    }
    }

    unreachable();
}

/* The top cell op the right operand. Overflow (a NEXT's step: Illegal function call); Division by
   zero. */
static inline Status hot_int(Regs *const r, const HotSrc src, const HotOp op)
{
    const int32_t b = hot_operand(r, src);
    Cell *const top = r->sp;
    int32_t v, rem;
    bool overflow = false;

    switch (op) {
    case HOT_ADD:
    case HOT_STEP:
        overflow = __builtin_add_overflow(top->i, b, &v);
        break;

    case HOT_SUB:
        overflow = __builtin_sub_overflow(top->i, b, &v);
        break;

    case HOT_MUL:
        overflow = __builtin_mul_overflow(top->i, b, &v);
        break;

    case HOT_DIV: {
        const Status st = hot_idiv(top->i, b, &v, &rem);

        if (st != ST_OK)
            return st;

        break;
    }

    case HOT_AND:
        v = top->i & b;
        break;

    case HOT_OR:
        v = top->i | b;
        break;

    case HOT_MOD: { /* the remainder takes the left operand's sign; by -1, 0 */
        rem = 0;

        if (b != -1) {
            const Status st = hot_idiv(top->i, b, &v, &rem);

            if (st != ST_OK)
                return st;
        }

        v = rem;
        break;
    }

    case HOT_XOR:
        v = top->i ^ b;
        break;

    case HOT_EQV:
        v = ~(top->i ^ b);
        break;

    case HOT_IMP:
        v = ~top->i | b;
        break;
    }

    if (overflow)
        return op == HOT_STEP ? E_IQ : E_OV;

    top->i = v;
    return ST_OK;
}

/* The INT ops: name, right operand, what it does. */
/* The INT operations in one mode, the right operand's: popped, a variable's, a local's, a byte or a
   word after the op. Mode by mode, as exec_run's labels have always been laid out: by operation,
   int_index ran 1% more instructions. */
#define HOT_INT_OPS(X, mode, src)                                                                  \
    X(i_add##mode, src, HOT_ADD)                                                                   \
    X(i_sub##mode, src, HOT_SUB)                                                                   \
    X(i_mul##mode, src, HOT_MUL)                                                                   \
    X(i_div##mode, src, HOT_DIV)                                                                   \
    X(i_and##mode, src, HOT_AND)                                                                   \
    X(i_or##mode, src, HOT_OR)
#define HOT_INTS(X)                                                                                \
    HOT_INT_OPS(X, , HOT_POPPED)                                                                   \
    HOT_INT_OPS(X, _v, HOT_VAR)                                                                    \
    HOT_INT_OPS(X, _u8, HOT_U8)                                                                    \
    X(i_step_v, HOT_VAR, HOT_STEP)                                                                 \
    X(i_mod, HOT_POPPED, HOT_MOD)                                                                  \
    X(i_xor, HOT_POPPED, HOT_XOR)                                                                  \
    X(i_mod_u8, HOT_U8, HOT_MOD)                                                                   \
    HOT_INT_OPS(X, _i16, HOT_I16)                                                                  \
    X(i_mod_i16, HOT_I16, HOT_MOD)                                                                 \
    HOT_INT_OPS(X, _l, HOT_LOC)                                                                    \
    X(i_step_l, HOT_LOC, HOT_STEP)

/* The INT ops behind X. */
#define HOT_XINTS(X)                                                                               \
    X(i_eqv, HOT_POPPED, HOT_EQV)                                                                  \
    X(i_imp, HOT_POPPED, HOT_IMP)

#define X(name, src, op)                                                                           \
    static inline Status hot_##name(Regs *const r) { return hot_int(r, src, op); }
HOT_INTS(X)
HOT_XINTS(X)
#undef X

/* The popped INT against the right operand: OPER_GT, OPER_EQ or OPER_LT. */
static inline uint8_t hot_rel(Regs *const r, const HotSrc src)
{
    const int32_t b = hot_operand(r, src);
    const int32_t a = r->sp++->i;
    return a > b ? OPER_GT : a == b ? OPER_EQ : OPER_LT;
}

/* A compare: the relation against the op's mask, -1 or 0 pushed. */
static inline Status hot_cmp(Regs *const r, const HotSrc src)
{
    const uint8_t got = hot_rel(r, src); /* its operand, then its mask */
    const bool yes = got & hot_u8(r);
    (--r->sp)->i = yes ? -1 : 0;
    return ST_OK;
}

static inline Status hot_i_cmp(Regs *const r) { return hot_cmp(r, HOT_POPPED); }
static inline Status hot_i_cmp_v(Regs *const r) { return hot_cmp(r, HOT_VAR); }
static inline Status hot_i_cmp_l(Regs *const r) { return hot_cmp(r, HOT_LOC); }
static inline Status hot_i_cmp_u8(Regs *const r) { return hot_cmp(r, HOT_U8); }
static inline Status hot_i_cmp_i16(Regs *const r) { return hot_cmp(r, HOT_I16); }

/* ---- SINGLE: an x87 operation each, its result rounded once as stored (num/single.c) ---- */

/* Where a SINGLE op's right operand comes from, what the op does. */
typedef enum { HF_POPPED, HF_VAR, HF_VARI, HF_LOC, HF_LOCI, HF_U8, HF_F32 } HotFsrc;
typedef enum { HF_ADD, HF_SUB, HF_MUL, HF_DIV } HotFop;

/* A SINGLE op's bytes, by its right operand. */
static inline uint32_t hot_fsize(const HotFsrc src)
{
    return src == HF_POPPED ? 1 : src == HF_U8 ? 2 : 5;
}

static inline Sngl hot_foperand(Regs *const r, const HotFsrc src)
{
    switch (src) {
    case HF_POPPED:
        return r->sp++->u;

    case HF_VAR:
    case HF_LOC:
        return rd32(hot_cell(r, src == HF_LOC));

    case HF_VARI: /* an INT variable */
    case HF_LOCI:
        return fmth_from_i32((int32_t)rd32(hot_cell(r, src == HF_LOCI)));

    case HF_U8:
        return fmth_from_i32(hot_u8(r));

    case HF_F32:
        return hot_u32(r);
    }

    unreachable();
}

/* The top cell op the right operand. Division by zero. */
static inline Status hot_flt(Regs *const r, const HotFsrc src, const HotFop op)
{
    const Sngl b = hot_foperand(r, src);
    Cell *const top = r->sp;

    switch (op) {
    case HF_ADD:
        top->u = fmth_add(top->u, b);
        break;

    case HF_SUB:
        top->u = fmth_sub(top->u, b);
        break;

    case HF_MUL:
        top->u = fmth_mul(top->u, b);
        break;

    case HF_DIV:
        if (!(b & ~SNGL_SIGN))
            return E_DZ;

        top->u = sngl_bits(sngl_val(top->u) / sngl_val(b));
        break;
    }

    return ST_OK;
}

/* The SINGLE operations in one mode: popped, a variable's, an INT variable's, a byte, a SINGLE after
   the op, a local's, an INT local's. */
#define HOT_FLT_OPS(X, mode, src)                                                                  \
    X(f_add##mode, src, HF_ADD)                                                                    \
    X(f_sub##mode, src, HF_SUB)                                                                    \
    X(f_mul##mode, src, HF_MUL)                                                                    \
    X(f_div##mode, src, HF_DIV)
/* Each SINGLE mode's row: ROW(X, mode, src). */
#define HOT_FLT_MODES(X, ROW)                                                                      \
    ROW(X, , HF_POPPED)                                                                            \
    ROW(X, _v, HF_VAR)                                                                             \
    ROW(X, _vi, HF_VARI)                                                                           \
    ROW(X, _u8, HF_U8)                                                                             \
    ROW(X, _f32, HF_F32)                                                                           \
    ROW(X, _l, HF_LOC)                                                                             \
    ROW(X, _li, HF_LOCI)
#define HOT_FCMP_OP(X, mode, src) X(f_cmp##mode, src)
#define HOT_FLTS(X) HOT_FLT_MODES(X, HOT_FLT_OPS)
#define HOT_FCMPS(X) HOT_FLT_MODES(X, HOT_FCMP_OP)

#define X(name, src, op)                                                                           \
    static inline Status hot_##name(Regs *const r) { return hot_flt(r, src, op); }
HOT_FLTS(X)
#undef X

/* The popped SINGLE against the right operand: OPER_GT, OPER_EQ or OPER_LT. */
static inline uint8_t hot_frel(Regs *const r, const HotFsrc src)
{
    const Sngl b = hot_foperand(r, src);
    const int c = sngl_cmp(r->sp++->u, b);
    return c > 0 ? OPER_GT : c == 0 ? OPER_EQ : OPER_LT;
}

#define X(name, src)                                                                               \
    static inline Status hot_##name(Regs *const r)                                                 \
    {                                                                                              \
        const uint8_t got = hot_frel(r, src);                                                      \
        const bool yes = got & hot_u8(r);                                                          \
        (--r->sp)->i = yes ? -1 : 0;                                                               \
        return ST_OK;                                                                              \
    }
HOT_FCMPS(X)
#undef X

/* The CMPIFs: name, handler, right operand, its relation. */
#define HOT_CMPIFS(X)                                                                              \
    X(i, oper_i_cmpif, HOT_POPPED, hot_rel)                                                        \
    X(i_v, oper_i_cmpif_v, HOT_VAR, hot_rel)                                                       \
    X(i_u8, oper_i_cmpif_u8, HOT_U8, hot_rel)                                                      \
    X(f, oper_f_cmpif, HF_POPPED, hot_frel)                                                        \
    X(f_v, oper_f_cmpif_v, HF_VAR, hot_frel)                                                       \
    X(f_vi, oper_f_cmpif_vi, HF_VARI, hot_frel)                                                    \
    X(f_u8, oper_f_cmpif_u8, HF_U8, hot_frel)                                                      \
    X(f_f32, oper_f_cmpif_f32, HF_F32, hot_frel)                                                   \
    X(i_l, oper_i_cmpif_l, HOT_LOC, hot_rel)                                                       \
    X(f_l, oper_f_cmpif_l, HF_LOC, hot_frel)                                                       \
    X(f_li, oper_f_cmpif_li, HF_LOCI, hot_frel)

/* ---- one cell's ops: loads, unary ops, conversions -------------------------------------------- */

static inline Status hot_f_ld_u8(Regs *const r)
{
    const Sngl v = hot_foperand(r, HF_U8);
    (--r->sp)->a = v;
    return ST_OK;
}

static inline Status hot_f_ld_vi(Regs *const r)
{
    const Sngl v = hot_foperand(r, HF_VARI);
    (--r->sp)->a = v;
    return ST_OK;
}

static inline Status hot_f_ld_li(Regs *const r)
{
    const Sngl v = hot_foperand(r, HF_LOCI);
    (--r->sp)->a = v;
    return ST_OK;
}

/* -0 stays 0. */
static inline Status hot_f_neg(Regs *const r)
{
    if (r->sp->u & ~SNGL_SIGN)
        r->sp->u ^= SNGL_SIGN;

    return ST_OK;
}

static inline Status hot_i_not(Regs *const r)
{
    r->sp->i = ~r->sp->i;
    return ST_OK;
}

static inline Status hot_drop(Regs *const r)
{
    r->sp++;
    return ST_OK;
}

static inline Status hot_i_tof(Regs *const r)
{
    r->sp->u = fmth_from_i32(r->sp->i);
    return ST_OK;
}

/* Overflow at -2^31. */
static inline Status hot_i_neg(Regs *const r)
{
    if (r->sp->i == INT32_MIN)
        return E_OV;

    r->sp->i = -r->sp->i;
    return ST_OK;
}

/* Illegal function call past a LONG. */
static inline Status hot_f_toi(Regs *const r)
{
    const int32_t v = fmth_rint32(r->sp->u);

    if (v == INT32_MIN && r->sp->u != SNGL_MIN_I32) /* -2^31 itself is one */
        return E_IQ;

    r->sp->a = (uint32_t)v;
    return ST_OK;
}

/* A byte, 0..255: Illegal function call. */
static inline Status hot_f_tob(Regs *const r)
{
    const int32_t v = fmth_rint32(r->sp->u);

    if (v != (int16_t)v || (uint32_t)v > 255)
        return E_IQ;

    r->sp->a = (uint32_t)v;
    return ST_OK;
}

/* An address, 0..65535. */
static inline Status hot_f_toa(Regs *const r)
{
    const int32_t v = fmth_rint32(r->sp->u);

    if ((uint32_t)v > UINT16_MAX)
        return E_IQ;

    r->sp->a = (uint32_t)v;
    return ST_OK;
}

static inline Status hot_i_tob(Regs *const r) { return r->sp->u > 255 ? E_IQ : ST_OK; }
static inline Status hot_i_toa(Regs *const r) { return r->sp->u > 65535 ? E_IQ : ST_OK; }
static inline Status hot_i_chk16(Regs *const r)
{
    return r->sp->i != (int16_t)r->sp->i ? E_OV : ST_OK;
}
static inline Status hot_i_chk8(Regs *const r) { return r->sp->u > 255 ? E_OV : ST_OK; }

/* The one-byte ops that may fail: name; the rest (HOT_OPS) cannot. */
#define HOT_FAILS(X)                                                                               \
    X(i_neg, oper_i_neg)                                                                           \
    X(f_toi, oper_f_toi)                                                                           \
    X(i_tob, oper_i_tob)                                                                           \
    X(i_toa, oper_i_toa)                                                                           \
    X(i_chk16, exec_i_chk16)                                                                       \
    X(i_chk8, exec_i_chk8)                                                                         \
    X(d_add, dubl_add)                                                                             \
    X(d_sub, dubl_sub)                                                                             \
    X(d_mul, dubl_mul)                                                                             \
    X(d_div, dubl_div)

/* I.NEXT.UP or .DN at op, its cells from base (0, or the frame its twin's are in): its counter plus
   its step stored; *back, not past its limit (above it up, below it down). Illegal function call
   past an INT's range, nothing stored. */
static inline Status hot_next(const uint8_t *const op, const Addr base, const bool up, bool *const back)
{
    const Addr cell = base + load32(op + 1);
    int32_t n;

    if (__builtin_add_overflow((int32_t)rd32(cell), (int32_t)rd32(base + load32(op + 1 + VCELL)), &n))
        return E_IQ;

    wr32(cell, (uint32_t)n);
    const int32_t lim = (int32_t)rd32(base + load32(op + 1 + 2 * VCELL));
    *back = up ? n <= lim : n >= lim;
    return ST_OK;
}

/* The INT compare-and-branches (comp/fuse.c): name, the compare's right operand. */
#define HOT_JFCS(X)                                                                                \
    X(i_jfc, HOT_POPPED)                                                                           \
    X(i_jfc_v, HOT_VAR)                                                                            \
    X(i_jfc_u8, HOT_U8)                                                                            \
    X(i_jfc_i16, HOT_I16)                                                                          \
    X(i_jfc_l, HOT_LOC)

/* A compare-and-branch at r->ip (past its op): r->ip at its Jump; its compare true, so not taken. */
static inline bool hot_jfc(Regs *const r, const HotSrc src)
{
    const uint8_t got = hot_rel(r, src);
    return got & hot_u8(r);
}

/* ---- DOUBLE (num/double.c): a cell's eight bytes ---- */

enum : uint64_t { HOT_DSIGN = 1ull << 63 };

/* A DOUBLE's high dword: its sign, exponent and top of its significand. */
static inline uint32_t hot_dhi(const Cell *const c) { return (uint32_t)(c->raw >> 32); }

/* +0 or -0, by the cell's bits. A result's tests are the x87's own: its bits would go through an
   8-aligned stack temporary, and the frame realigned for it takes the loop's seventh register. */
static inline bool hot_dzero(const Cell *const c) { return !(c->u | hot_dhi(c) << 1); }

static inline Status hot_dld(Regs *const r, const bool local)
{
    const Addr cell = hot_cell(r, local);
    *--r->sp = load_cell(at(cell));
    return ST_OK;
}

static inline Status hot_dst(Regs *const r, const bool local)
{
    const Addr cell = hot_cell(r, local);
    store_cell(at(cell), *r->sp++);
    return ST_OK;
}

static inline Status hot_d_ld_v(Regs *const r) { return hot_dld(r, false); }
static inline Status hot_d_st_v(Regs *const r) { return hot_dst(r, false); }
static inline Status hot_d_ld_l(Regs *const r) { return hot_dld(r, true); }
static inline Status hot_d_st_l(Regs *const r) { return hot_dst(r, true); }

static inline Status hot_d_ld_k(Regs *const r)
{
    *--r->sp = load_cell(r->ip);
    r->ip += sizeof(Cell);
    return ST_OK;
}

typedef enum { HD_ADD, HD_SUB, HD_MUL, HD_DIV } HotDop;

/* The top two DOUBLEs: the left op the right. Overflow; Division by zero, the divisor 0. */
static inline Status hot_dbl(Regs *const r, const HotDop op)
{
    if (op == HD_DIV && r->sp->d == 0) /* +0 and -0 */
        return E_DZ;

    const double b = r->sp++->d;
    Cell *const top = r->sp;

    switch (op) {
    case HD_ADD:
        top->d += b;
        break;

    case HD_SUB:
        top->d -= b;
        break;

    case HD_MUL:
        top->d *= b;
        break;

    case HD_DIV:
        top->d /= b;
        break;
    }

    return __builtin_fabs(top->d) <= __DBL_MAX__ ? ST_OK : E_OV; /* not NaN, not infinite */
}

static inline Status hot_d_add(Regs *const r) { return hot_dbl(r, HD_ADD); }
static inline Status hot_d_sub(Regs *const r) { return hot_dbl(r, HD_SUB); }
static inline Status hot_d_mul(Regs *const r) { return hot_dbl(r, HD_MUL); }
static inline Status hot_d_div(Regs *const r) { return hot_dbl(r, HD_DIV); }

/* -0 stays 0. */
static inline Status hot_d_neg(Regs *const r)
{
    if (!hot_dzero(r->sp))
        r->sp->raw ^= HOT_DSIGN;

    return ST_OK;
}

/* The relation of the top two against the op's mask: -1 or 0, the low dword's. */
static inline Status hot_d_cmp(Regs *const r)
{
    const double b = r->sp++->d;
    const double a = r->sp->d;
    const uint8_t rel = a > b ? OPER_GT : a < b ? OPER_LT : OPER_EQ;
    r->sp->u = rel & hot_u8(r) ? UINT32_MAX : 0;
    return ST_OK;
}

static inline Status hot_d_nz(Regs *const r)
{
    r->sp->u = hot_dzero(r->sp) ? 0 : UINT32_MAX;
    return ST_OK;
}

static inline Status hot_i_tod(Regs *const r)
{
    r->sp->d = r->sp->i;
    return ST_OK;
}

static inline Status hot_f_tod(Regs *const r)
{
    r->sp->d = r->sp->f;
    return ST_OK;
}

static inline Status hot_d_tof(Regs *const r)
{
    r->sp->u = sngl_bits((float)r->sp->d);
    return ST_OK;
}

/* ---- LONG, ULONG and UINT: a LONG the cell's 8 bytes, a UINT its low dword. Those behind X run
   off their handlers alone. ---- */

/* Where a LONG op's right operand comes from, what the op does: a LONG's, wrapped (W), a ULONG's
   (U). */
typedef enum { HL_POPPED, HL_VAR, HL_LOC, HL_U8 } HotLsrc;
typedef enum {
    HL_ADD,
    HL_SUB,
    HL_MUL,
    HL_DIV,
    HL_MOD,
    HL_AND,
    HL_OR,
    HL_XOR,
    HL_WADD,
    HL_WSUB,
    HL_WMUL,
    HL_UADD,
    HL_USUB,
    HL_UMUL,
    HL_UDIV,
    HL_UMOD,
} HotLop;

/* A LONG op's bytes, by its right operand. */
static inline uint32_t hot_lsize(const HotLsrc src)
{
    return src == HL_VAR || src == HL_LOC ? 1 + VCELL : src == HL_U8 ? 2 : 1;
}

static inline uint64_t hot_loperand(Regs *const r, const HotLsrc src)
{
    switch (src) {
    case HL_POPPED:
        return r->sp++->raw;

    case HL_VAR:
    case HL_LOC:
        return load_cell(at(hot_cell(r, src == HL_LOC))).raw;

    case HL_U8:
        return hot_u8(r);
    }

    unreachable();
}

/* The top cell op the right operand. Overflow; Division by zero. */
static inline Status hot_long(Regs *const r, const HotLsrc src, const HotLop op)
{
    const uint64_t b = hot_loperand(r, src);
    Cell *const top = r->sp;
    const uint64_t a = top->raw;
    const int64_t sa = (int64_t)a, sb = (int64_t)b;
    int64_t s = 0;
    uint64_t v = 0;
    bool over = false;

    switch (op) {
    case HL_ADD:
        over = __builtin_add_overflow(sa, sb, &s);
        v = (uint64_t)s;
        break;

    case HL_SUB:
        over = __builtin_sub_overflow(sa, sb, &s);
        v = (uint64_t)s;
        break;

    case HL_MUL:
        over = __builtin_mul_overflow(sa, sb, &s);
        v = (uint64_t)s;
        break;

    case HL_DIV:
    case HL_MOD: /* the remainder takes the left operand's sign; by -1, 0 */
        if (!b)
            return E_DZ;

        if (sb == -1) {
            over = op == HL_DIV && sa == INT64_MIN;
            v = op == HL_DIV ? 0 - a : 0;
        } else
            v = (uint64_t)(op == HL_DIV ? sa / sb : sa % sb);

        break;

    case HL_AND:
        v = a & b;
        break;

    case HL_OR:
        v = a | b;
        break;

    case HL_XOR:
        v = a ^ b;
        break;

    case HL_WADD:
        v = a + b;
        break;

    case HL_WSUB:
        v = a - b;
        break;

    case HL_WMUL:
        v = a * b;
        break;

    case HL_UADD:
        over = __builtin_add_overflow(a, b, &v);
        break;

    case HL_USUB:
        over = __builtin_sub_overflow(a, b, &v);
        break;

    case HL_UMUL:
        over = __builtin_mul_overflow(a, b, &v);
        break;

    case HL_UDIV:
    case HL_UMOD:
        if (!b)
            return E_DZ;

        v = op == HL_UDIV ? a / b : a % b;
        break;
    }

    if (over)
        return E_OV;

    top->raw = v;
    return ST_OK;
}

/* L.TO.V x k (exec.h LTO_*): the LONG at x op= the popped value. Overflow. */
static inline Status hot_lto(Regs *const r, const bool local)
{
    const Addr x = hot_cell(r, local);
    const uint8_t k = hot_u8(r);
    const Cell b = *r->sp++;
    const int64_t right = k & LTO_INT ? b.i : b.l, left = load_cell(at(x)).l;
    int64_t v;
    bool over;

    switch (k & ~LTO_INT) {
    case LTO_ADD:
        over = __builtin_add_overflow(left, right, &v);
        break;

    case LTO_SUB:
        over = __builtin_sub_overflow(left, right, &v);
        break;

    default:
        over = __builtin_mul_overflow(left, right, &v);
    }

    if (over)
        return E_OV;

    store_cell(at(x), (Cell){.l = v});

    return ST_OK;
}

static inline Status hot_l_to_v(Regs *const r) { return hot_lto(r, false); }
static inline Status hot_l_to_l(Regs *const r) { return hot_lto(r, true); }

/* The LONG ops of one byte: name, right operand, what it does. */
#define HOT_LONGS(X)                                                                               \
    X(l_add, HL_POPPED, HL_ADD)                                                                    \
    X(l_sub, HL_POPPED, HL_SUB)                                                                    \
    X(l_mul, HL_POPPED, HL_MUL)                                                                    \
    X(l_add_v, HL_VAR, HL_ADD)                                                                     \
    X(l_sub_v, HL_VAR, HL_SUB)                                                                     \
    X(l_mul_v, HL_VAR, HL_MUL)                                                                     \
    X(l_add_l, HL_LOC, HL_ADD)                                                                     \
    X(l_sub_l, HL_LOC, HL_SUB)                                                                     \
    X(l_mul_l, HL_LOC, HL_MUL)                                                                     \
    X(l_add_u8, HL_U8, HL_ADD)                                                                     \
    X(l_sub_u8, HL_U8, HL_SUB)

/* Behind X, the right operand popped: name, what it does. */
#define HOT_XLONGS(X)                                                                              \
    X(l_div, HL_DIV)                                                                               \
    X(l_mod, HL_MOD)                                                                               \
    X(l_and, HL_AND)                                                                               \
    X(l_or, HL_OR)                                                                                 \
    X(l_xor, HL_XOR)                                                                               \
    X(lw_add, HL_WADD)                                                                             \
    X(lw_sub, HL_WSUB)                                                                             \
    X(lw_mul, HL_WMUL)                                                                             \
    X(ul_add, HL_UADD)                                                                             \
    X(ul_sub, HL_USUB)                                                                             \
    X(ul_mul, HL_UMUL)                                                                             \
    X(ul_div, HL_UDIV)                                                                             \
    X(ul_mod, HL_UMOD)

#define X(name, src, op)                                                                           \
    static inline Status hot_##name(Regs *const r) { return hot_long(r, src, op); }
HOT_LONGS(X)
#undef X
#define X(name, op)                                                                                \
    static inline Status hot_##name(Regs *const r) { return hot_long(r, HL_POPPED, op); }
HOT_XLONGS(X)
#undef X

/* The popped LONG, or ULONG when uns, against the right operand: OPER_GT, OPER_EQ or OPER_LT. */
static inline uint8_t hot_lrel(Regs *const r, const HotLsrc src, const bool uns)
{
    const uint64_t b = hot_loperand(r, src);
    const uint64_t a = r->sp++->raw;
    const bool lt = uns ? a < b : (int64_t)a < (int64_t)b;
    return a == b ? OPER_EQ : lt ? OPER_LT : OPER_GT;
}

static inline Status hot_lcmp(Regs *const r, const bool uns)
{
    const uint8_t got = hot_lrel(r, HL_POPPED, uns);
    const bool yes = got & hot_u8(r);
    (--r->sp)->i = yes ? -1 : 0;
    return ST_OK;
}

static inline Status hot_l_cmp(Regs *const r) { return hot_lcmp(r, false); }
static inline Status hot_ul_cmp(Regs *const r) { return hot_lcmp(r, true); }

/* The LONG compare-and-branches (comp/fuse.c): name, the compare's right operand. */
#define HOT_LJFCS(X)                                                                               \
    X(l_jfc, HL_POPPED)                                                                            \
    X(l_jfc_v, HL_VAR)                                                                             \
    X(l_jfc_l, HL_LOC)

/* hot_jfc's of LONGs. */
static inline bool hot_ljfc(Regs *const r, const HotLsrc src)
{
    const uint8_t got = hot_lrel(r, src, false);
    return got & hot_u8(r);
}

/* Overflow at -2^63. */
static inline Status hot_l_neg(Regs *const r)
{
    if (r->sp->l == INT64_MIN)
        return E_OV;

    r->sp->l = -r->sp->l;
    return ST_OK;
}

static inline Status hot_lw_neg(Regs *const r)
{
    r->sp->raw = 0 - r->sp->raw;
    return ST_OK;
}

static inline Status hot_l_not(Regs *const r)
{
    r->sp->raw = ~r->sp->raw;
    return ST_OK;
}

/* A shift's count popped: past bits, Illegal function call. */
static inline Status hot_count(Regs *const r, const uint32_t bits, uint32_t *const n)
{
    *n = r->sp++->u;
    return *n < bits ? ST_OK : E_IQ;
}

static inline Status hot_l_shl(Regs *const r)
{
    uint32_t n;
    const Status st = hot_count(r, 64, &n);

    if (st == ST_OK)
        r->sp->raw <<= n;

    return st;
}

/* The sign shifted in. */
static inline Status hot_l_shr(Regs *const r)
{
    uint32_t n;
    const Status st = hot_count(r, 64, &n);
    const int64_t x = r->sp->l;

    if (st == ST_OK)
        r->sp->l = x < 0 ? ~(~x >> n) : x >> n;

    return st;
}

static inline Status hot_ul_shr(Regs *const r)
{
    uint32_t n;
    const Status st = hot_count(r, 64, &n);

    if (st == ST_OK)
        r->sp->raw >>= n;

    return st;
}

static inline Status hot_u_shr(Regs *const r)
{
    uint32_t n;
    const Status st = hot_count(r, 32, &n);

    if (st == ST_OK)
        r->sp->u >>= n;

    return st;
}

static inline Status hot_i_tol(Regs *const r)
{
    r->sp->l = r->sp->i;
    return ST_OK;
}

static inline Status hot_u_tol(Regs *const r)
{
    r->sp->raw = r->sp->u;
    return ST_OK;
}

/* Overflow past an INT. */
static inline Status hot_l_toi(Regs *const r)
{
    const int64_t x = r->sp->l;

    if (x != (int32_t)x)
        return E_OV;

    r->sp->raw = (uint32_t)x;
    return ST_OK;
}

static inline Status hot_l_tod(Regs *const r)
{
    r->sp->d = (double)r->sp->l;
    return ST_OK;
}

/* Toward 0; Overflow past a LONG, and NaN. */
static inline Status hot_d_tol(Regs *const r)
{
    const double d = r->sp->d, top = 9223372036854775808.0;

    if (!(d >= -top && d < top))
        return E_OV;

    r->sp->l = (int64_t)d;
    return ST_OK;
}

/* A UINT's op: checked, or wrapped (W) as an INT's bits. */
typedef enum { HU_ADD, HU_SUB, HU_MUL, HU_DIV, HU_MOD, HU_WADD, HU_WSUB, HU_WMUL } HotUop;

/* The top cell op the popped one. Overflow; Division by zero. */
static inline Status hot_uint(Regs *const r, const HotUop op)
{
    const uint32_t b = r->sp++->u;
    Cell *const top = r->sp;
    const uint32_t a = top->u;
    uint32_t v = 0;
    bool over = false;

    switch (op) {
    case HU_ADD:
        over = __builtin_add_overflow(a, b, &v);
        break;

    case HU_SUB:
        over = __builtin_sub_overflow(a, b, &v);
        break;

    case HU_MUL:
        over = __builtin_mul_overflow(a, b, &v);
        break;

    case HU_DIV:
    case HU_MOD:
        if (!b)
            return E_DZ;

        v = op == HU_DIV ? a / b : a % b;
        break;

    case HU_WADD:
        v = a + b;
        break;

    case HU_WSUB:
        v = a - b;
        break;

    case HU_WMUL:
        v = a * b;
        break;
    }

    if (over)
        return E_OV;

    top->u = v;
    return ST_OK;
}

/* The UINT and wrapped INT ops behind X: name, what it does. */
#define HOT_XUINTS(X)                                                                              \
    X(u_add, HU_ADD)                                                                               \
    X(u_sub, HU_SUB)                                                                               \
    X(u_mul, HU_MUL)                                                                               \
    X(u_div, HU_DIV)                                                                               \
    X(u_mod, HU_MOD)                                                                               \
    X(iw_add, HU_WADD)                                                                             \
    X(iw_sub, HU_WSUB)                                                                             \
    X(iw_mul, HU_WMUL)

#define X(name, op)                                                                                \
    static inline Status hot_##name(Regs *const r) { return hot_uint(r, op); }
HOT_XUINTS(X)
#undef X

static inline Status hot_u_cmp(Regs *const r)
{
    const uint32_t b = r->sp++->u;
    const uint32_t a = r->sp++->u;
    const uint8_t got = a == b ? OPER_EQ : a < b ? OPER_LT : OPER_GT;
    (--r->sp)->i = got & hot_u8(r) ? -1 : 0;
    return ST_OK;
}

static inline Status hot_iw_neg(Regs *const r)
{
    r->sp->u = 0 - r->sp->u;
    return ST_OK;
}

/* ---- fused stores (comp/fuse.c): x = a op b, x = k, x = a in one op ---- */

/* An INT op and its store: x's cell, a's, then op's own operand; on an error r->ip past it all. */
static inline Status hot_int_st(Regs *const r, const HotSrc src, const HotOp op)
{
    const Addr x = hot_u32(r);
    Cell top = {.a = rd32(hot_u32(r))};

    Regs t = {r->ip, &top, r->fp};

    const Status st = hot_int(&t, src, op);
    r->ip = t.ip;

    if (st != ST_OK)
        return st;

    wr32(x, top.u);
    return ST_OK;
}

/* The INT ops a store fuses with: the op's name in both cases, its right operand, what it does. */
#define HOT_INT_STS(X)                                                                             \
    X(I_ADD_V, i_add_v, HOT_VAR, HOT_ADD)                                                          \
    X(I_SUB_V, i_sub_v, HOT_VAR, HOT_SUB)                                                          \
    X(I_MUL_V, i_mul_v, HOT_VAR, HOT_MUL)                                                          \
    X(I_DIV_V, i_div_v, HOT_VAR, HOT_DIV)                                                          \
    X(I_AND_V, i_and_v, HOT_VAR, HOT_AND)                                                          \
    X(I_OR_V, i_or_v, HOT_VAR, HOT_OR)                                                             \
    X(I_ADD_U8, i_add_u8, HOT_U8, HOT_ADD)                                                         \
    X(I_SUB_U8, i_sub_u8, HOT_U8, HOT_SUB)                                                         \
    X(I_MUL_U8, i_mul_u8, HOT_U8, HOT_MUL)                                                         \
    X(I_DIV_U8, i_div_u8, HOT_U8, HOT_DIV)                                                         \
    X(I_AND_U8, i_and_u8, HOT_U8, HOT_AND)                                                         \
    X(I_OR_U8, i_or_u8, HOT_U8, HOT_OR)                                                            \
    X(I_MOD_U8, i_mod_u8, HOT_U8, HOT_MOD)                                                         \
    X(I_ADD_I16, i_add_i16, HOT_I16, HOT_ADD)                                                      \
    X(I_SUB_I16, i_sub_i16, HOT_I16, HOT_SUB)                                                      \
    X(I_MUL_I16, i_mul_i16, HOT_I16, HOT_MUL)                                                      \
    X(I_DIV_I16, i_div_i16, HOT_I16, HOT_DIV)                                                      \
    X(I_AND_I16, i_and_i16, HOT_I16, HOT_AND)                                                      \
    X(I_OR_I16, i_or_i16, HOT_I16, HOT_OR)                                                         \
    X(I_MOD_I16, i_mod_i16, HOT_I16, HOT_MOD)

#define X(NAME, name, src, op)                                                                     \
    static inline Status hot_##name##_st(Regs *const r) { return hot_int_st(r, src, op); }
HOT_INT_STS(X)
#undef X

static inline Status hot_i_set_u8(Regs *const r)
{
    const Addr x = hot_u32(r);
    wr32(x, hot_u8(r));
    return ST_OK;
}

static inline Status hot_i_set_i16(Regs *const r)
{
    const Addr x = hot_u32(r);
    wr32(x, load16(r->ip));
    r->ip += 2;
    return ST_OK;
}

static inline Status hot_i_set_i32(Regs *const r)
{
    const Addr x = hot_u32(r);
    wr32(x, hot_u32(r));
    return ST_OK;
}

static inline Status hot_i_mov(Regs *const r)
{
    const Addr x = hot_u32(r);
    wr32(x, rd32(hot_u32(r)));
    return ST_OK;
}

/* fn on vm.ip and vm.sp: its error raised. */
[[gnu::always_inline]]
static inline void hot_op(Session *const S, Status (*const fn)(Regs *r))
{
    Regs r = {S->vm.ip, S->vm.sp, S->vm.fp};

    const Status st = fn(&r);
    S->vm.ip = r.ip;
    S->vm.sp = r.sp;
    fault_check(&S->fault, st);
}
#endif
