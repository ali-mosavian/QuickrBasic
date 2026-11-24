/* Quickr's code generator: a checked statement to the VM's stack code, as QBasic's makes it.
 *
 * # How it works
 * Every operand is pushed, then its op: I.ADD, F.ADD, D.ADD by the operands' type, which sema
 * made one. A right operand that is one load folds into the operator (I.ADD.V, I.CMP.U8), and the
 * line's code is fused (comp/fuse.c), as QBasic's is. A bool is an int, -1 true, as QBasic's
 * compares give it. A variable is its id, a parameter's through its ref (V.LD, V.ST);
 * link_resolve (comp/select.c) places each.
 *
 * Control flow is the VM's jumps, each a line and a body offset: 0 the statement, BOFF the line's
 * end, which threads to the next line. A jump within the line (and, or, a if c else b, chains)
 * goes to a label: an entry of the line's statement map, body 1, 2 ... A jump drops the string
 * temporaries, so with a string on the stack it is CALL QJ by an offset instead (qr/qrt.c). Fusion
 * runs a segment at a time, ended where a jump lands or a QJ starts, so neither moves.
 *
 * A for loop is QBasic's FOR and NEXT: its limit made inclusive once the loop is entered, so its
 * end for fuses to I.NEXT.UP or I.NEXT.DN; a step whose sign is known only when run, I.PAST.
 *
 * A string literal loads from its line's text in the program store; an escape is CHR$ of its
 * byte, joined on.
 *
 * A record is QBasic's TYPE: a variable's a box R.ADDR finds, a field FL.LD and FL.ST at its
 * offset, a nested one's offsets added at compile time. A list is a dynamic array: an element
 * I.LD.A1 and its kin, a record's RA.ADDR, a field's list's FE.ADDR, a fixed array field's
 * FX.IDX; its methods and an f-string's specs are CALL QX (qr/qrt.c). A record or list result is
 * written through a last parameter, the caller's place. A match is SELECT CASE: its value held,
 * each case's line its tests, past the case before's JMP to end match.
 *
 * # Example
 *    while i < n          I.LD.V i, I.JFC.V n < (end while's line, BOFF)
 *    end while            JMP (while's line, 0)
 */
#include "qr/qcg.h"
#include "comp/codegen.h"
#include "comp/fuse.h"
#include "num/double.h"
#include "qr/qnat.h"
#include "qr/qrt.h"
#include "vm/exec.h"

enum { TXT_MAX = 255 }; /* S.LD.TXT's length at most */

/* The int and single operators a right operand folds into: by form (plain, a variable's load, a
   literal's), then operator. */
enum { QA_ADD, QA_SUB, QA_MUL, QA_DIV, QA_AND, QA_OR };
static const uint8_t qcg_iops[3][6] = {
    {O_I_ADD, O_I_SUB, O_I_MUL, O_I_DIV, O_I_AND, O_I_OR},
    {O_I_ADD_V, O_I_SUB_V, O_I_MUL_V, O_I_DIV_V, O_I_AND_V, O_I_OR_V},
    {O_I_ADD_U8, O_I_SUB_U8, O_I_MUL_U8, O_I_DIV_U8, O_I_AND_U8, O_I_OR_U8},
};
static const uint8_t qcg_fops[3][4] = {
    {O_F_ADD, O_F_SUB, O_F_MUL, O_F_DIV},
    {O_F_ADD_V, O_F_SUB_V, O_F_MUL_V, O_F_DIV_V},
    {O_F_ADD_F32, O_F_SUB_F32, O_F_MUL_F32, O_F_DIV_F32},
};
static const uint8_t qcg_dops[4] = {O_D_ADD, O_D_SUB, O_D_MUL, O_D_DIV};

/* A relation's D.CMP mask (num/double.h's DC_*). */
static uint8_t qcg_mask(const QKind op)
{
    switch (op) {
    case QK_EQ:
        return DC_EQ;

    case QK_NE:
        return DC_GT | DC_LT;

    case QK_LT:
        return DC_LT;

    case QK_LE:
        return DC_LT | DC_EQ;

    case QK_GT:
        return DC_GT;

    default:
        return DC_GT | DC_EQ;
    }
}

/* One of four ops by type: an int's or a bool's, a single's, a double's, a string's. */
static uint8_t qcg_pick(
    const QType t,
    const uint8_t i,
    const uint8_t f,
    const uint8_t d,
    const uint8_t s
)
{
    switch (t) {
    case QY_SINGLE:
        return f;

    case QY_DOUBLE:
    case QY_LONG: /* its 8 bytes moved as a double's */
    case QY_ULONG:
        return d;

    case QY_STRING:
        return s;

    default:
        return i;
    }
}

/* A parameter's ref: a fn's own, a lambda's capture's, a resumable generator's state, and a
   record's or list's result. */
static bool qcg_par(const QVar *const v)
{
    return v->kind == QV_PARAM || v->kind == QV_MUT || v->cap || v->byref ||
           (v->kind == QV_RESULT && v->fn->byarg);
}

static void qcg_load(Code *const c, const QVar *const v)
{
    code_opid(c, qcg_pick(v->type, O_I_LD_V, O_F_LD_V, O_D_LD_V, O_S_LD_V), v->id, qcg_par(v));
}

static void qcg_store(Code *const c, const QVar *const v)
{
    code_opid(c, qcg_pick(v->type, O_I_ST_V, O_F_ST_V, O_D_ST_V, O_S_ST_V), v->id, qcg_par(v));
}

/* Is the code from right to the end one op, op, of size bytes? */
static bool qcg_right(
    const Code *const c,
    const uint8_t *const right,
    const uint8_t op,
    const uint32_t size
)
{
    return (uint32_t)(c->out - right) == size && *right == op;
}

/* Operator k (QA_*) of type w after its operands: the right one's load, at right, folded in when
   one variable's or literal's. */
static void qcg_arith(Code *const c, uint8_t *const right, const QType w, const uint32_t k)
{
    uint32_t form = 0;

    if (w == QY_DOUBLE) {
        code_op(c, qcg_dops[k]);
        return;
    }

    if (w == QY_SINGLE) {
        form = qcg_right(c, right, O_F_LD_V, 1 + VCELL) ? 1
               : qcg_right(c, right, O_F_LD_F32, 5)     ? 2
                                                        : 0;
    } else {
        form = qcg_right(c, right, O_I_LD_V, 1 + VCELL) ? 1
               : qcg_right(c, right, O_I_LD_U8, 2)      ? 2
                                                        : 0;
    }

    const uint8_t op = w == QY_SINGLE ? qcg_fops[form][k] : qcg_iops[form][k];

    if (!form) {
        code_op(c, op);
        return;
    }

    *right = op;
    c->iload = c->iloadend = nullptr; /* no longer a load */
}

/* Integer type t as QX.INT and QX.CONV take it; a float's. */
static QrtNum qcg_num(const QType t)
{
    switch (t) {
    case QY_BYTE:
        return QN_I8;

    case QY_SHORT:
        return QN_I16;

    case QY_LONG:
        return QN_I64;

    case QY_UBYTE:
        return QN_U8;

    case QY_USHORT:
        return QN_U16;

    case QY_UINT:
        return QN_U32;

    case QY_ULONG:
        return QN_U64;

    case QY_SINGLE:
        return QN_F32;

    case QY_DOUBLE:
        return QN_F64;

    default:
        return QN_I32;
    }
}

/* CALL QX.INT: operator op of type t, wrapped or not; more: a compare's relation, QX_ICA/ICB. */
static void qcg_xint(
    Code *const c,
    const QrtIop op,
    const QType t,
    const bool wrap,
    const uint32_t more
)
{
    code_int(
        c,
        QX_INT | (uint32_t)op << QX_OP | (uint32_t)qcg_num(t) << QX_NUM |
            (uint32_t)wrap << QX_WRAP | more
    );
    code_call(c, K_QX);
}

/* CALL QX.CONV: a from as a to, wrapped or not. */
static void qcg_xconv(Code *const c, const QType from, const QType to, const bool wrap)
{
    code_int(
        c,
        QX_CONV | (uint32_t)qcg_num(from) << QX_OP | (uint32_t)qcg_num(to) << QX_TO |
            (uint32_t)wrap << QX_WRAP
    );
    code_call(c, K_QX);
}

/* Is t's value an int cell's int, compared and computed by the int ops: an integer to 16 bits,
   an int, a bool, an enum? A uint's bits are no int. */
static bool qcg_icell(const QType t) { return !qast_wide(t) && t != QY_UINT; }

/* A compare of two values of type w, the right one's load at right (folded when it can be). */
static void qcg_compare(Code *const c, const QType w, const QKind op, uint8_t *const right)
{
    const uint8_t m = qcg_mask(op);

    if (qast_integer(w) && !qcg_icell(w))
        qcg_xint(c, QI_CMP, w, false, (uint32_t)m << QX_REL);
    else if (w == QY_SINGLE)
        cg_fcmp(c, m, right);
    else if (w == QY_DOUBLE || w == QY_STRING)
        code_opb(c, w == QY_DOUBLE ? O_D_CMP : O_S_CMP, m);
    else
        cg_icmp(c, m, right);
}

/* ---- jumps */

/* The code since the last segment's end fused; the next segment starts here. */
static void qcg_flush(QCg *const g)
{
    Code *const c = g->c;

    if (!c->full && c->out != g->seg) {
        c->out = fuse_code(g->env->sy, g->seg, c->out);
        c->iload = c->iloadend = c->cmp = c->cmpend = c->shend = nullptr;
    }

    g->seg = c->out;
}

/* A Jump to statement s's line: its start (body 0), its end (BOFF), or past the JMP it starts
   with. */
static void qcg_target(QCg *const g, const QStmt *const s, const uint32_t body, const JumpMode mode)
{
    code_dword(g->c, g->env->lines[s->line].at);
    code_dword(g->c, body);
    code_dword(g->c, mode);
}

/* Jump op op to qcg_target's. */
static void qcg_to(
    QCg *const g,
    const uint8_t op,
    const QStmt *const s,
    const uint32_t body,
    const JumpMode mode
)
{
    code_op(g->c, op);
    qcg_target(g, s, body, mode);
}

/* A jump forward within the line, landed by qcg_land: a label's body, or a QJ's operand. */
typedef struct {
    uint32_t body;
    uint8_t *qj;
} QCgHop;

/* CALL QJ when, its offset patched by qcg_land; null: the code is full, the line made again. */
static uint8_t *qcg_qj(Code *const c, const QrtWhen when)
{
    uint8_t *const at = c->out + 1;
    code_op(c, O_I_LD_I32);
    code_dword(c, when);
    code_call(c, K_QJ);
    return c->full ? nullptr : at;
}

/* A jump within the line when: JMP, I.JF or I.JT to a label; with a string on the stack, QJ. */
static QCgHop qcg_hop(QCg *const g, const QrtWhen when)
{
    static const uint8_t ops[] = {[QJ_ALWAYS] = O_JMP, [QJ_FALSE] = O_I_JF, [QJ_TRUE] = O_I_JT};

    if (g->strings) { /* a segment of its own: fusion can't move it */
        qcg_flush(g);
        uint8_t *const at = qcg_qj(g->c, when);
        qcg_flush(g);
        return (QCgHop){.qj = at};
    }

    const uint32_t body = ++g->hops;
    code_op(g->c, ops[when]);
    code_dword(g->c, g->line);
    code_dword(g->c, body);
    code_dword(g->c, TO_STMT);
    return (QCgHop){.body = body};
}

/* Hop h lands here: a label, its map entry's room taken from the code's; or the QJ's offset. */
static void qcg_land(QCg *const g, const QCgHop h)
{
    qcg_flush(g);
    Code *const c = g->c;

    if (c->full)
        return;

    if (h.qj) {
        const int32_t from = (int32_t)(h.qj + 4 + 2 - c->buf);
        const int32_t rel = (int32_t)(c->out - c->buf) - from;
        store32(h.qj, (uint32_t)(rel * (1 << QJ_SHIFT)) | load32(h.qj));
        return;
    }

    g->labels = qast_room(g->mem, g->labels, g->nlabels, &g->caplabels, sizeof *g->labels);
    g->labels[g->nlabels++] = (QCgLabel){.body = h.body, .code = (uint32_t)(c->out - c->buf)};

    c->lim -= sizeof(Stmt);

    if (c->out > c->lim)
        c->full = true;
}

/* ---- values */

static void qcg_expr(QCg *g, const QExpr *e);
static void qcg_iter(QCg *g, const QExpr *e);

