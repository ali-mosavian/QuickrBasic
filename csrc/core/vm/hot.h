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
#include "vm/operands.h"

typedef struct {
    uint8_t *ip;
    Cell *sp;
} Regs;

static inline uint8_t hot_u8(Regs *const r) { return *r->ip++; }

static inline uint32_t hot_u32(Regs *const r)
{
    const uint32_t v = load32(r->ip);
    r->ip += 4;
    return v;
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

static inline Status hot_i_ld_v(Regs *const r)
{
    const Addr cell = hot_u32(r);
    (--r->sp)->a = rd32(cell);
    return ST_OK;
}

static inline Status hot_i_st_v(Regs *const r)
{
    const Addr cell = hot_u32(r);
    wr32(cell, r->sp++->u);
    return ST_OK;
}

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
typedef enum { HOT_POPPED, HOT_VAR, HOT_U8 } HotSrc;
typedef enum { HOT_ADD, HOT_SUB, HOT_MUL, HOT_DIV, HOT_AND, HOT_OR, HOT_STEP } HotOp;

/* An INT op's bytes, by its right operand. */
static inline uint32_t hot_size(const HotSrc src)
{
    return src == HOT_VAR ? 1 + VCELL : src == HOT_U8 ? 2 : 1;
}

static inline int32_t hot_operand(Regs *const r, const HotSrc src)
{
    switch (src) {
    case HOT_POPPED:
        return r->sp++->i;

    case HOT_VAR:
        return (int32_t)rd32(hot_u32(r));

    case HOT_U8:
        return hot_u8(r);
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
    }

    if (overflow)
        return op == HOT_STEP ? E_IQ : E_OV;

    top->i = v;
    return ST_OK;
}

/* The INT ops: name, right operand, what it does. */
#define HOT_INTS(X)                                                                                \
    X(i_add, HOT_POPPED, HOT_ADD)                                                                  \
    X(i_sub, HOT_POPPED, HOT_SUB)                                                                  \
    X(i_mul, HOT_POPPED, HOT_MUL)                                                                  \
    X(i_div, HOT_POPPED, HOT_DIV)                                                                  \
    X(i_and, HOT_POPPED, HOT_AND)                                                                  \
    X(i_or, HOT_POPPED, HOT_OR)                                                                    \
    X(i_add_v, HOT_VAR, HOT_ADD)                                                                   \
    X(i_sub_v, HOT_VAR, HOT_SUB)                                                                   \
    X(i_mul_v, HOT_VAR, HOT_MUL)                                                                   \
    X(i_div_v, HOT_VAR, HOT_DIV)                                                                   \
    X(i_and_v, HOT_VAR, HOT_AND)                                                                   \
    X(i_or_v, HOT_VAR, HOT_OR)                                                                     \
    X(i_add_u8, HOT_U8, HOT_ADD)                                                                   \
    X(i_sub_u8, HOT_U8, HOT_SUB)                                                                   \
    X(i_mul_u8, HOT_U8, HOT_MUL)                                                                   \
    X(i_div_u8, HOT_U8, HOT_DIV)                                                                   \
    X(i_and_u8, HOT_U8, HOT_AND)                                                                   \
    X(i_or_u8, HOT_U8, HOT_OR)                                                                     \
    X(i_step_v, HOT_VAR, HOT_STEP)

#define X(name, src, op)                                                                           \
    static inline Status hot_##name(Regs *const r) { return hot_int(r, src, op); }
HOT_INTS(X)
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
    const bool yes = hot_rel(r, src) & hot_u8(r);
    (--r->sp)->i = yes ? -1 : 0;
    return ST_OK;
}

static inline Status hot_i_cmp(Regs *const r) { return hot_cmp(r, HOT_POPPED); }
static inline Status hot_i_cmp_v(Regs *const r) { return hot_cmp(r, HOT_VAR); }
static inline Status hot_i_cmp_u8(Regs *const r) { return hot_cmp(r, HOT_U8); }

/* I.NEXT at op: its counter plus its step stored; *back, not past its limit by its mask. Illegal
   function call past an INT's range, nothing stored. */
static inline Status hot_next(const uint8_t *const op, bool *const back)
{
    const Addr cell = load32(op + 1);
    int32_t n;

    if (__builtin_add_overflow((int32_t)rd32(cell), (int32_t)rd32(load32(op + 5)), &n))
        return E_IQ;

    wr32(cell, (uint32_t)n);
    const int32_t lim = (int32_t)rd32(load32(op + 9));
    *back = !((n > lim ? OPER_GT : n == lim ? OPER_EQ : OPER_LT) & op[13]);
    return ST_OK;
}

/* fn on vm.ip and vm.sp: its error raised. */
[[gnu::always_inline]]
static inline void hot_op(Session *const S, Status (*const fn)(Regs *r))
{
    Regs r = {S->vm.ip, S->vm.sp};

    const Status st = fn(&r);
    S->vm.ip = r.ip;
    S->vm.sp = r.sp;
    fault_check(&S->fault, st);
}
#endif
