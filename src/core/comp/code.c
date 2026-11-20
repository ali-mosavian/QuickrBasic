/* Writes code: bytes and ops, and conversions of the value the code so far leaves.
 *
 * # How it works
 * A Code holds the buffer, where the next byte goes and what the code so far ends with: its value's
 * type, its last INT load, its last compare, its SHORT class. The emitters write a checked tree's
 * code and raise nothing: past lim they drop bytes and set full, for the caller.
 *
 * A conversion (code_toint, code_tofloat, ...) looks at the value's type and appends the op that
 * changes it; an INT load just made is rewritten in place where it can be (code_tofloat).
 *
 * # asm structure (retired/asm32/comp/comp.asm, expr.asm)
 * | asm | here | |
 * |---|---|---|
 * | cm_byt, cm_op | code_byte, code_op | MIRROR; D1 |
 * | cm_opx, cm_opw, cm_dw, cm_ins, cm_lint, cm_call | code_opb, code_opw, code_dword (code_word), code_insert, code_int, code_call | MIRROR; D1 |
 * | cm_opid, cm_vop | code_opid, parop | MIRROR |
 * | cm_ileaf | code_intleaf | MIRROR |
 * | cm_shv | code_shclass | MIRROR |
 * | cm_toi, cm_tof, cm_tod, cm_tob, cm_toa, cm_dnz | code_toint, code_tofloat, code_todouble, code_tobyte, code_toaddr, code_dtest | MIRROR |
 * | cm_tos, cm_tgt, cm_store, cm_fst | comp_tostring, comp_target, comp_store (comp.c) | MOVED |
 * | cm_shid, cm_sop | shortid, shortop (codegen.c) | MOVED |
 * | g_csh, g_cshe, g_cshc, g_xdbl | Code.sh, Code.shend, Code.shclass, Code.fixdbl | D2 |
 * | cty, cll, clle, ccmp, ccmpe, cxb, cxlim, (EDI) | Code.type, iload, iloadend, cmp, cmpend, buf, lim, out | D2 |
 *
 * - D1. A byte past lim is dropped and sets Code.full, not an Out of memory error.
 * - D2. The state is a Code, not module globals, so the generator runs on any buffer.
 */
#include "comp/code.h"
#include <string.h>
#include "comp/ast.h"
#include "mem/symbols.h"
#include "num/fpmath.h"
#include "vm/exec.h"

void code_opb(Code *const c, const uint8_t op, const uint8_t b)
{
    code_op(c, op);
    code_byte(c, b);
}

void code_word(Code *const c, const uint16_t w)
{
    if (c->lim - c->out >= 2) {
        store16(c->out, w);
        c->out += 2;
        return;
    }

    code_byte(c, (uint8_t)w);
    code_byte(c, (uint8_t)(w >> 8));
}

void code_opw(Code *const c, const uint8_t op, const uint16_t w)
{
    code_op(c, op);
    code_word(c, w);
}

void code_dword(Code *const c, const uint32_t d)
{
    if (c->lim - c->out >= 4) {
        store32(c->out, d);
        c->out += 4;
        return;
    }

    code_word(c, (uint16_t)d);
    code_word(c, (uint16_t)(d >> 16));
}

/* A mark into the code, at or past where: moved up a byte with it. Null is below any. */
static void follow(uint8_t **const start, uint8_t **const end, const uint8_t *const where)
{
    if ((uintptr_t)*start >= (uintptr_t)where) {
        ++*start;
        ++*end;
    }
}

void code_insert(Code *const c, const uint8_t b, uint8_t *const where)
{
    if (c->out >= c->lim) {
        c->full = true;
        return;
    }

    memmove(where + 1, where, (size_t)(c->out - where));
    c->out++;
    *where = b;
    follow(&c->iload, &c->iloadend, where);
    follow(&c->cmp, &c->cmpend, where);
}

void code_int(Code *const c, const uint32_t n)
{
    c->type = T_INT;
    c->iload = c->out;

    if (n > 0xFFFF) {
        code_op(c, O_I_LD_I32);
        code_dword(c, n);
    } else if (n > 0xFF)
        code_opw(c, O_I_LD_I16, (uint16_t)n);
    else
        code_opb(c, O_I_LD_U8, (uint8_t)n);

    c->iloadend = c->out;
}

void code_call(Code *const c, const uint8_t k) { code_opb(c, O_CALL, k); }