/* A string constant's text: each literal's runs from its line, its escapes CHR$, joined. */
static void qcg_string(QCg *const g, const QPiece *s)
{
    Code *const c = g->c;
    bool first = true;

    if (!s) { /* "" */
        code_op(c, O_S_LD_TXT);
        code_byte(c, 0);
        code_dword(c, g->env->base);
        return;
    }

    for (; s; s = s->next) {
        const QTok *const t = s->lit;
        const QLine *const ln = &g->env->lines[t->line];
        const Addr text = g->env->base + ln->at + HDR + (uint32_t)(t->text - ln->text);
        uint32_t i = 0;

        do {
            uint32_t n = 0;

            while (i + n < t->len && t->text[i + n] != '\\' && n < TXT_MAX)
                n++;

            if (n || !t->len) {
                code_op(c, O_S_LD_TXT);
                code_byte(c, (uint8_t)n);
                code_dword(c, text + i);
            } else {
                const char e = t->text[i + 1];
                code_int(c, e == 'n' ? '\n' : e == 't' ? '\t' : (uint8_t)e);
                code_call(c, K_FN_CHRS);
                n = 2;
            }

            if (!first)
                code_op(c, O_S_CAT);

            first = false;
            i += n;
        } while (i < t->len);
    }
}

/* A constant as type as: an int's, a float's, a bool's -1 or 0, a string's. */
static void qcg_const(QCg *const g, const QExpr *const e, const QType as)
{
    Code *const c = g->c;
    const double d = e->type == QY_ULONG     ? (double)(uint64_t)e->val.i
                     : qast_integer(e->type) ? (double)e->val.i
                                             : e->val.d;

    switch (as) {
    case QY_SINGLE:
        code_op(c, O_F_LD_F32);
        code_dword(c, (Cell){.f = (float)d}.u);

        return;

    case QY_DOUBLE:
    case QY_LONG:
    case QY_ULONG: {
        const uint64_t raw = as == QY_DOUBLE ? (Cell){.d = d}.raw : (uint64_t)e->val.i;

        code_op(c, O_D_LD_K);
        code_dword(c, (uint32_t)raw);
        code_dword(c, (uint32_t)(raw >> 32));
        return;
    }

    case QY_STRING:
        qcg_string(g, e->val.s);
        return;

    case QY_BOOL:
        code_int(c, e->val.i ? 0xFFFFFFFFu : 0);
        return;

    default:
        code_int(c, (uint32_t)e->val.i);
    }
}

/* QrtKind of a value of type t: an enum's an int. */
static QrtKind qcg_kind(const QType t)
{
    switch (t) {
    case QY_SINGLE:
        return QRT_SINGLE;

    case QY_DOUBLE:
        return QRT_DOUBLE;

    case QY_BOOL:
        return QRT_BOOL;

    case QY_STRING:
        return QRT_STRING;

    case QY_UINT:
        return QRT_UINT;

    case QY_LONG:
        return QRT_LONG;

    case QY_ULONG:
        return QRT_ULONG;

    default:
        return QRT_INT;
    }
}

/* Is every value of integer type a one of integer type b? */
static bool qcg_within(const QType a, const QType b)
{
    if (qast_unsigned(a) != qast_unsigned(b))
        return qast_unsigned(a) && qast_bits(a) < qast_bits(b);

    return qast_bits(a) <= qast_bits(b);
}

/* An int cell's int of type from (qcg_icell's) as integer type to: Overflow past to's range, by
   I.CHK8 or I.CHK16 where they check it; wrap keeps to's bits instead. */
static void qcg_narrow(Code *const c, const QType from, const QType to, const bool wrap)
{
    if (!wrap && qcg_within(from, to) && qcg_icell(to))
        return;
    if (!wrap && to == QY_UBYTE)
        code_op(c, O_I_CHK8);
    else if (!wrap && to == QY_SHORT)
        code_op(c, O_I_CHK16);
    else
        qcg_xconv(c, from, to, wrap);
}

/* The value on top, of type from, as type to: a float to an integer toward 0, an integer to a
   narrower one checked, or wrapped in wrap(..). */
static void qcg_conv(Code *const c, const QType from, const QType to, const bool wrap)
{
    if (from == to)
        return;

    if (to == QY_STRING) {
        code_int(c, qcg_kind(from));
        code_call(c, K_QSTR);
        return;
    }

    if (qast_integer(from) && qast_integer(to)) {
        if (qcg_icell(from))
            qcg_narrow(c, from, to, wrap);
        else if (wrap || !qcg_within(from, to) || qast_wide(to) != qast_wide(from))
            qcg_xconv(c, from, to, wrap);

        return;
    }

    if (qast_integer(from) && !qcg_icell(from)) { /* a uint, a long or a ulong to a float */
        qcg_xconv(c, from, to, false);
        return;
    }

    if (qast_float(from) && qast_integer(to) && (to != QY_INT || wrap)) {
        qcg_xconv(c, from, to, wrap);
        return;
    }

    if (to == QY_INT) { /* int(x): truncated */
        if (from == QY_SINGLE)
            code_op(c, O_F_TOD);

        code_opb(c, O_D_FN, DF_FIX);
        code_op(c, O_D_TOI);
        return;
    }

    if (to == QY_SINGLE)
        code_op(c, qast_integer(from) ? O_I_TOF : O_D_TOF);
    else
        code_op(c, qast_integer(from) ? O_I_TOD : O_F_TOD);
}

static void qcg_convert(Code *const c, const QType from, const QType to)
{
    qcg_conv(c, from, to, false);
}

/* e's value as type w: a constant made so, else converted. */
static void qcg_as(QCg *const g, const QExpr *const e, const QType w)
{
    if (e->kind == QE_CONST) {
        qcg_const(g, e, w);
        return;
    }

    qcg_expr(g, e);
    qcg_convert(g->c, e->type, w);
}

/* e's value pushed, held under what follows: a string counted (qcg_hop). */
static void qcg_hold(QCg *const g, const QType t, const int32_t by)
{
    if (t == QY_STRING)
        g->strings += (uint32_t)by;
}

/* An integer operator's QX.INT operator. */
static QrtIop qcg_iop(const QKind op)
{
    switch (op) {
    case QK_PLUS:
        return QI_ADD;

    case QK_MINUS:
        return QI_SUB;

    case QK_STAR:
        return QI_MUL;

    case QK_DIV:
        return QI_DIV;

    case QK_MOD:
        return QI_MOD;

    case QK_POW:
        return QI_POW;

    case QK_AMP:
        return QI_AND;

    case QK_BAR:
        return QI_OR;

    case QK_SHL:
        return QI_SHL;

    case QK_SHR:
        return QI_SHR;

    default:
        return QI_XOR;
    }
}

/* Operator op of integer type w other than an int's, or in wrap(..), its operands pushed: an int
   cell's by the int ops, then its range checked (qcg_narrow), the rest by QX.INT. A shift keeps
   w's bits; & | ^ stay in w's range. */
static void qcg_iarith(
    QCg *const g,
    uint8_t *const right,
    const QType w,
    const QKind op,
    const bool wrap,
    const uint32_t cells
)
{
    Code *const c = g->c;
    const bool bits = op == QK_AMP || op == QK_BAR || op == QK_CARET || op == QK_XOR;
    const bool checked =
        op == QK_PLUS || op == QK_MINUS || op == QK_STAR || op == QK_DIV || op == QK_POW;

    if (!qcg_icell(w) && !(bits && w == QY_UINT) && !(op == QK_SHL && w == QY_UINT)) {
        qcg_xint(c, qcg_iop(op), w, wrap, cells);
        return;
    }

    if (wrap && checked) {
        qcg_xint(c, qcg_iop(op), w, true, 0);
        return;
    }

    static const uint8_t qa[] = {
        [QK_PLUS] = QA_ADD,
        [QK_MINUS] = QA_SUB,
        [QK_STAR] = QA_MUL,
        [QK_DIV] = QA_DIV,
        [QK_AMP] = QA_AND,
        [QK_BAR] = QA_OR
    };

    switch (op) {
    case QK_PLUS:
    case QK_MINUS:
    case QK_STAR:
    case QK_DIV:
        qcg_arith(c, right, QY_INT, qa[op]);
        qcg_narrow(c, QY_INT, w, false);
        return;

    case QK_POW:
        code_call(c, K_IPOW);
        qcg_narrow(c, QY_INT, w, false);
        return;

    case QK_MOD:
        code_op(c, O_I_MOD);
        return;

    case QK_AMP:
    case QK_BAR:
        qcg_arith(c, right, QY_INT, qa[op]);
        return;

    case QK_SHL:
        code_call(c, K_QSHL);

        if (w != QY_INT && w != QY_UINT)
            qcg_narrow(c, QY_INT, w, true);

        return;

    case QK_SHR:
        code_call(c, K_QSHR);
        return;

    default:
        code_op(c, O_I_XOR);
    }
}

/* An operand of long or ulong w that is an int cell's integer, maybe converted: pushed as that
   cell, QX.INT widens it. Saves QX.CONV's two ops. */
static const QExpr *qcg_bycell(const QExpr *e, const QType w)
{
    if (e->kind == QE_CONV && !e->wrap && qast_integer(e->a->type))
        e = e->a;

    return qast_wide(w) && qast_integer(e->type) && qcg_icell(e->type) &&
                   (w == QY_LONG || qast_unsigned(e->type))
               ? e
               : nullptr;
}

/* Operand e of the binary as w: as its int cell when that saves ops (qcg_bycell). */
static uint32_t qcg_operand(QCg *const g, const QExpr *const e, const QType w, const uint32_t flag)
{
    const QExpr *const cell = e->kind == QE_CONST ? nullptr : qcg_bycell(e, w);

    if (!cell) {
        qcg_as(g, e, w);
        return 0;
    }

    qcg_expr(g, cell);
    return flag;
}

static void qcg_binary(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QType w = e->work;
    const bool shift = e->op == QK_SHL || e->op == QK_SHR;
    uint32_t cells = qcg_operand(g, e->a, w, 1u << QX_ICA);
    qcg_hold(g, w, 1);
    uint8_t *const right = c->out;

    if (shift)
        qcg_as(g, e->b, QY_INT);
    else
        cells |= qcg_operand(g, e->b, w, 1u << QX_ICB);

    qcg_hold(g, w, -1);

    if (qast_integer(w) && (w != QY_INT || e->wrap) && e->op != QK_SLASH) {
        qcg_iarith(g, right, w, e->op, e->wrap, cells);
        return;
    }

    switch (e->op) {
    case QK_PLUS:
        if (w == QY_STRING)
            code_op(c, O_S_CAT);
        else
            qcg_arith(c, right, w, QA_ADD);

        return;

    case QK_MINUS:
        qcg_arith(c, right, w, QA_SUB);
        return;

    case QK_STAR:
        qcg_arith(c, right, w, QA_MUL);
        return;

    case QK_SLASH:
        qcg_arith(c, right, w, QA_DIV);
        return;

    case QK_POW:
        if (w == QY_DOUBLE)
            code_op(c, O_D_POW);
        else
            code_call(c, w == QY_INT ? K_IPOW : K_FPOW);

        return;

    case QK_DIV:
        qcg_arith(c, right, QY_INT, QA_DIV);
        return;

    case QK_MOD:
        code_op(c, O_I_MOD);
        return;

    case QK_AMP:
        qcg_arith(c, right, QY_INT, QA_AND);
        return;

    case QK_BAR:
        qcg_arith(c, right, QY_INT, QA_OR);
        return;

    case QK_SHL:
        code_call(c, K_QSHL);
        return;

    case QK_SHR:
        code_call(c, K_QSHR);
        return;

    default: /* ^, xor */
        code_op(c, O_I_XOR);
    }
}

static void qcg_unary(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QType t = e->type;
    qcg_expr(g, e->a);

    if (qast_integer(t) && (!qcg_icell(t) || (e->wrap && e->op == QK_MINUS)))
        qcg_xint(c, e->op == QK_MINUS ? QI_NEG : QI_NOT, t, e->wrap, 0);
    else if (e->op == QK_MINUS) {
        code_op(c, qcg_pick(t, O_I_NEG, O_F_NEG, O_D_NEG, 0));

        if (qast_integer(t))
            qcg_narrow(c, QY_INT, t, false);
    } else if (qast_unsigned(t)) { /* ~ of a ubyte or a ushort: its bits flipped */
        code_int(c, (1u << qast_bits(t)) - 1);
        code_op(c, O_I_XOR);
    } else
        code_op(c, O_I_NOT);
}

