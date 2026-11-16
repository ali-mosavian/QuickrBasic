/* The simple ops: loads, stores, INT and SINGLE operators, compares, IF, conversions, DROP.
 *
 * # How it works
 * An INT op works on the top cell's low dword. Each is vm/hot.h's, which exec_run runs inline; a
 * handler here runs it on vm.ip and vm.sp, for exec_until and the ops table.
 *
 * A compare op's operand is its mask of the relations it is true for (OPER_GT, OPER_EQ, OPER_LT).
 * It pushes -1 or 0. A CMPIF's false, like a false IF, ends the line (flow_nextln).
 *
 * # Example
 *   x% + 5     I_LD_V x%, then I_ADD_U8 5
 *
 * # asm structure (src/vm/bc.asm)
 * | asm | here | |
 * |---|---|---|
 * | bc_X, every op handler | oper_x, the op's name in lower case (bc_I_ADD_V is oper_i_add_v) | MIRROR; D1 |
 * | bvar, fcall, isrc, fsrc, ibin, fbin, ctail, icmp, fcmp | exec_cell, operand, int_op, sngl_op, int_cmp, sngl_cmp_op, compared | MIRROR; D2 |
 * | g_bflt, compare mask bits 1 2 4 | tables here; OPER_GT, OPER_EQ, OPER_LT | MIRROR |
 * | g_relbit (calls.asm) | compared | MOVED |
 *
 * - D1. bc_F_LD_V is oper_i_ld_v: an INT and a SINGLE load alike.
 * - D2. A variable op holds its cell, set at link (link_resolve), not the variable's id.
 */
#include "vm/operands.h"
#include "session.h"
#include "io/repl.h"
#include "num/fpmath.h"
#include "vm/flow.h"
#include "vm/hot.h"

/* ---- loads and stores ----------------------------------------------------------------------- */

void oper_i_ld_v(Session *const S) { hot_op(S, hot_i_ld_v); }
void oper_i_ld_i16(Session *const S) { hot_op(S, hot_i_ld_i16); }
void oper_i_ld_i32(Session *const S) { hot_op(S, hot_i_ld_i32); }
void oper_i_ld_u8(Session *const S) { hot_op(S, hot_i_ld_u8); }
void oper_f_ld_f32(Session *const S) { hot_op(S, hot_i_ld_i32); }
void oper_f_ld_u8(Session *const S) { hot_op(S, hot_f_ld_u8); }
void oper_f_ld_vi(Session *const S) { hot_op(S, hot_f_ld_vi); }
void oper_f_ld_pi(Session *const S) { hot_op(S, hot_f_ld_pi); }
/* A string variable: its descriptor. */
void oper_s_ld_v(Session *const S) { exec_pusha(&S->vm, exec_cell(&S->vm)); }

void oper_i_st_v(Session *const S) { hot_op(S, hot_i_st_v); }
void oper_f_st_v(Session *const S) { hot_op(S, hot_i_st_v); }

/* ---- INT ------------------------------------------------------------------------------------ */

#define X(name, src, op)                                                                           \
    void oper_##name(Session *const S) { hot_op(S, hot_##name); }
HOT_INTS(X)
#undef X

void oper_i_neg(Session *const S) { hot_op(S, hot_i_neg); }
void oper_i_not(Session *const S) { hot_op(S, hot_i_not); }

#define X(NAME, name, src, op)                                                                     \
    void oper_##name##_st(Session *const S) { hot_op(S, hot_##name##_st); }
HOT_INT_STS(X)
#undef X

void oper_i_set_u8(Session *const S) { hot_op(S, hot_i_set_u8); }
void oper_i_set_i16(Session *const S) { hot_op(S, hot_i_set_i16); }
void oper_i_set_i32(Session *const S) { hot_op(S, hot_i_set_i32); }
void oper_i_mov(Session *const S) { hot_op(S, hot_i_mov); }

/* ---- SINGLE --------------------------------------------------------------------------------- */

#define X(name, src, op)                                                                           \
    void oper_##name(Session *const S) { hot_op(S, hot_##name); }
HOT_FLTS(X)
#undef X

void oper_f_neg(Session *const S) { hot_op(S, hot_f_neg); }

/* ---- compares ------------------------------------------------------------------------------- */

void oper_i_cmp(Session *const S) { hot_op(S, hot_i_cmp); }
void oper_i_cmp_v(Session *const S) { hot_op(S, hot_i_cmp_v); }
void oper_i_cmp_u8(Session *const S) { hot_op(S, hot_i_cmp_u8); }
void oper_i_cmp_i16(Session *const S) { hot_op(S, hot_i_cmp_i16); }
#define X(name, src)                                                                               \
    void oper_##name(Session *const S) { hot_op(S, hot_##name); }
HOT_FCMPS(X)
#undef X

/* A CMPIF: the relation against the op's mask; false ends the line. */
#define X(name, handler, src, rel)                                                                 \
    void handler(Session *const S)                                                                 \
    {                                                                                              \
        Regs r = {S->vm.ip, S->vm.sp};                                                             \
        const uint8_t got = rel(&r, src);                                                          \
        const bool yes = got & hot_u8(&r);                                                         \
        S->vm.ip = r.ip;                                                                           \
        S->vm.sp = r.sp;                                                                           \
                                                                                                   \
        if (!yes)                                                                                  \
            flow_nextln(S);                                                                        \
    }
HOT_CMPIFS(X)
#undef X

/* IF: false ends the line. A SINGLE is false at +0 and -0. */
void oper_i_if(Session *const S)
{
    if (!exec_popi(&S->vm))
        flow_nextln(S);
}

void oper_f_if(Session *const S)
{
    if (!(exec_pop(&S->vm).u & ~SNGL_SIGN))
        flow_nextln(S);
}

/* ---- conversions ---------------------------------------------------------------------------- */

void oper_i_tof(Session *const S) { hot_op(S, hot_i_tof); }
void oper_f_toi(Session *const S) { hot_op(S, hot_f_toi); }
void oper_f_tob(Session *const S) { hot_op(S, hot_f_tob); }
void oper_f_toa(Session *const S) { hot_op(S, hot_f_toa); }
void oper_i_tob(Session *const S) { hot_op(S, hot_i_tob); }
void oper_i_toa(Session *const S) { hot_op(S, hot_i_toa); }

void oper_drop(Session *const S) { hot_op(S, hot_drop); }