/* cm_vop: X.LD.V or X.ST.V of a scalar parameter: V.LD, V.ST id kind (kind 0 4 bytes, 1 8, 2 a
   Desc); false: op not one of them. */
static bool parop(Code *const c, const uint8_t op, const uint32_t id)
{
    uint8_t vop, kind;

    switch (op) {
    case O_I_LD_V:
    case O_F_LD_V:
        vop = O_V_LD;
        kind = 0;
        break;

    case O_D_LD_V:
        vop = O_V_LD;
        kind = 1;
        break;

    case O_S_LD_V:
        vop = O_V_LD;
        kind = 2;
        break;

    case O_I_ST_V:
    case O_F_ST_V:
        vop = O_V_ST;
        kind = 0;
        break;

    case O_D_ST_V:
        vop = O_V_ST;
        kind = 1;
        break;

    case O_S_ST_V:
        vop = O_V_ST;
        kind = 2;
        break;

    default:
        return false;
    }

    code_op(c, vop);
    code_dword(c, id);
    code_byte(c, kind);
    return true;
}

void code_opid(Code *const c, const uint8_t op, const uint32_t id, const bool par)
{
    if (par && parop(c, op, id))
        return;

    code_op(c, op);

    code_dword(c, id); /* FV: the cell's VCELL, the id till linked */

    if (exec_flags(op) & FA) /* the array's cell, link's */
        code_dword(c, 0);
}

uint8_t *code_intleaf(Code *const c)
{
    if (c->out != c->iloadend)
        return nullptr;

    const uint8_t op = *c->iload;
    return op == O_I_LD_V || op == O_I_LD_U8 || op == O_I_LD_I32 || op == O_I_LD_I16 ? c->iload
                                                                                     : nullptr;
}

uint8_t code_shclass(Code *const c)
{
    c->shclass = c->out == c->shend ? c->sh : SH_NONE;
    return c->shclass;
}

void code_toint(Code *const c)
{
    if (c->type == T_INT)
        return;

    const uint8_t op = c->type == T_DBL ? O_D_TOI : O_F_TOI;
    c->type = T_INT;
    code_op(c, op);
}

void code_todouble(Code *const c)
{
    uint8_t op;

    if (c->type == T_INT)
        op = O_I_TOD;
    else if (c->type == T_FLT)
        op = O_F_TOD;
    else
        return;

    c->type = T_DBL;
    code_op(c, op);
}

void code_dtest(Code *const c)
{
    if (c->type != T_DBL)
        return;

    c->type = T_INT;
    code_op(c, O_D_NZ);
}

/* A byte or an address: a U8 load needs nothing. */
static void tonarrow(Code *const c, const uint8_t fop, const uint8_t iop)
{
    if (c->type == T_DBL)
        code_toint(c);

    const bool is_int = c->type == T_INT;

    if (is_int) {
        const uint8_t *const leaf = code_intleaf(c);

        if (leaf && *leaf == O_I_LD_U8)
            return;
    }

    c->type = T_INT;
    code_op(c, is_int ? iop : fop);
}

void code_tobyte(Code *const c) { tonarrow(c, O_F_TOB, O_I_TOB); }
void code_toaddr(Code *const c) { tonarrow(c, O_F_TOA, O_I_TOA); }

void code_tofloat(Code *const c)
{
    if (c->type == T_FLT)
        return;
    if (c->type == T_DBL) {
        c->type = T_FLT;
        code_op(c, O_D_TOF);
        return;
    }

    c->type = T_FLT;
    uint8_t *const leaf = code_intleaf(c);

    if (!leaf) {
        code_op(c, O_I_TOF);
        return;
    }

    switch (*leaf) {
    case O_I_LD_V:
        *leaf = O_F_LD_VI;
        return;

    case O_I_LD_U8:
        *leaf = O_F_LD_U8;
        return;
    }

    const Sngl f = *leaf == O_I_LD_I32 ? fmth_from_i32((int32_t)load32(leaf + 1))
                                       : fmth_from_u16(load16(leaf + 1));
    c->out = leaf; /* F.LD.F32 in its place */
    code_op(c, O_F_LD_F32);
    code_dword(c, f);
}

void code_toinfo(Code *const c, const uint8_t info)
{
    if (info & SK_DBL)
        code_todouble(c);
    else if ((info & VK_MASK) == VK_INT)
        code_toint(c);
    else if ((info & VK_MASK) == VK_FLT)
        code_tofloat(c);
}