/* Chain item k as the pair's type w: a middle's from its temporary. */
static void qcg_item(QCg *const g, const QExpr *const e, const uint32_t k, const QType w)
{
    if (!e->temps[k]) {
        qcg_as(g, e->items[k], w);
        return;
    }

    qcg_load(g->c, e->temps[k]);
    qcg_convert(g->c, e->items[k]->type, w);
}

/* a < b <= c from pair k on: the pair compared, a middle stored once; false, past the rest. */
static void qcg_chain(QCg *const g, const QExpr *const e, const uint32_t k)
{
    const QType w = e->works[k];
    qcg_item(g, e, k, w);
    qcg_hold(g, w, 1);

    if (e->temps[k + 1]) {
        qcg_expr(g, e->items[k + 1]);
        qcg_store(g->c, e->temps[k + 1]);
    }

    uint8_t *const right = g->c->out;
    qcg_item(g, e, k + 1, w);
    qcg_hold(g, w, -1);
    qcg_compare(g->c, w, e->ops[k], right);

    if (k + 2 >= e->n)
        return;

    const QCgHop no = qcg_hop(g, QJ_FALSE);
    qcg_chain(g, e, k + 1);
    const QCgHop over = qcg_hop(g, QJ_ALWAYS);
    qcg_land(g, no);
    code_int(g->c, 0);
    qcg_land(g, over);
}

/* a and b, a or b. */
static void qcg_logic(QCg *const g, const QExpr *const e)
{
    const bool and = e->kind == QE_AND;
    qcg_expr(g, e->a);
    const QCgHop skip = qcg_hop(g, and? QJ_FALSE : QJ_TRUE);
    qcg_expr(g, e->b);
    const QCgHop over = qcg_hop(g, QJ_ALWAYS);
    qcg_land(g, skip);
    code_int(g->c, and? 0 : 0xFFFFFFFFu);
    qcg_land(g, over);
}

/* a if c else b: a's value on the stack at the jump past b. */
static void qcg_cond(QCg *const g, const QExpr *const e)
{
    qcg_expr(g, e->c);
    const QCgHop other = qcg_hop(g, QJ_FALSE);
    qcg_expr(g, e->a);
    qcg_hold(g, e->type, 1);
    const QCgHop over = qcg_hop(g, QJ_ALWAYS);
    qcg_hold(g, e->type, -1);
    qcg_land(g, other);
    qcg_expr(g, e->b);
    qcg_land(g, over);
}

/* x in a..b: a <= x and x < b, x once. */
static void qcg_in(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QType w = e->work;

    if (e->var) {
        qcg_expr(g, e->a);
        qcg_store(c, e->var);
    }

    for (uint32_t side = 0; side < 2; side++) {
        if (e->var)
            qcg_load(c, e->var);
        else
            qcg_as(g, e->a, w);

        qcg_hold(g, w, 1);
        uint8_t *const right = c->out;
        qcg_as(g, side ? e->c : e->b, w);
        qcg_hold(g, w, -1);
        qcg_compare(c, w, side ? QK_LT : QK_GE, right);
    }

    /* the first compare's flag is under the second's: and of -1 and 0 */
    code_op(c, O_I_AND);

    if (e->neg)
        code_op(c, O_I_NOT);
}

/* ---- places: records and lists */

static const QTy *qcg_ty(const QCg *const g, const QType t) { return qast_ty(g->env->prog, t); }
static bool qcg_rec(const QCg *const g, const QType t) { return qast_record(g->env->prog, t); }
static bool qcg_list(const QCg *const g, const QType t) { return qast_array(g->env->prog, t); }

/* Is t's value kept in a place, a record's or a list's, not on the stack? */
static bool qcg_byplace(const QCg *const g, const QType t)
{
    return qcg_rec(g, t) || qcg_list(g, t);
}

uint32_t qcg_scalar_et(const QType t)
{
    switch (t) {
    case QY_SINGLE:
        return ET_R4;

    case QY_DOUBLE:
    case QY_LONG:
    case QY_ULONG:
        return ET_R8;

    case QY_STRING:
        return ET_SD;

    case QY_BYTE:
        return ET_SB;

    case QY_UBYTE:
        return ET_BY;

    case QY_SHORT:
        return ET_SH;

    case QY_USHORT:
        return ET_US;

    default:
        return ET_I4;
    }
}

/* Type t's ET_*: a record's ET_REC + its TYPE, a scalar's qcg_scalar_et. */
static uint32_t qcg_et(const QCg *const g, const QType t)
{
    return qcg_rec(g, t) ? ET_REC + qcg_ty(g, t)->td : qcg_scalar_et(t);
}

/* Is t kept in 2 bytes or 1 in a record or a list: N.LD.A, P.REFN? */
static bool qcg_short(const QCg *const g, const QType t) { return et_narrow(qcg_et(g, t)); }

/* A value of type t's bytes in a record or an array. */
static uint32_t qcg_size(const QCg *const g, const QType t)
{
    switch (qcg_et(g, t)) {
    case ET_R8:
        return 8;

    case ET_SD:
        return DSZ;

    case ET_SH:
    case ET_US:
        return 2;

    case ET_BY:
    case ET_SB:
        return 1;

    default:
        return qcg_rec(g, t) ? qcg_ty(g, t)->size : 4;
    }
}

static QExpr qcg_named(QVar *const v)
{
    return (QExpr){.kind = QE_NAME, .var = v, .type = v->type};
}

/* Place a's field k. */
static QExpr qcg_fieldof(const QCg *const g, const QExpr *const a, const uint32_t k)
{
    return (QExpr){
        .kind = QE_FIELD,
        .a = (QExpr *)a,
        .field = k,
        .type = qcg_ty(g, a->type)->fields[k].type,
    };
}

/* The address on top moved on by off bytes. */
static void qcg_offset(QCg *const g, const uint32_t off)
{
    if (!off)
        return;

    uint8_t *const right = g->c->out;
    code_int(g->c, off);
    qcg_arith(g->c, right, QY_INT, QA_ADD);
}

static void qcg_into(QCg *g, const QExpr *dst, const QExpr *e);
static void qcg_place(QCg *g, const QExpr *dst, const QExpr *e);
static void qcg_dropall(QCg *g, const QOwners *o);
static void qcg_build(QCg *g, const QExpr *dst, const QExpr *e);
static uint32_t qcg_addr(QCg *g, const QExpr *e);

/* A fresh record or list built in its temporary (sema's): that temporary's place. */
static QExpr qcg_settle(QCg *const g, const QExpr *const e)
{
    const QExpr t = qcg_named(e->var);
    qcg_build(g, &t, e);
    return t;
}

/* Record place a's field f's offset from a's address, pushed; f's own added. */
static uint32_t qcg_fieldat(QCg *const g, const QExpr *const f)
{
    return qcg_addr(g, f->a) + qcg_ty(g, f->a->type)->fields[f->field].off;
}

/* The variable a list or array place names: its own, or the temporary a fresh one is built in. */
static QVar *qcg_listvar(QCg *const g, const QExpr *const a)
{
    if (a->kind == QE_NAME)
        return a->var;

    qcg_settle(g, a);
    return a->var;
}

static void qcg_x(Code *c, QrtX op);

/* QX.MAPAT's and QX.MAPDEL's operands: map place m's key, its ETs, then its address, last: the
   key's evaluation may move the heap. */
static void qcg_mapargs(QCg *const g, const QExpr *const m, const QExpr *const key)
{
    const QTy *const y = qcg_ty(g, m->type);
    qcg_as(g, key, y->key);
    code_int(g->c, qcg_et(g, y->key));
    code_int(g->c, qcg_et(g, y->elem));
    qcg_offset(g, qcg_addr(g, m));
}

/* Element place e, a[i]: its address pushed. A field's list's by FE.ADDR, its fixed array's by
   FX.IDX; a variable's by RA.ADDR; a map's value by QX.MAPAT. */
static void qcg_elem(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QExpr *const a = e->a;
    const QTy *const y = qcg_ty(g, a->type);

    if (y->kind == QT_MAP) {
        qcg_mapargs(g, a, e->b);
        code_int(c, e->insert ? QM_SET : QM_READ);
        qcg_x(c, QX_MAPAT);
        return;
    }

    if (a->kind == QE_FIELD) {
        const uint32_t off = qcg_fieldat(g, a);

        if (y->kind == QT_LIST) {
            qcg_expr(g, e->b);
            code_op(c, O_FE_ADDR);
            code_dword(c, off);
            code_byte(c, 1);
            code_dword(c, qcg_size(g, y->elem));
            return;
        }

        qcg_offset(g, off);
        qcg_expr(g, e->b);
        code_op(c, O_FX_IDX);
        code_dword(c, 0);
        code_dword(c, y->count);
        code_dword(c, qcg_size(g, y->elem));
        return;
    }

    const QVar *const v = qcg_listvar(g, a);
    qcg_expr(g, e->b);
    code_opid(c, O_RA_ADDR, v->id, false);
    code_byte(c, 1);
}

/* Record place e's address pushed; the bytes returned still to add, a field's. */
static uint32_t qcg_addr(QCg *const g, const QExpr *const e)
{
    if (qast_fresh(e)) {
        const QExpr t = qcg_settle(g, e);
        return qcg_addr(g, &t);
    }

    switch (e->kind) {
    case QE_NAME:
        code_opid(g->c, O_R_ADDR, e->var->id, false);
        return 0;

    case QE_FIELD:
        return qcg_fieldat(g, e);

    default:
        qcg_elem(g, e);
        return 0;
    }
}

/* List place e's cell's address pushed: a variable's, a parameter's through its ref, a field's. */
static void qcg_cell(QCg *const g, const QExpr *const e)
{
    if (e->kind == QE_FIELD) {
        qcg_offset(g, qcg_fieldat(g, e));
        return;
    }

    const QVar *const v = qcg_listvar(g, e);
    code_opid(g->c, qcg_par(v) ? O_V_REF : O_V_CELL, v->id, false);
}

/* A field's load or store of type t, off bytes past the address under it. */
static void qcg_fieldop(QCg *const g, const bool store, const uint32_t off, const QType t)
{
    Code *const c = g->c;

    if (t == QY_STRING) {
        code_op(c, store ? O_SF_ST : O_SF_LD);
        code_word(c, (uint16_t)off);
        code_word(c, 0); /* a variable string's Desc */
        return;
    }

    code_op(c, store ? O_FL_ST : O_FL_LD);
    code_word(c, (uint16_t)off);
    code_byte(c, (uint8_t)qcg_et(g, t));
}

/* Is element place e a variable's list's, loaded and stored by I.LD.A1 and its kin? */
static bool qcg_byindex(const QCg *const g, const QExpr *const e)
{
    return e->kind == QE_INDEX && e->a->kind != QE_FIELD && qcg_list(g, e->a->type);
}

/* Scalar place e's value pushed. */
static void qcg_loadat(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;

    if (e->kind == QE_NAME) {
        qcg_load(c, e->var);
        return;
    }

    if (e->kind == QE_FIELD) {
        qcg_fieldop(g, false, qcg_fieldat(g, e), e->type);
        return;
    }

    if (qcg_byindex(g, e)) {
        const QVar *const v = qcg_listvar(g, e->a);
        qcg_expr(g, e->b);

        if (qcg_short(g, e->type)) {
            code_opid(c, O_N_LD_A, v->id, false);
            code_byte(c, 1);
            code_byte(c, (uint8_t)qcg_et(g, e->type));
            return;
        }

        code_opid(c, qcg_pick(e->type, O_I_LD_A1, O_F_LD_A1, O_D_LD_A1, O_S_LD_A1), v->id, false);
        return;
    }

    qcg_elem(g, e);
    qcg_fieldop(g, false, 0, e->type);
}

/* A list element's store, the index under the value. */
static void qcg_storeat(QCg *const g, const QVar *const list, const QType type)
{
    if (qcg_short(g, type)) {
        code_opid(g->c, O_N_ST_A, list->id, false);
        code_byte(g->c, 1);
        code_byte(g->c, (uint8_t)qcg_et(g, type));
        return;
    }

    code_opid(g->c, qcg_pick(type, O_I_ST_A1, O_F_ST_A1, O_D_ST_A1, O_S_ST_A1), list->id, false);
}

/* A scalar place's store: what goes before the value (an address, an index), then its op. */
typedef struct {
    uint32_t off;
    const QVar *list; /* by I.ST.A1 and its kin */
    bool at;          /* by FL.ST or SF.ST, the address under the value */
} QCgPut;

static QCgPut qcg_before(QCg *const g, const QExpr *const e)
{
    if (e->kind == QE_NAME)
        return (QCgPut){};

    if (e->kind == QE_FIELD)
        return (QCgPut){.off = qcg_fieldat(g, e), .at = true};

    if (qcg_byindex(g, e)) {
        const QVar *const v = qcg_listvar(g, e->a);
        qcg_expr(g, e->b);
        return (QCgPut){.list = v};
    }

    qcg_elem(g, e);
    return (QCgPut){.at = true};
}

