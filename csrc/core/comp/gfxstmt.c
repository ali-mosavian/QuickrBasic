/* The graphics statements compiled: SCREEN, PSET, PRESET, LINE, CIRCLE.
 *
 * # How it works
 * A statement's operands are pushed, then an O_SYSCALL of its service (vm/syscalls.def). A point,
 * [STEP] (x, y), is its two coordinates as DOUBLEs and its own service, before the statement's. An
 * argument left out is -1, the runtime's "none"; SCREEN's are each a value and whether it was given.
 *
 * LINE's B and BF are names to the lexer: their text tells them.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | gstm_stmt | NEW |
 */
#include "comp/gfxstmt.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "vm/exec.h"
#include "vm/syscall.h"

enum { SCREEN_ARGS = 4, NONE = -1 };

static void gstm_op(Comp *const C, const Syscall n) { comp_byte(comp_op(C, O_SYSCALL), n); }

static void gstm_double(Comp *const C)
{
    expr_compile(C);
    comp_todouble(C);
}

static void gstm_int(Comp *const C)
{
    expr_compile(C);
    comp_toint(C);
}

/* An argument there: not at its ',' or the statement's end. */
static bool gstm_given(Comp *const C) { return comp_cur(C) != ',' && !comp_end(C); }

/* An optional INT argument: its value, or NONE. */
static void gstm_optint(Comp *const C)
{
    if (gstm_given(C))
        gstm_int(C);
    else
        comp_int(C, (uint32_t)NONE);
}

/* [STEP] (x, y): the point, as absolute's kind or, STEP, step's. */
static void gstm_point(Comp *const C, const Syscall absolute, const Syscall step)
{
    const bool stepped = comp_cur(C) == TK_STEP;
    C->p += stepped;
    comp_open(C);
    gstm_double(C);
    comp_comma(C);
    gstm_double(C);
    comp_close(C);
    gstm_op(C, stepped ? step : absolute);
}

/* SCREEN [mode] [, [colorswitch] [, [apage] [, vpage]]]: each a value and whether given. */
static void gstm_screen(Comp *const C)
{
    for (uint32_t i = 0; i < SCREEN_ARGS; i++) {
        const bool given = (!i || comp_optcomma(C)) && gstm_given(C);

        if (given)
            gstm_int(C);
        else
            comp_int(C, 0);

        comp_int(C, given);
    }

    gstm_op(C, SC_SCREEN);
}

/* PSET or PRESET [STEP] (x, y) [, color]. */
static void gstm_pset(Comp *const C, const Syscall plain)
{
    gstm_point(C, SC_N1, SC_S1);

    if (!comp_optcomma(C)) {
        gstm_op(C, plain);
        return;
    }

    gstm_int(C);
    gstm_op(C, SC_PSETC);
}

/* LINE's B or BF at comp.p: 1, 2; 0 neither. */
static uint8_t gstm_box(Comp *const C)
{
    const uint8_t *const p = C->p;

    if (comp_cur(C) != M_VAR)
        return 0;

    const uint8_t n = p[1];
    const bool b = n && upcase(p[2]) == 'B';
    const uint8_t how = b && n == 1 ? 1 : b && n == 2 && upcase(p[3]) == 'F' ? 2 : 0;

    if (how)
        C->p = comp_skipname(p);

    return how;
}

/* LINE [[STEP] (x1, y1)] - [STEP] (x2, y2) [, [color] [, [B | BF] [, style]]]: without the first
   point, from the last. */
static void gstm_line(Comp *const C)
{
    if (comp_cur(C) == TK_MINUS)
        gstm_op(C, SC_LAST);
    else
        gstm_point(C, SC_N1, SC_S1);

    comp_need(C, TK_MINUS);
    gstm_point(C, SC_N2, SC_S2);
    uint8_t how = 0;

    if (comp_optcomma(C)) {
        gstm_optint(C);

        if (comp_optcomma(C)) {
            how = gstm_box(C);

            if (!how && gstm_given(C)) /* neither B nor BF */
                fault_error(C->fault, E_SN);

            comp_int(C, how);

            if (comp_optcomma(C))
                gstm_optint(C);
            else
                comp_int(C, (uint32_t)NONE);
        } else {
            comp_int(C, 0);
            comp_int(C, (uint32_t)NONE);
        }
    } else {
        comp_int(C, (uint32_t)NONE);
        comp_int(C, 0);
        comp_int(C, (uint32_t)NONE);
    }

    gstm_op(C, SC_LINE);
}

/* CIRCLE [STEP] (x, y), radius [, [color] [, [start] [, [end] [, aspect]]]]: the angles and the
   aspect, each given, set before it. */
static void gstm_circle(Comp *const C)
{
    static const Syscall sets[] = {SC_CSTART, SC_CEND, SC_CASPECT};

    gstm_point(C, SC_N1, SC_S1);
    comp_comma(C);
    gstm_double(C);

    if (comp_optcomma(C))
        gstm_optint(C);
    else
        comp_int(C, (uint32_t)NONE);

    for (uint32_t i = 0; i < countof(sets) && comp_optcomma(C); i++) {
        if (!gstm_given(C))
            continue;

        gstm_double(C);
        gstm_op(C, sets[i]);
    }

    gstm_op(C, SC_CIRCLE);
}

bool gstm_stmt(Comp *const C)
{
    const uint8_t t = C->p[1];

    if (t == TK1_LINE && C->p[2] == TK_INPUT)
        return false;

    switch (t) {
    case TK1_SCREEN:
        C->p += 2;
        gstm_screen(C);
        return true;

    case TK1_PSET:
    case TK1_PRESET:
        C->p += 2;
        gstm_pset(C, t == TK1_PSET ? SC_PSET : SC_PRESET);
        return true;

    case TK1_LINE:
        C->p += 2;
        gstm_line(C);
        return true;

    case TK1_CIRCLE:
        C->p += 2;
        gstm_circle(C);
        return true;
    }

    return false;
}