static void qcg_put(QCg *const g, const QExpr *const e, const QCgPut p)
{
    if (p.at)
        qcg_fieldop(g, true, p.off, e->type);
    else if (p.list)
        qcg_storeat(g, p.list, e->type);
    else
        qcg_store(g->c, e->var);
}

/* Record place p emptied: its strings "", its lists empty, the rest 0. */
static void qcg_clear(QCg *const g, const QExpr *const p)
{
    qcg_offset(g, qcg_addr(g, p));
    code_int(g->c, qcg_ty(g, p->type)->td);
    code_call(g->c, K_RCLR);
}

static void qcg_set(QCg *g, const QExpr *place, const QExpr *v);

/* Record place p, emptied, given its fields' defaults, its records' too. */
static void qcg_defaults(QCg *const g, const QExpr *const p)
{
    const QTy *const y = qcg_ty(g, p->type);

    for (uint32_t k = 0; y->kind == QT_REC && k < y->nfields; k++) {
        const QExpr f = qcg_fieldof(g, p, k);

        if (y->fields[k].def)
            qcg_set(g, &f, y->fields[k].def);
        else if (qcg_rec(g, f.type))
            qcg_defaults(g, &f);
    }
}

/* A record of type t copied, its destination's and its source's addresses on the stack: R.COPY,
   or RCOPY when it holds strings or lists. */
static void qcg_recop(QCg *const g, const QType t)
{
    const QTy *const y = qcg_ty(g, t);

    if (y->deep) {
        code_int(g->c, y->td);
        code_call(g->c, K_RCOPY);
        return;
    }

    code_op(g->c, O_R_COPY);
    code_word(g->c, (uint16_t)y->size);
}

/* Record place src's value copied to record place dst. */
static void qcg_copyrec(QCg *const g, const QExpr *const dst, const QExpr *const src)
{
    qcg_offset(g, qcg_addr(g, dst));
    qcg_offset(g, qcg_addr(g, src));
    qcg_recop(g, dst->type);
}

/* x?: T! place x checked, its error copied to the fn's result and returned; x's place, a fresh
   one's temporary, for its value's read. */
static QExpr qcg_tried(QCg *const g, const QExpr *const e)
{
    const QExpr t = qast_fresh(e->a) ? qcg_settle(g, e->a) : *e->a;
    const QExpr err = qcg_fieldof(g, &t, QF_FLAG);
    qcg_loadat(g, &err);
    const QCgHop ok = qcg_hop(g, QJ_FALSE);
    const QExpr res = qcg_named(e->fn->res);
    static const uint32_t parts[] = {QF_FLAG, QF_CODE, QF_MESSAGE, QF_LINE};

    for (uint32_t k = 0; k < countof(parts); k++) {
        const QExpr to = qcg_fieldof(g, &res, parts[k]);
        const QExpr from = qcg_fieldof(g, &t, parts[k]);
        qcg_set(g, &to, &from);
    }

    qcg_dropall(g, &e->fn->own);
    code_op(g->c, O_PRET);
    code_dword(g->c, e->fn->proc);
    code_dword(g->c, 0);
    qcg_land(g, ok);
    return t;
}

/* m.get(k) built in dst: has whether k's value's address is not 0, then the value from it. */
static void qcg_get(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    Code *const c = g->c;
    const QVar *const at = e->temps[0];
    const QExpr has = qcg_fieldof(g, dst, QF_FLAG);
    const QExpr val = qcg_fieldof(g, dst, QF_VAL);

    qcg_mapargs(g, e->a, e->b);
    code_int(c, QM_FIND);
    qcg_x(c, QX_MAPAT);
    qcg_store(c, at);

    for (uint32_t k = 0; k < 2; k++) { /* has set, then a jump past the value when not */
        const QCgPut put = k ? (QCgPut){} : qcg_before(g, &has);

        qcg_load(c, at);
        uint8_t *const right = c->out;
        code_int(c, 0);
        qcg_compare(c, QY_INT, QK_NE, right);

        if (!k)
            qcg_put(g, &has, put);
    }

    const QCgHop none = qcg_hop(g, QJ_FALSE);
    const QCgPut put = qcg_before(g, &val);
    qcg_load(c, at);
    qcg_fieldop(g, false, 0, val.type);
    qcg_put(g, &val, put);
    qcg_land(g, none);
}

/* A slice's two ends pushed: 0 and the most an int holds for those left out. */
static void qcg_ends(QCg *const g, const QExpr *const e)
{
    if (e->b)
        qcg_expr(g, e->b);
    else
        code_int(g->c, 0);

    if (e->c)
        qcg_expr(g, e->c);
    else
        code_int(g->c, INT32_MAX);
}

/* List place src's elements to list place dst: moved (src left empty), or copied. */
static void qcg_listto(
    QCg *const g,
    const QExpr *const dst,
    const QExpr *const src,
    const bool move
)
{
    qcg_cell(g, src);
    qcg_cell(g, dst);
    code_call(g->c, move ? K_AMOVE : K_ACOPY);
}

/* Scalar or compound value v stored in place p. */
static void qcg_set(QCg *const g, const QExpr *const p, const QExpr *const v)
{
    if (qcg_byplace(g, p->type)) {
        qcg_into(g, p, v);
        return;
    }

    const QCgPut put = qcg_before(g, p);
    qcg_as(g, v, p->type);
    qcg_put(g, p, put);
}

/* Record, tuple or enum value e (QE_NEW) built in place dst, field by field: emptied first when
   a field left out is no number. */
static void qcg_new(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    const QTy *const y = qcg_ty(g, e->type);
    const bool en = y->kind == QT_ENUM;
    const uint32_t first = en ? y->vars[e->field].first : 0;
    bool clear = false;

    for (uint32_t k = 0; k < e->nargv; k++)
        clear |= !e->argv[k] && qcg_byplace(g, y->fields[first + k].type);

    if (clear)
        qcg_clear(g, dst);

    if (en) {
        const QExpr tag = qcg_fieldof(g, dst, 0);
        const QExpr v = {.kind = QE_CONST, .type = QY_INT, .val.i = e->field};

        qcg_set(g, &tag, &v);
    }

    for (uint32_t k = 0; k < e->nargv; k++) {
        const QExpr f = qcg_fieldof(g, dst, first + k);
        const QExpr zero = {.kind = QE_CONST, .type = f.type};

        if (e->argv[k])
            qcg_set(g, &f, e->argv[k]);
        else if (qcg_rec(g, f.type))
            qcg_defaults(g, &f);
        else if (!clear || !qcg_byplace(g, f.type))
            qcg_set(g, &f, &zero);
    }
}

/* A by-reference argument, a 0 and an address: a variable's cell, a field's, an element's, a
   list's cell. */
static void qcg_ref(QCg *const g, const QExpr *const a)
{
    Code *const c = g->c;

    if (qcg_list(g, a->type)) {
        code_int(c, 0);
        qcg_cell(g, a);
        return;
    }

    if (a->kind == QE_NAME && !qcg_rec(g, a->type)) {
        code_int(c, 0);
        code_opid(c, qcg_par(a->var) ? O_V_REF : O_V_CELL, a->var->id, false);
        return;
    }

    const uint32_t off = qcg_addr(g, a);

    if (qcg_short(g, a->type)) { /* its ref tagged: read and written as 2 bytes or 1 */
        qcg_offset(g, off);
        code_opb(c, O_P_REFN, (uint8_t)qcg_et(g, a->type));
        return;
    }

    code_op(c, O_P_REF);
    code_word(c, (uint16_t)off);
}

/* print(a, b): each spelled, a space between, a new line. */
static void qcg_print(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;

    for (uint32_t i = 0; i < e->nargv; i++) {
        const QExpr *const a = e->argv[i];

        if (i) {
            code_int(c, QRT_SPACE);
            code_call(c, K_QPR);
        }

        qcg_expr(g, a);
        code_int(c, qcg_kind(a->type));
        code_call(c, K_QPR);
    }

    code_call(c, K_PNL);
}

/* Argument a for a parameter of type t: by reference where it has a place, else a value cell and
   its ref; a string's counted held, on the stack under the rest. */
static void qcg_arg(QCg *const g, const QExpr *const a, const QType t, uint32_t *const held)
{
    if (qcg_byplace(g, t) || (!qast_fresh(a) && a->type == t)) {
        qcg_ref(g, a);
        return;
    }

    qcg_expr(g, a);
    code_opb(g->c, O_BYVAL, a->type == QY_STRING);
    *held += a->type == QY_STRING;
    qcg_hold(g, a->type, 1);
}

/* A call through fn value e->a: its arguments as qcg_call's, a record's or list's result's place
   dst, then the value, QX.CALL. */
static void qcg_icall(QCg *const g, const QExpr *const e, const QExpr *const dst)
{
    const QTy *const y = qcg_ty(g, e->a->type);
    uint32_t held = 0;

    for (uint32_t k = 0; k < e->nargv; k++)
        qcg_arg(g, e->argv[k], y->fields[k].type, &held);

    if (qcg_byplace(g, y->elem))
        qcg_ref(g, dst);

    qcg_expr(g, e->a);
    g->strings -= held;
    qcg_x(g->c, QX_FNCALL);
}

/* Native fn call e: its entry's steps, each argument as its parameter's type. */
static void qcg_native(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QNat *const nat = e->fn->nat;
    uint32_t held = 0;

    for (uint32_t i = 0; i < nat->n; i++) {
        const QNatStep st = nat->steps[i];

        switch (st.kind) {
        case QN_ARG: {
            const QType t = e->fn->params[st.n].var->type;
            qcg_as(g, e->argv[st.n], t);
            qcg_hold(g, t, 1);
            held += t == QY_STRING;
            break;
        }

        case QN_OP:
            code_op(c, st.op);
            break;

        case QN_OPB:
            code_opb(c, st.op, (uint8_t)st.n);
            break;

        case QN_INT:
            code_int(c, (uint32_t)st.n);
            break;

        case QN_CALL:
            code_call(c, st.op);
            break;

        case QN_SYS:
            code_opb(c, O_SYSCALL, st.op);
            break;

        case QN_X:
            code_int(c, (uint32_t)st.n);
            code_call(c, K_QX);
        }
    }

    g->strings -= held;
}

/* Fn f called, its arguments pushed. */
static void qcg_pcall(QCg *const g, const QFn *const f)
{
    Code *const c = g->c;
    code_op(c, O_PCALL); /* the procedure, its locals' cells, its parameters (link's), its body */
    code_dword(c, f->proc);
    code_dword(c, 0);
    code_word(c, 0);
    qcg_target(g, f->def, 0, PAST_JMP);
}

/* A fn's call: each argument by reference where it has a place, else a value cell and its ref;
   a record's or list's result's place, dst, last. A parameter bound to a lambda takes none. */
static void qcg_call(QCg *const g, const QExpr *const e, const QExpr *const dst)
{
    if (e->builtin == QB_PRINT) {
        qcg_print(g, e);
        return;
    }

    if (e->builtin == QB_ICALL) {
        qcg_icall(g, e, dst);
        return;
    }

    const QFn *const f = e->fn;
    uint32_t held = 0;

    if (f->native) {
        qcg_native(g, e);
        return;
    }

    for (uint32_t k = 0; k < f->npar; k++) {
        const QParam *const p = &f->params[k];
        const QExpr *const a = f->keep ? f->keep[k] : e->argv[k];

        if (p->lam) /* bound to its lambda: no value */
            continue;

        if (p->mut) {
            qcg_ref(g, a);
            continue;
        }

        qcg_arg(g, a, p->var->type, &held);
    }

    if (f->byarg)
        qcg_ref(g, dst);

    g->strings -= held;
    qcg_pcall(g, f);
}

/* Lambda application e's arguments, each stored to its parameter's temporary; those read where
   they are need none. */
static void qcg_applied(QCg *const g, const QExpr *const e)
{
    for (uint32_t k = 0; k < e->nargv; k++)
        if (e->temps[k]) {
            const QExpr t = qcg_named(e->temps[k]);
            qcg_set(g, &t, e->argv[k]);
        }
}

/* CALL QX op. */
static void qcg_x(Code *const c, const QrtX op)
{
    code_int(c, op);
    code_call(c, K_QX);
}

/* ---- drop */

/* List place a's last element: a[len(a) - 1]'s parts, in len, one, last, at. */
typedef struct {
    QExpr len, one, last, at;
} QCgLast;

static void qcg_lastof(QCgLast *const l, const QExpr *const a, const QType elem)
{
    l->len = (QExpr){
        .kind = QE_CALL,
        .builtin = QB_LEN,
        .a = (QExpr *)a,
        .type = QY_INT,
    };

    l->one = (QExpr){.kind = QE_CONST, .type = QY_INT, .val.i = 1};

    l->last = (QExpr){
        .kind = QE_BINARY,
        .op = QK_MINUS,
        .a = &l->len,
        .b = &l->one,
        .type = QY_INT,
        .work = QY_INT,
    };

    l->at = (QExpr){
        .kind = QE_INDEX,
        .a = (QExpr *)a,
        .b = &l->last,
        .type = elem,
    };
}

static void qcg_bump(Code *c, QVar *v, uint32_t by);
static uint32_t qcg_top(QCg *g);
static void qcg_back(QCg *g, QrtWhen when, uint32_t top);

static void qcg_drop(QCg *g, const QExpr *p, uint32_t depth);

static bool qcg_drops(const QCg *const g, const QType t) { return qast_drops(g->env->prog, t); }

/* List or array place p's elements dropped in turn, by the frame's index at depth. */
static void qcg_dropeach(QCg *const g, const QExpr *const p, const uint32_t depth)
{
    Code *const c = g->c;
    const QTy *const y = qcg_ty(g, p->type);
    QVar *const i = g->own->ix[depth];
    const QExpr at = qcg_named(i);
    const QExpr elem = {
        .kind = QE_INDEX,
        .a = (QExpr *)p,
        .b = (QExpr *)&at,
        .type = y->elem,
    };

    code_int(c, 0);
    qcg_store(c, i);
    const uint32_t top = qcg_top(g);
    qcg_load(c, i);
    uint8_t *const right = c->out;

    if (y->kind == QT_ARRAY)
        code_int(c, y->count);
    else {
        qcg_cell(g, p);
        qcg_x(c, QX_LEN);
    }

    qcg_compare(c, QY_INT, QK_LT, right);
    const QCgHop out = qcg_hop(g, QJ_FALSE);
    qcg_drop(g, &elem, depth + 1);
    qcg_bump(c, i, 1);
    qcg_back(g, QJ_ALWAYS, top);
    qcg_land(g, out);
}

/* The value with drop in place p dropped: a record's fn drop, then its fields'; a list's
   elements'; T?'s value when it has one, T!'s when no error. */
static void qcg_drop(QCg *const g, const QExpr *const p, const uint32_t depth)
{
    const QTy *const y = qcg_ty(g, p->type);

    switch (y->kind) {
    case QT_REC:
    case QT_TUPLE:
        if (y->drop) {
            qcg_ref(g, p);
            qcg_pcall(g, y->drop);
        }

        for (uint32_t k = 0; k < y->nfields; k++)
            if (qcg_drops(g, y->fields[k].type)) {
                const QExpr f = qcg_fieldof(g, p, k);
                qcg_drop(g, &f, depth);
            }

        return;

    case QT_OPT:
    case QT_RES: {
        const QExpr flag = qcg_fieldof(g, p, QF_FLAG);
        const QExpr v = qcg_fieldof(g, p, QF_VAL);
        qcg_loadat(g, &flag);
        const QCgHop none = qcg_hop(g, y->kind == QT_OPT ? QJ_FALSE : QJ_TRUE);
        qcg_drop(g, &v, depth);
        qcg_land(g, none);
        return;
    }

    default:
        qcg_dropeach(g, p, depth);
    }
}

/* Owner v's live flag set to on. */
static void qcg_live(QCg *const g, const QVar *const v, const bool on)
{
    code_int(g->c, on ? 1 : 0);
    qcg_store(g->c, v->live);
}

/* Owner v's value dropped if it holds one. */
static void qcg_dropvar(QCg *const g, QVar *const v)
{
    const QExpr p = qcg_named(v);
    qcg_load(g->c, v->live);
    const QCgHop none = qcg_hop(g, QJ_FALSE);
    qcg_drop(g, &p, 0);
    qcg_land(g, none);
}

/* The frame's owners' values dropped, the last declared first: its fn returns, the program
   ends. */
static void qcg_dropall(QCg *const g, const QOwners *const o)
{
    for (uint32_t k = o->nowned; k-- > 0;)
        qcg_dropvar(g, o->owned[k]);
}

/* The value with drop in place p dropped before p takes another: an owner's if it holds one. */
static void qcg_dropold(QCg *const g, const QExpr *const p)
{
    if (p->kind == QE_NAME && p->var->live)
        qcg_dropvar(g, p->var);
    else
        qcg_drop(g, p, 0);
}

/* List place a's last element dropped, if its type has drop. */
static void qcg_droplast(QCg *const g, const QExpr *const a, const QType elem)
{
    if (!qcg_drops(g, elem))
        return;

    QCgLast l;
    qcg_lastof(&l, a, elem);
    qcg_drop(g, &l.at, 0);
}

/* Value e taken from its owner: its live flag cleared. */
static void qcg_moved(QCg *const g, const QExpr *const e)
{
    if (e->kind == QE_NAME && e->move)
        qcg_live(g, e->var, false);
}

/* A list's method; a record's pop copied to dst. */
static void qcg_listop(QCg *const g, const QExpr *const e, const QExpr *const dst)
{
    Code *const c = g->c;
    const QType elem = qcg_ty(g, e->a->type)->elem;
    const uint32_t et = qcg_et(g, elem);

    if (qcg_ty(g, e->a->type)->kind == QT_MAP) { /* remove(k) */
        qcg_mapargs(g, e->a, e->argv[0]);
        qcg_x(c, QX_MAPDEL);
        return;
    }

    switch (e->builtin) {
    case QB_PUSH:
    case QB_INSERT: { /* one more at the end, the value there; insert's moved to its place */
        const QExpr *v = e->argv[e->nargv - 1];
        QExpr held;

        if (qcg_rec(g, elem) && qast_fresh(v)) { /* built first: the element's address stays */
            held = qcg_settle(g, v);
            v = &held;
        }

        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_x(c, QX_PUSH);

        if (e->a->kind == QE_FIELD) { /* the push may move its record: found again */
            code_op(c, O_DROP);
            QCgLast l;
            qcg_lastof(&l, e->a, elem);
            qcg_set(g, &l.at, v);
        } else if (qcg_rec(g, elem)) {
            code_opid(c, O_RA_ADDR, qcg_listvar(g, e->a)->id, false);
            code_byte(c, 1);
            qcg_offset(g, qcg_addr(g, v));
            qcg_recop(g, elem);
        } else {
            qcg_as(g, v, elem);
            qcg_storeat(g, qcg_listvar(g, e->a), elem);
        }

        qcg_moved(g, e->argv[e->nargv - 1]);

        if (e->builtin == QB_PUSH)
            return;

        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_expr(g, e->argv[0]);
        qcg_x(c, QX_SINK);
        return;
    }

    case QB_POP:
        if (dst) {
            qcg_offset(g, qcg_addr(g, dst));
            qcg_cell(g, e->a);
            qcg_x(c, QX_LAST);
            qcg_recop(g, elem);
        } else
            qcg_droplast(g, e->a, elem);

        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_x(c, dst || qcg_rec(g, elem) ? QX_DROP : QX_POP);
        return;

    default: /* remove: moved last, then dropped */
        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_expr(g, e->argv[0]);
        qcg_x(c, QX_RAISE);
        qcg_droplast(g, e->a, elem);
        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_x(c, QX_DROP);
    }
}

/* x in a, a list: QX.HAS. */
static void qcg_has(QCg *const g, const QExpr *const e)
{
    if (qcg_ty(g, e->b->type)->kind == QT_MAP) {
        qcg_mapargs(g, e->b, e->a);
        code_int(g->c, QM_FIND);
        qcg_x(g->c, QX_MAPAT);
        uint8_t *const right = g->c->out;
        code_int(g->c, 0);
        qcg_compare(g->c, QY_INT, e->neg ? QK_EQ : QK_NE, right);
        return;
    }

    const QType elem = qcg_ty(g, e->b->type)->elem;
    qcg_cell(g, e->b);
    code_int(g->c, qcg_et(g, elem) | (qast_wide(elem) ? QRT_RAW : 0));
    qcg_as(g, e->a, elem);
    qcg_x(g->c, QX_HAS);

    if (e->neg)
        code_op(g->c, O_I_NOT);
}

/* f"..": its parts joined, each a literal's text, a value's string() or its spec's QX.FMT. */
static void qcg_fstring(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;

    if (!e->n)
        qcg_string(g, nullptr);

    for (uint32_t k = 0; k < e->n; k++) {
        const QExpr *const v = e->items[k];

        if (v->tok->kind == QK_FPART)
            qcg_string(g, v->val.s);
        else if (!e->specs[k]) {
            qcg_expr(g, v);
            qcg_convert(c, v->type, QY_STRING);
        } else {
            const QPiece spec = {.lit = e->specs[k]};

            qcg_expr(g, v);
            qcg_hold(g, v->type, 1);
            qcg_string(g, &spec);
            code_int(c, qcg_kind(v->type));
            qcg_x(c, QX_FMT);
            qcg_hold(g, v->type, -1);
        }

        if (k)
            code_op(c, O_S_CAT);

        qcg_hold(g, QY_STRING, k ? 0 : 1);
    }

    qcg_hold(g, QY_STRING, e->n ? -1 : 0);
}

/* Fresh record or list e built straight in place dst. */
static void qcg_build(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    switch (e->kind) {
    case QE_NEW:
        qcg_new(g, dst, e);
        return;

    case QE_LIST:
        qcg_listop(g, e, dst);
        return;

    case QE_COND: {
        qcg_expr(g, e->c);
        const QCgHop other = qcg_hop(g, QJ_FALSE);
        qcg_into(g, dst, e->a);
        const QCgHop over = qcg_hop(g, QJ_ALWAYS);
        qcg_land(g, other);
        qcg_into(g, dst, e->b);
        qcg_land(g, over);
        return;
    }

    case QE_TRY: {
        const QExpr t = qcg_tried(g, e);
        const QExpr v = qcg_fieldof(g, &t, QF_VAL);
        qcg_into(g, dst, &v);
        return;
    }

    case QE_APPLY:
        qcg_applied(g, e);
        qcg_into(g, dst, e->a);
        return;

    case QE_ITER: { /* built in its acc, or its T? after */
        qcg_iter(g, e);
        const QExpr acc = qcg_named(e->iter->acc);

        if (e->iter->result)
            qcg_into(g, dst, e->iter->result);
        else if (dst->kind != QE_NAME || dst->var != e->iter->acc)
            qcg_into(g, dst, &acc);

        return;
    }

    default: /* a call's */
        if (e->builtin == QB_GET) {
            qcg_get(g, dst, e);
            return;
        }

        if (e->builtin == QB_SLICE) { /* the ends first: they may move the heap */
            qcg_ends(g, e);
            qcg_cell(g, e->a);
            qcg_cell(g, dst);
            qcg_x(g->c, QX_LSLICE);
            return;
        }

        if (e->builtin == QB_COPY) {
            if (qcg_list(g, e->type))
                qcg_listto(g, dst, e->a, false);
            else
                qcg_copyrec(g, dst, e->a);

            return;
        }

        qcg_call(g, e, dst);
    }
}

/* Record or list value e to place dst: built there, or in its temporary first; a place's
   copied, a list variable's moved (sema saw it is not used again). */
static void qcg_into(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    qcg_place(g, dst, e);
    qcg_moved(g, e);
}

/* qcg_into's value e to place dst. */
static void qcg_place(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    if (qast_fresh(e) && e->var) {
        const QExpr t = qcg_settle(g, e);

        if (qcg_list(g, e->type))
            qcg_listto(g, dst, &t, true);
        else
            qcg_copyrec(g, dst, &t);

        return;
    }

    if (qast_fresh(e)) {
        qcg_build(g, dst, e);
        return;
    }

    if (qcg_list(g, e->type))
        qcg_listto(g, dst, e, e->kind == QE_NAME && qcg_ty(g, e->type)->owns);
    else
        qcg_copyrec(g, dst, e);
}

static void qcg_expr(QCg *const g, const QExpr *const e)
{
    switch (e->kind) {
    case QE_CONST:
        qcg_const(g, e, e->type);
        return;

    case QE_NAME:
        qcg_load(g->c, e->var);
        return;

    case QE_UNARY:
        qcg_unary(g, e);
        return;

    case QE_BINARY:
        qcg_binary(g, e);
        return;

    case QE_CHAIN:
        qcg_chain(g, e, 0);
        return;

    case QE_AND:
    case QE_OR:
        qcg_logic(g, e);
        return;

    case QE_COND:
        qcg_cond(g, e);
        return;

    case QE_IN:
        qcg_in(g, e);
        return;

    case QE_CALL:
        if (e->builtin == QB_LEN && e->a->type == QY_STRING) {
            qcg_expr(g, e->a);
            code_call(g->c, K_FN_LEN);
            return;
        }

        if (e->builtin == QB_LEN) {
            qcg_cell(g, e->a);
            qcg_x(g->c, QX_LEN);
            return;
        }

        if (e->builtin == QB_BYTE) {
            qcg_expr(g, e->a);
            qcg_expr(g, e->b);
            qcg_x(g->c, QX_BYTE);
            return;
        }

        if (e->builtin == QB_SLICE && e->type == QY_STRING) {
            qcg_expr(g, e->a);
            qcg_ends(g, e);
            qcg_x(g->c, QX_SLICE);
            return;
        }

        qcg_call(g, e, nullptr);
        return;

    case QE_CONV:
        qcg_expr(g, e->a);
        qcg_conv(g->c, e->a->type, e->type, e->wrap);
        return;

    case QE_FIELD:
    case QE_INDEX:
        qcg_loadat(g, e);
        return;

    case QE_IS: { /* T?'s has, T!'s err */
        const QExpr f = qcg_fieldof(g, e->a, QF_FLAG);
        qcg_loadat(g, &f);

        if ((e->op == QK_NONE) != e->neg)
            code_op(g->c, O_I_NOT);

        return;
    }

    case QE_TRY: {
        const QExpr t = qcg_tried(g, e);
        const QExpr v = qcg_fieldof(g, &t, QF_VAL);
        qcg_loadat(g, &v);
        return;
    }

    case QE_LIST:
        qcg_listop(g, e, nullptr);
        return;

    case QE_HAS:
        qcg_has(g, e);
        return;

    case QE_FSTR:
        qcg_fstring(g, e);
        return;

    case QE_APPLY:
        qcg_applied(g, e);
        qcg_expr(g, e->a);
        return;

    case QE_ITER:
        qcg_iter(g, e);
        qcg_expr(g, e->iter->result);
        return;

    case QE_FNREF:
        code_int(g->c, e->fn->proc);
        return;

    default: /* a record's, a tuple's: built where it is wanted */
        return;
    }
}

/* ---- chains */

static void qcg_zero(QCg *g, QType t);
static void qcg_fresh(QCg *g, QVar *v);

/* A chain's loop's exits: to its next element and past its end. A for's are jumps to its end
   for's start and end; an expression's, hops within the line, landed after. */
typedef struct {
    const QStmt *end; /* the for's end for; null an expression's */
    QCgHop *hops;
    bool *done; /* each hop's: past the end, else to the next element */
    uint32_t n, cap;
} QCgExits;

static void qcg_leave(QCg *const g, QCgExits *const x, const QrtWhen when, const bool done)
{
    static const uint8_t ops[] = {[QJ_ALWAYS] = O_JMP, [QJ_FALSE] = O_I_JF, [QJ_TRUE] = O_I_JT};

    if (x->end) {
        qcg_to(g, ops[when], x->end, done ? BOFF : 0, TO_STMT);
        return;
    }

    uint32_t cap = x->cap;
    x->hops = qast_room(g->mem, x->hops, x->n, &x->cap, sizeof *x->hops);
    x->done = qast_room(g->mem, x->done, x->n, &cap, sizeof *x->done);
    x->done[x->n] = done;
    x->hops[x->n++] = qcg_hop(g, when);
}

/* x's exits to the next element (done false) or past the end land here. */
static void qcg_exits(QCg *const g, const QCgExits *const x, const bool done)
{
    for (uint32_t k = 0; k < x->n; k++)
        if (x->done[k] == done)
            qcg_land(g, x->hops[k]);
}

/* A loop's start here: a label's body; with strings on the stack, the code's offset. */
static uint32_t qcg_top(QCg *const g)
{
    qcg_flush(g);

    if (g->strings)
        return (uint32_t)(g->c->out - g->c->buf);

    const uint32_t body = ++g->hops;
    qcg_land(g, (QCgHop){.body = body});

    return body;
}

/* A jump back to qcg_top's top when; with strings on the stack, a QJ. */
static void qcg_back(QCg *const g, const QrtWhen when, const uint32_t top)
{
    static const uint8_t ops[] = {[QJ_ALWAYS] = O_JMP, [QJ_FALSE] = O_I_JF, [QJ_TRUE] = O_I_JT};

    Code *const c = g->c;

    if (!g->strings) {
        code_op(c, ops[when]);
        code_dword(c, g->line);
        code_dword(c, top);
        code_dword(c, TO_STMT);
        return;
    }

    qcg_flush(g);
    uint8_t *const at = qcg_qj(c, when);

    if (at) {
        const int32_t rel = (int32_t)top - (int32_t)(at + 4 + 2 - c->buf);
        store32(at, (uint32_t)(rel * (1 << QJ_SHIFT)) | load32(at));
    }

    qcg_flush(g);
}

/* Counter i stepped by stp: I.STEP.V, or a resumable's, whose cells are refs, an add. */
static void qcg_step(Code *const c, QVar *const i, QVar *const stp)
{
    qcg_load(c, i);

    if (qcg_par(stp)) {
        qcg_load(c, stp);
        code_op(c, O_I_ADD);
    } else
        code_opid(c, O_I_STEP_V, stp->id, false);

    qcg_store(c, i);
}

/* v += by. */
static void qcg_bump(Code *const c, QVar *const v, const uint32_t by)
{
    qcg_load(c, v);
    uint8_t *const right = c->out;
    code_int(c, by);
    qcg_arith(c, right, QY_INT, QA_ADD);
    qcg_store(c, v);
}

/* The flag of chain it's counter past its last: entered, the last counted. */
static void qcg_beyond(QCg *const g, const QIter *const it)
{
    qcg_load(g->c, it->i);
    uint8_t *const right = g->c->out;
    qcg_load(g->c, it->lim);
    qcg_compare(g->c, QY_INT, it->rev ? QK_LT : QK_GT, right);
}

static void qcg_keptinit(QCg *g, QVar *v, const QExpr *call);

/* Chain it's start: its source held, its counter from its first, each step's and its end's
   state; on to its end when its source is empty. */
static void qcg_chainstart(QCg *const g, const QIter *const it, QCgExits *const x)
{
    Code *const c = g->c;

    if (it->hold) {
        const QExpr h = qcg_named(it->hold);
        qcg_set(g, &h, it->src);
    } else if (it->src)
        qcg_settle(g, it->src);

    if (it->start)
        qcg_keptinit(g, it->gen, it->start);

    if (it->from != QO_GEN) { /* i from its first, lim its last, stp the way */
        for (uint32_t k = 0; k < 2; k++) {
            if ((k == 0) == it->rev) {
                qcg_expr(g, it->hi);
                uint8_t *const right = c->out;
                code_int(c, 1);
                qcg_arith(c, right, QY_INT, QA_SUB);
            } else
                qcg_expr(g, it->lo);

            qcg_store(c, k ? it->lim : it->i);
        }

        code_int(c, it->rev ? (uint32_t)-1 : 1);
        qcg_store(c, it->stp);
    }

    for (uint32_t k = 0; k < it->n; k++) {
        const QStage *const st = &it->stages[k];

        if (st->kind == QI_TAKE || st->kind == QI_SKIP) {
            qcg_expr(g, st->e);
            qcg_store(c, st->n);
            code_int(c, 0);
            qcg_store(c, st->k);
        } else if (st->kind == QI_ZIP) { /* n from b's first, k its last */
            if (st->of->type == QY_INT)
                qcg_expr(g, st->of);
            else
                code_int(c, 0);

            qcg_store(c, st->n);
            qcg_expr(g, st->to);
            uint8_t *const right = c->out;
            code_int(c, 1);
            qcg_arith(c, right, QY_INT, QA_SUB);
            qcg_store(c, st->k);
        }
    }

    switch (it->sink) {
    case QZ_SUM:
    case QZ_COUNT:
    case QZ_ANY:
    case QZ_ALL:
        if (it->sink == QZ_ALL)
            code_int(c, 1);
        else
            qcg_zero(g, it->acc->type);

        qcg_store(c, it->acc);
        break;

    case QZ_MIN:
    case QZ_MAX:
    case QZ_FIND:
        code_int(c, 0);
        qcg_store(c, it->has);
        break;

    case QZ_FOLD: {
        const QExpr a = qcg_named(it->acc);
        qcg_set(g, &a, it->init);
        break;
    }

    case QZ_COLLECT:
        qcg_fresh(g, it->acc);
        break;

    default:
        break;
    }

    if (it->from != QO_GEN) {
        qcg_beyond(g, it);
        qcg_leave(g, x, QJ_TRUE, true);
    }
}

/* Chain it's element, each time: its source's, through its steps. */
static void qcg_element(QCg *const g, const QIter *const it, QCgExits *const x)
{
    Code *const c = g->c;

    if (it->from == QO_GEN) {
        const QExpr got = qcg_named(it->got);
        qcg_set(g, &got, it->next);
        const QExpr has = qcg_fieldof(g, &got, QF_FLAG);
        qcg_expr(g, &has);
        qcg_leave(g, x, QJ_FALSE, true);
    }

    for (uint32_t k = 0; k < it->n; k++) {
        const QStage *const st = &it->stages[k];

        switch (st->kind) {
        case QI_MAP:
            if (st->x) {
                const QExpr v = qcg_named(st->x);
                qcg_set(g, &v, st->e);
            }

            break;

        case QI_FILTER:
            qcg_expr(g, st->e);
            qcg_leave(g, x, QJ_FALSE, false);
            break;

        case QI_TAKE: {
            qcg_load(c, st->k);
            uint8_t *const right = c->out;
            qcg_load(c, st->n);
            qcg_compare(c, QY_INT, QK_GE, right);
            qcg_leave(g, x, QJ_TRUE, true);
            qcg_bump(c, st->k, 1);
            break;
        }

        case QI_SKIP: {
            qcg_load(c, st->k);
            uint8_t *const right = c->out;
            qcg_load(c, st->n);
            qcg_compare(c, QY_INT, QK_LT, right);
            const QCgHop past = qcg_hop(g, QJ_FALSE);
            qcg_bump(c, st->k, 1);
            qcg_leave(g, x, QJ_ALWAYS, false);
            qcg_land(g, past);
            break;
        }

        case QI_ZIP: {
            qcg_load(c, st->n);
            uint8_t *const right = c->out;
            qcg_load(c, st->k);
            qcg_compare(c, QY_INT, QK_GT, right);
            qcg_leave(g, x, QJ_TRUE, true);
            const QExpr v = qcg_named(st->x);
            qcg_set(g, &v, st->e);
            qcg_bump(c, st->n, 1);
            break;
        }
        }
    }
}

/* Chain it's element used up by its end: kept, counted, tested. */
static void qcg_used(QCg *const g, const QIter *const it, QCgExits *const x)
{
    Code *const c = g->c;
    const QExpr acc = it->acc ? qcg_named(it->acc) : (QExpr){};

    const QExpr *const v = it->parts[0];

    switch (it->sink) {
    case QZ_SUM: {
        const QType t = it->acc->type;
        qcg_load(c, it->acc);
        uint8_t *const right = c->out;
        qcg_as(g, v, t);

        if (qast_integer(t) && t != QY_INT)
            qcg_iarith(g, right, t, QK_PLUS, false, 0);
        else
            qcg_arith(c, right, t, QA_ADD);

        qcg_store(c, it->acc);
        return;
    }

    case QZ_COUNT:
        if (it->pred) {
            qcg_expr(g, it->pred);
            qcg_leave(g, x, QJ_FALSE, false);
        }

        qcg_bump(c, it->acc, 1);
        return;

    case QZ_ANY:
    case QZ_ALL:
        qcg_expr(g, it->pred);
        qcg_leave(g, x, it->sink == QZ_ANY ? QJ_FALSE : QJ_TRUE, false);
        code_int(c, it->sink == QZ_ANY);
        qcg_store(c, it->acc);
        qcg_leave(g, x, QJ_ALWAYS, true);
        return;

    case QZ_FIND:
        qcg_expr(g, it->pred);
        qcg_leave(g, x, QJ_FALSE, false);
        qcg_set(g, &acc, v);
        code_int(c, 1);
        qcg_store(c, it->has);
        qcg_leave(g, x, QJ_ALWAYS, true);
        return;

    case QZ_MIN:
    case QZ_MAX: { /* the first, or one past acc */
        qcg_load(c, it->has);
        const QCgHop first = qcg_hop(g, QJ_FALSE);
        qcg_expr(g, v);
        const uint32_t held = v->type == QY_STRING;
        g->strings += held;
        uint8_t *const right = c->out;
        qcg_load(c, it->acc);
        g->strings -= held;
        qcg_compare(c, v->type, it->sink == QZ_MIN ? QK_LT : QK_GT, right);
        qcg_leave(g, x, QJ_FALSE, false);
        qcg_land(g, first);
        qcg_set(g, &acc, v);
        code_int(c, 1);
        qcg_store(c, it->has);
        return;
    }

    case QZ_FOLD:
        qcg_set(g, &acc, it->pred);
        return;

    case QZ_COLLECT:
        qcg_expr(g, it->push);
        return;

    case QZ_FOR: /* each name's part, unless it names it where it is */
        for (uint32_t k = 0; k < it->nparts; k++)
            if (!it->xs[k]->place) {
                const QExpr p = qcg_named(it->xs[k]);
                qcg_set(g, &p, it->parts[k]);
            }

        return;
    }
}

/* Chain it's loop's end: on to the next element, its counter stepped, back to its start. */
static void qcg_again(QCg *const g, const QIter *const it, const uint32_t line)
{
    Code *const c = g->c;

    if (it->from != QO_GEN) {
        qcg_step(c, it->i, it->stp);
        qcg_beyond(g, it);
    }

    const QrtWhen when = it->from == QO_GEN ? QJ_ALWAYS : QJ_FALSE;

    if (line == g->line) {
        qcg_back(g, when, it->top);
        return;
    }

    static const uint8_t ops[] = {[QJ_ALWAYS] = O_JMP, [QJ_FALSE] = O_I_JF};

    code_op(c, ops[when]);
    code_dword(c, line);
    code_dword(c, it->top);
    code_dword(c, TO_STMT);
}

/* Chain expression e's loop, its value in its acc. */
static void qcg_iter(QCg *const g, const QExpr *const e)
{
    QIter *const it = e->iter;
    QCgExits x = {};

    qcg_chainstart(g, it, &x);
    it->top = qcg_top(g);
    qcg_element(g, it, &x);
    qcg_used(g, it, &x);
    qcg_exits(g, &x, false);
    qcg_again(g, it, g->line);
    qcg_exits(g, &x, true);
}

/* for x in a chain, s: its loop's start and element on the for's line, its body after. */
static void qcg_chainfor(QCg *const g, const QStmt *const s)
{
    QIter *const it = s->e->iter;
    QCgExits x = {.end = s->link};

    qcg_chainstart(g, it, &x);
    it->top = qcg_top(g);
    qcg_element(g, it, &x);
    qcg_used(g, it, &x);
}

/* ---- generators */

static bool qcg_isgen(const QCg *const g, const QType t)
{
    return qast_compound(t) && qcg_ty(g, t)->kind == QT_GEN;
}

static void qcg_pret(Code *const c, const QFn *const f)
{
    code_op(c, O_PRET);
    code_dword(c, f->proc);
    code_dword(c, 0);
}

/* var v = g(..), call: each argument kept in its host's variable, v from the start. */
static void qcg_keptinit(QCg *const g, QVar *const v, const QExpr *const call)
{
    const QFn *const r = qcg_ty(g, v->type)->gen;

    for (uint32_t k = 0; k < call->nargv; k++)
        if (!r->params[k].hidden && !r->params[k].lam)
            qcg_set(g, r->keep[k], call->argv[k]);

    code_int(g->c, 0);
    qcg_store(g->c, v);
}

/* On from where generator f stopped: to the yield its resume index names (a sole yield's
   straight); a resumable's done (-1) to its end, 0 on into its body. */
static void qcg_resume(QCg *const g, const QFn *const f)
{
    Code *const c = g->c;
    const uint32_t n = f->nyields;

    for (uint32_t k = 0; k < n; k++) {
        if (k + 1 == n && !f->resumable) {
            qcg_to(g, O_JMP, f->yields[k]->link, BOFF, TO_STMT);
            return;
        }

        qcg_load(c, f->resume);
        uint8_t *const right = c->out;
        code_int(c, k + 1);
        qcg_compare(c, QY_INT, QK_NE, right);
        qcg_to(g, O_I_JF, f->yields[k]->link, BOFF, TO_STMT);
    }

    if (f->resumable) {
        qcg_load(c, f->resume);
        uint8_t *const right = c->out;
        code_int(c, 0);
        qcg_compare(c, QY_INT, QK_GE, right);
        qcg_to(g, O_I_JF, f->end, 0, TO_STMT);
    }
}

/* yield v: its number the resume index (when it needs one), v the loop's variable or next()'s
   result; then to the loop's body, or back to next()'s caller. */
static void qcg_yield(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;
    const QFn *const f = s->fn;

    if (f->resume) {
        code_int(c, (uint32_t)s->by);
        qcg_store(c, f->resume);
    }

    if (f->resumable) {
        const QExpr res = qcg_named(f->res);
        qcg_set(g, &res, s->e);
        qcg_pret(c, f);
        return;
    }

    const QStmt *const loop = f->loop;

    if (!loop->var->place) {
        const QExpr x = qcg_named(loop->var);
        qcg_set(g, &x, s->e);
    }

    qcg_to(g, O_JMP, loop, BOFF, TO_STMT);
}

/* Generator f's return: past its loop, or to its end. */
static void qcg_done(QCg *const g, const QFn *const f)
{
    if (f->resumable)
        qcg_to(g, O_JMP, f->end, 0, TO_STMT);
    else
        qcg_to(g, O_JMP, f->loop->link, BOFF, TO_STMT);
}

/* Generator s->fn's end: past its loop; a resumable's done, next() none from now on. */
static void qcg_ended(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;
    const QFn *const f = s->fn;

    if (!f->resumable) {
        qcg_done(g, f);
        return;
    }

    code_int(c, (uint32_t)-1);
    qcg_store(c, f->resume);
    const QExpr res = qcg_named(f->res);
    qcg_set(g, &res, s->e);
    qcg_pret(c, f);
}

/* for x in g(..): g's parameters given their values, then into its body. */
static void qcg_genfor(QCg *const g, const QStmt *const s)
{
    const QFn *const f = s->fn;

    for (uint32_t k = 0; k < f->npar; k++) {
        QVar *const v = f->params[k].var;

        if (!f->params[k].lam && !f->params[k].hidden && !v->constant && !v->place) {
            const QExpr p = qcg_named(v);
            qcg_set(g, &p, s->e->argv[k]);
        }
    }

    qcg_to(g, O_JMP, f->def, BOFF, TO_STMT);
}

/* ---- statements */

/* A variable's zero: 0, 0.0, false, "". */
static void qcg_zero(QCg *const g, const QType t)
{
    const QExpr zero = {.kind = QE_CONST, .type = t};

    qcg_const(g, &zero, t);
}

/* A clause's target from the one before it: past an elseif's or else's JMP, or the end if. */
static void qcg_clause(QCg *const g, const uint8_t op, const QStmt *const next)
{
    const JumpMode mode = next->kind == QS_ENDIF || next->kind == QS_ENDMATCH ? TO_STMT : PAST_JMP;
    qcg_to(g, op, next, 0, mode);
}

/* The end if of the if whose clause s is. */
static const QStmt *qcg_endif(const QStmt *s)
{
    while (s->kind != QS_ENDIF)
        s = s->link;

    return s;
}

/* For s's counter: a range's, a list's index. */
static QVar *qcg_counter(const QStmt *const s) { return s->each ? s->index : s->var; }

/* The flag of for s's counter past its limit, its step's sign known (by) or not (I.PAST).
   entered: the limit inclusive, as end for makes it; else the range's end, past when reached. */
static void qcg_past(QCg *const g, const QStmt *const s, const bool entered)
{
    Code *const c = g->c;
    qcg_load(c, qcg_counter(s));

    if (s->by) {
        uint8_t *const right = c->out;
        qcg_load(c, s->lim);
        qcg_compare(
            c,
            QY_INT,
            s->by > 0 ? (entered ? QK_GT : QK_GE) : (entered ? QK_LT : QK_LE),
            right
        );
        return;
    }

    qcg_load(c, s->lim);
    qcg_load(c, s->stp);
    code_op(c, O_I_PAST);

    if (entered)
        return;

    qcg_load(c, qcg_counter(s));
    uint8_t *const right = c->out;
    qcg_load(c, s->lim);
    qcg_compare(c, QY_INT, QK_EQ, right);
    code_op(c, O_I_OR);
}

/* For s's limit made inclusive, the loop entered: the range's end less the step's sign. */
static void qcg_inclusive(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;
    qcg_load(c, s->lim);

    if (s->by) {
        uint8_t *const right = c->out;
        code_int(c, 1);
        qcg_arith(c, right, QY_INT, s->by > 0 ? QA_SUB : QA_ADD);
    } else { /* compares give -1 or 0: lim + (stp > 0) - (stp < 0) */
        for (uint32_t k = 0; k < 2; k++) {
            qcg_load(c, s->stp);
            uint8_t *const right = c->out;
            code_int(c, 0);
            qcg_compare(c, QY_INT, k ? QK_LT : QK_GT, right);
            code_op(c, k ? O_I_SUB : O_I_ADD);
        }
    }

    qcg_store(c, s->lim);
}

/* Variable v, declared with no value: 0, "", an empty list, an array's elements 0, a record's
   fields their defaults. */
static void qcg_fresh(QCg *const g, QVar *const v)
{
    Code *const c = g->c;
    const QExpr p = qcg_named(v);

    if (qcg_rec(g, v->type)) {
        qcg_clear(g, &p);
        qcg_defaults(g, &p);
        return;
    }

    if (!qcg_list(g, v->type)) {
        qcg_zero(g, v->type);
        qcg_store(c, v);
        return;
    }

    const QTy *const y = qcg_ty(g, v->type);

    if (y->kind == QT_ARRAY) { /* REDIM v(0 TO count - 1): DIM's ( lo hi mlo mhi id flags ) */
        code_int(c, 0);
        code_int(c, y->count - 1);
        code_int(c, 0);
        code_int(c, 0);
        code_int(c, v->id);
        code_int(c, DIM_REDIM | 1);
        code_call(c, K_DIM);
        return;
    }

    qcg_cell(g, &p); /* its old elements freed, then empty */
    code_call(c, K_ERASE);
    code_int(c, v->id);
    code_int(c, 0);
    code_call(c, K_AEMPTY);
}

/* let q, r = v: v's fields to the names. */
static void qcg_destructure(QCg *const g, const QStmt *const s)
{
    const QDecl *last = s->decls;

    while (last->next)
        last = last->next;

    const QExpr *const v = last->value;
    const QExpr t = qast_fresh(v) ? qcg_settle(g, v) : *v;
    uint32_t k = 0;

    for (const QDecl *d = s->decls; d; d = d->next, k++) {
        const QExpr to = qcg_named(d->var);
        const QExpr f = qcg_fieldof(g, &t, k);

        if (d->var->live)
            qcg_dropvar(g, d->var);

        qcg_set(g, &to, &f);

        if (d->var->live)
            qcg_live(g, d->var, true);
    }
}

/* a, b = x, y: the values on the stack, stored in turn from the last; or each in its temporary
   first. a, b = f(): its fields in turn. */
static void qcg_parallel(QCg *const g, const QStmt *const s)
{
    const QExpr *const to = s->target;
    const QExpr *const v = s->e;

    if (v->kind != QE_TUPLE) {
        const QExpr t = qast_fresh(v) ? qcg_settle(g, v) : *v;

        for (uint32_t k = 0; k < to->n; k++) {
            const QExpr f = qcg_fieldof(g, &t, k);
            qcg_set(g, to->items[k], &f);
        }

        return;
    }

    if (!s->temps) {
        for (uint32_t k = 0; k < to->n; k++) {
            qcg_as(g, v->items[k], to->items[k]->type);
            qcg_hold(g, to->items[k]->type, 1);
        }

        for (uint32_t k = to->n; k-- > 0;) {
            qcg_hold(g, to->items[k]->type, -1);
            qcg_store(g->c, to->items[k]->var);
        }

        return;
    }

    for (uint32_t k = 0; k < to->n; k++) {
        const QExpr t = qcg_named(s->temps[k]);
        qcg_set(g, &t, v->items[k]);
    }

    for (uint32_t k = 0; k < to->n; k++) {
        const QExpr t = qcg_named(s->temps[k]);
        qcg_set(g, to->items[k], &t);
    }
}

/* Place p, holding a value with drop, given v: v made first, p's old value dropped, then v moved
   in. */
static void qcg_replace(QCg *const g, const QExpr *const p, const QExpr *const v)
{
    const QExpr t = qast_fresh(v) ? qcg_settle(g, v) : *v;
    qcg_dropold(g, p);
    qcg_into(g, p, &t);

    if (p->kind == QE_NAME && p->var->live)
        qcg_live(g, p->var, true);
}

/* ---- --test */

/* A LineRef to statement s's line: ON LOCAL ERROR's handler, RESUME's line. */
static void qcg_lineref(QCg *const g, const uint8_t op, const QStmt *const s)
{
    code_op(g->c, op);

    if (op == O_RESUME)
        code_byte(g->c, RESUME_LINE);

    code_word(g->c, NONUM);
    code_dword(g->c, g->env->lines[s->line].at);
}

/* Test s->fn run, its name said when it returns unfailed. */
static void qcg_testrun(QCg *const g, const QStmt *const s)
{
    const QProg *const p = g->env->prog;
    const QPiece name = {.lit = s->fn->name};

    Code *const c = g->c;
    code_int(c, 0);
    qcg_store(c, p->failed);
    qcg_pcall(g, s->fn);
    qcg_load(c, p->failed);
    const QCgHop out = qcg_hop(g, QJ_TRUE);
    qcg_bump(c, p->passes, 1);
    qcg_string(g, &name);
    qcg_x(c, QX_PASSED);
    qcg_land(g, out);
}

/* Test s->fn's error handler, in its frame: its failure said, then on at its end. */
static void qcg_testfail(QCg *const g, const QStmt *const s)
{
    const QProg *const p = g->env->prog;
    const QPiece name = {.lit = s->fn->name};

    code_int(g->c, 1);
    qcg_store(g->c, p->failed);
    qcg_bump(g->c, p->fails, 1);
    qcg_string(g, &name);
    qcg_x(g->c, QX_FAILED);
    qcg_lineref(g, O_RESUME, s->fn->end);
}

/* A statement a one-line if may take, or one that is no block's. */
static void qcg_simple(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;

    switch (s->kind) {
    case QS_DECL:
        if (s->op == QK_CONST)
            return;

        if (s->decls->next && !s->decls->value && !s->decls->type) {
            qcg_destructure(g, s);
            return;
        }

        for (const QDecl *d = s->decls; d; d = d->next) {
            const QExpr to = qcg_named(d->var);

            if (d->var->live) /* a loop's declaration run again */
                qcg_dropvar(g, d->var);

            if (qcg_isgen(g, d->var->type))
                qcg_keptinit(g, d->var, d->value);
            else if (d->value)
                qcg_set(g, &to, d->value);
            else
                qcg_fresh(g, d->var);

            if (d->var->live)
                qcg_live(g, d->var, true);
        }

        return;

    case QS_ASSIGN:
        for (const QStmt *p = s->pre; p; p = p->next)
            qcg_set(g, p->target, p->e);

        if (s->target->kind == QE_TUPLE)
            qcg_parallel(g, s);
        else if (qcg_drops(g, s->target->type))
            qcg_replace(g, s->target, s->e);
        else
            qcg_set(g, s->target, s->e);

        return;

    case QS_CALL:
        if (qcg_byplace(g, s->e->type) && s->e->var) {
            const QExpr t = qcg_settle(g, s->e);

            if (qcg_drops(g, t.type))
                qcg_drop(g, &t, 0);

            return;
        }

        if (s->e->kind == QE_LIST && s->e->builtin == QB_POP && qcg_byplace(g, s->e->type)) {
            qcg_listop(g, s->e, nullptr);
            return;
        }

        qcg_expr(g, s->e);

        if (s->e->type != QY_VOID)
            code_op(c, O_DROP);

        return;

    case QS_BREAK:
        qcg_to(g, O_JMP, s->link->link, BOFF, TO_STMT);
        return;

    case QS_YIELD:
        qcg_yield(g, s);
        return;

    case QS_ASSERT: {
        qcg_expr(g, s->e);
        const QCgHop ok = qcg_hop(g, QJ_TRUE);
        qcg_x(c, QX_ASSERT);
        qcg_land(g, ok);
        return;
    }

    case QS_CONTINUE: {
        const QStmt *const loop = s->link;

        if (loop->kind == QS_WHILE)
            qcg_to(g, O_JMP, loop, 0, TO_STMT);
        else if (loop->kind == QS_FOR)
            qcg_to(g, O_JMP, loop->link, 0, TO_STMT);
        else
            qcg_to(g, O_JMP, loop, BOFF, TO_STMT);

        return;
    }

    case QS_RETURN:
        if (s->fn->ytx) {
            qcg_done(g, s->fn);
            return;
        }

        if (s->e) {
            const QExpr res = qcg_named(s->fn->res);
            qcg_set(g, &res, s->e);
        }

        qcg_dropall(g, &s->fn->own);
        code_op(c, O_PRET);
        code_dword(c, s->fn->proc);
        code_dword(c, 0);
        return;

    default:
        return;
    }
}

/* A statement a debugger stops at: one that does something, not a block's end or else. */
static bool qcg_stops(const QStmt *const s)
{
    switch (s->kind) {
    case QS_DECL:
        return s->op != QK_CONST;

    case QS_ASSIGN:
    case QS_CALL:
    case QS_IF:
    case QS_WHILE:
    case QS_FOR:
    case QS_MATCH:
    case QS_CASE:
    case QS_ENDFOR:
    case QS_BREAK:
    case QS_CONTINUE:
    case QS_RETURN:
    case QS_YIELD:
        return true;

    default:
        return false;
    }
}

/* Match m's value, as its cases compare it: its variable's, its temporary's, its tag. */
static void qcg_subject(QCg *const g, const QStmt *const m)
{
    if (m->temp)
        qcg_load(g->c, m->temp);
    else
        qcg_expr(g, m->e);
}

/* match e: its value held, then to its first case's tests, past that line's JMP. */
static void qcg_match(QCg *const g, const QStmt *const s)
{
    if (s->place) {
        if (qast_fresh(s->e))
            qcg_settle(g, s->e);

        const QExpr tag = qcg_fieldof(g, s->place, 0);
        qcg_loadat(g, &tag);
        qcg_store(g->c, s->temp);
    } else if (s->temp) {
        qcg_expr(g, s->e);
        qcg_store(g->c, s->temp);
    }

    qcg_to(g, O_JMP, s->link, 0, PAST_JMP);
}

/* One pattern's flag: the value equal, in a to b, is op v; an enum's tag the value's. */
static void qcg_pattern(QCg *const g, const QStmt *const m, const QPat *const pt)
{
    Code *const c = g->c;
    const QType w = m->place ? QY_INT : m->ty;

    if (pt->kind == QP_VARIANT) {
        qcg_subject(g, m);
        uint8_t *const right = c->out;
        code_int(c, pt->variant);
        qcg_compare(c, QY_INT, QK_EQ, right);
        return;
    }

    for (uint32_t side = 0; side < (pt->kind == QP_RANGE ? 2u : 1u); side++) {
        qcg_subject(g, m);
        qcg_hold(g, w, 1);
        uint8_t *const right = c->out;
        qcg_as(g, side ? pt->b : pt->a, w);
        qcg_hold(g, w, -1);
        const QKind op = pt->kind == QP_IS      ? pt->op
                         : pt->kind == QP_RANGE ? (side ? QK_LE : QK_GE)
                                                : QK_EQ;
        qcg_compare(c, w, op, right);
    }

    if (pt->kind == QP_RANGE)
        code_op(c, O_I_AND);
}

/* case: the case before it done, to end match; then its patterns, any of them, else to the
   next case. */
static void qcg_case(QCg *const g, const QStmt *const s)
{
    const QStmt *end = s;

    while (end->kind != QS_ENDMATCH)
        end = end->link;

    qcg_to(g, O_JMP, end, BOFF, TO_STMT);

    if (!s->pats)
        return;

    for (const QPat *pt = s->pats; pt; pt = pt->next) {
        qcg_pattern(g, s->body, pt);

        if (pt != s->pats)
            code_op(g->c, O_I_OR);
    }

    qcg_clause(g, O_I_JF, s->link);
}

/* Statement s's code, unfused. */
static void qcg_body(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;

    switch (s->kind) {
    case QS_IF:
        qcg_expr(g, s->e);

        if (s->oneline) {
            qcg_to(g, O_I_JF, s, BOFF, TO_STMT);
            qcg_simple(g, s->body);
        } else
            qcg_clause(g, O_I_JF, s->link);

        return;

    case QS_ELSEIF:
        qcg_to(g, O_JMP, qcg_endif(s), BOFF, TO_STMT);
        qcg_expr(g, s->e);
        qcg_clause(g, O_I_JF, s->link);
        return;

    case QS_ELSE:
        qcg_to(g, O_JMP, qcg_endif(s), BOFF, TO_STMT);
        return;

    case QS_WHILE:
        qcg_expr(g, s->e);
        qcg_to(g, O_I_JF, s->link, BOFF, TO_STMT);
        return;

    case QS_ENDWHILE:
        qcg_to(g, O_JMP, s->link, 0, TO_STMT);
        return;

    case QS_ENDDO:
        qcg_to(g, O_JMP, s->link, BOFF, TO_STMT);
        return;

    case QS_FOR:
        if (s->fn) {
            qcg_genfor(g, s);
            return;
        }

        if (s->e->kind == QE_ITER) {
            qcg_chainfor(g, s);
            return;
        }

        if (s->each) { /* the index from 0, the limit the list's length */
            if (qast_fresh(s->e))
                qcg_settle(g, s->e);

            code_int(c, 0);
            qcg_store(c, s->index);

            if (qcg_ty(g, s->e->type)->kind == QT_ARRAY)
                code_int(c, qcg_ty(g, s->e->type)->count);
            else {
                qcg_cell(g, s->var->place->a);
                qcg_x(c, QX_LEN);
            }
        } else {
            qcg_expr(g, s->e);
            qcg_store(c, s->var);
            qcg_expr(g, s->to);
        }

        qcg_store(c, s->lim);

        if (s->by)
            code_int(c, (uint32_t)s->by);
        else
            qcg_expr(g, s->step);

        qcg_store(c, s->stp);
        qcg_past(g, s, false);
        qcg_to(g, O_I_JT, s->link, BOFF, TO_STMT);
        qcg_inclusive(g, s);
        return;

    case QS_ENDFOR: { /* NEXT's six ops (comp/fuse.c): I.NEXT.UP or I.NEXT.DN */
        const QStmt *const f = s->link;

        if (f->fn) {
            qcg_resume(g, f->fn);
            return;
        }

        if (f->e->kind == QE_ITER) {
            qcg_again(g, f->e->iter, g->env->lines[f->line].at);
            return;
        }

        qcg_step(c, qcg_counter(f), f->stp);
        qcg_past(g, f, true);
        qcg_to(g, O_I_JF, f, BOFF, TO_STMT);
        return;
    }

    case QS_FN:
        if (s->fn->native)
            return;

        qcg_to(g, O_JMP, s->fn->end, BOFF, TO_STMT);

        if (s->fn->resumable) /* a call's entry, past the JMP */
            qcg_resume(g, s->fn);
        if (s->fn->onfail)
            qcg_lineref(g, O_ONLERR, s->fn->onfail);

        return;


    case QS_MATCH:
        qcg_match(g, s);
        return;

    case QS_CASE:
        qcg_case(g, s);
        return;

    case QS_ENDFN:
        if (s->fn->ytx) {
            qcg_ended(g, s);
            return;
        }

        qcg_dropall(g, &s->fn->own);
        code_op(c, O_PRET);
        code_dword(c, s->fn->proc);
        code_dword(c, 0);
        return;

    case QS_ATEND:
        qcg_dropall(g, s->own);
        return;

    case QS_TESTRUN:
        qcg_testrun(g, s);
        return;

    case QS_TESTFAIL:
        qcg_testfail(g, s);
        return;

    case QS_TESTEND:
        qcg_load(c, g->env->prog->passes);
        qcg_load(c, g->env->prog->fails);
        qcg_x(c, QX_TALLY);
        return;

    default:
        qcg_simple(g, s);
    }
}

void qcg_stmt(QCg *const g, const QStmt *const s)
{
    g->seg = g->c->out;
    g->nlabels = g->hops = g->strings = 0;
    g->own = s->own;

    if (g->env->dbg && qcg_stops(s))
        code_op(g->c, O_DBG);

    qcg_body(g, s);
    qcg_flush(g);
}
