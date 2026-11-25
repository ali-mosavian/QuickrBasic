/* Quickr's code generator: values: an expression's code.
 *
 * # How it works
 * Every operand is pushed, then its op. A string literal loads from its line's text; and, or
 * and a if c else b jump within the line; an f-string joins its parts. A call is held as
 * QBasic's is (comp/code.h).
 */
#include "qr/qcg_int.h"
#include "num/double.h"

enum { TXT_MAX = 255 }; /* S.LD.TXT's length at most */

/* ---- values */

/* A string constant's text: each literal's runs from its line, its escapes CHR$, joined. */
void qcg_string(QCg *const g, const QPiece *s)
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
QrtKind qcg_kind(const QType t)
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

/* An integer of type from as integer type to, both in an int cell, an int's or a uint's bits:
   Overflow past to's range, by I.CHK8 or I.CHK16 where they check it; wrap keeps to's bits
   instead. */
static void qcg_narrow(Code *const c, const QType from, const QType to, const bool wrap)
{
    if (wrap ? qast_bits(to) == 32 : qcg_within(from, to))
        return;
    if (!wrap && to == QY_UBYTE)
        code_op(c, O_I_CHK8);
    else if (!wrap && to == QY_SHORT)
        code_op(c, O_I_CHK16);
    else
        qcg_nconv(c, from, to, wrap);
}

/* An integer of type from as integer type to, one of them a long or a ulong: an int cell's
   widened, a long's to an int checked; the rest by N.CONV. */
static void qcg_widen(Code *const c, const QType from, const QType to, const bool wrap)
{
    const bool fits = wrap || qcg_within(from, to);

    if (qast_wide(from) && qast_wide(to)) { /* the same bits */
        if (!fits)
            qcg_nconv(c, from, to, false);
    } else if (qast_wide(to) && !fits)
        qcg_nconv(c, from, to, false);
    else if (from == QY_UINT && qast_wide(to))
        code_xop(c, X_U_TOL);
    else if (qast_wide(to))
        code_op(c, O_I_TOL);
    else if (from == QY_LONG && to == QY_INT && !wrap)
        code_xop(c, X_L_TOI);
    else
        qcg_nconv(c, from, to, wrap);
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
        if (qast_wide(from) || qast_wide(to))
            qcg_widen(c, from, to, wrap);
        else
            qcg_narrow(c, from, to, wrap);

        return;
    }

    if (from == QY_LONG) { /* to a float */
        code_xop(c, X_L_TOD);

        if (to == QY_SINGLE)
            code_op(c, O_D_TOF);

        return;
    }

    if (qast_integer(from) && !qcg_icell(from)) { /* a uint or a ulong to a float */
        qcg_nconv(c, from, to, false);
        return;
    }

    if (qast_float(from) && to == QY_LONG && !wrap) {
        if (from == QY_SINGLE)
            code_op(c, O_F_TOD);

        code_xop(c, X_D_TOL);
        return;
    }

    if (qast_float(from) && qast_integer(to) && (to != QY_INT || wrap)) {
        qcg_nconv(c, from, to, wrap);
        return;
    }

    if (to == QY_INT) { /* int(x): truncated */
        if (from == QY_SINGLE)
            code_op(c, O_F_TOD);

        code_opb(c, O_D_FN, DF_FIX);
        code_xop(c, X_D_TOI);
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
void qcg_as(QCg *const g, const QExpr *const e, const QType w)
{
    if (e->kind == QE_CONST) {
        qcg_const(g, e, w);
        return;
    }

    qcg_expr(g, e);
    qcg_convert(g->c, e->type, w);
}

/* e's value pushed, held under what follows: a string counted (qcg_hop). */
void qcg_hold(QCg *const g, const QType t, const int32_t by)
{
    if (t == QY_STRING)
        g->strings += (uint32_t)by;
}

/* A long's + - * (QA_ADD .. QA_MUL), the right operand's load at right folded in when a
   variable's or a byte's. */
static void qcg_larith(Code *const c, uint8_t *const right, const uint32_t k)
{
    static const uint8_t var[] = {O_L_ADD_V, O_L_SUB_V, O_L_MUL_V};

    static const uint8_t byte[] = {X_L_ADD_U8, X_L_SUB_U8};

    if (code_ends_op(c, right, O_D_LD_V, 1 + VCELL)) {
        right[0] = var[k];
        return;
    }

    if (k != QA_MUL && code_ends_op(c, right, O_D_LD_K, 1 + sizeof(Cell)) &&
        load32(right + 1) <= UINT8_MAX && !load32(right + 5)) { /* the X op, its low byte kept */
        right[2] = right[1];
        right[0] = O_X;
        right[1] = byte[k];
        c->out = right + 3;
        return;
    }

    if (k == QA_SUB)
        code_xop(c, X_L_SUB);
    else
        code_op(c, k == QA_ADD ? O_L_ADD : O_L_MUL);
}

/* Operator op of a long or a ulong, w, its operands pushed. A shift's count is an int. */
static void qcg_warith(
    Code *const c,
    uint8_t *const right,
    const QType w,
    const QKind op,
    const bool wrap
)
{
    const bool uns = w == QY_ULONG;
    const uint32_t k = op == QK_PLUS ? QA_ADD : op == QK_MINUS ? QA_SUB : QA_MUL;

    switch (op) {
    case QK_PLUS:
    case QK_MINUS:
    case QK_STAR:
        if (wrap) {
            static const uint8_t xs[] =
                {[QA_ADD] = X_LW_ADD, [QA_SUB] = X_LW_SUB, [QA_MUL] = X_LW_MUL};

            code_xop(c, xs[k]);
        } else if (uns) {
            static const uint8_t xs[] =
                {[QA_ADD] = X_UL_ADD, [QA_SUB] = X_UL_SUB, [QA_MUL] = X_UL_MUL};

            code_xop(c, xs[k]);
        } else
            qcg_larith(c, right, k);

        return;

    case QK_DIV:
        if (wrap && !uns)
            qcg_xint(c, QI_DIV, w, true);
        else
            code_xop(c, uns ? X_UL_DIV : X_L_DIV);

        return;

    case QK_MOD:
        code_xop(c, uns ? X_UL_MOD : X_L_MOD);
        return;

    case QK_POW:
        qcg_xint(c, QI_POW, w, wrap);
        return;

    case QK_BAND:
        code_xop(c, X_L_AND);
        return;

    case QK_BOR:
        code_xop(c, X_L_OR);
        return;

    case QK_SHL:
        code_xop(c, X_L_SHL);
        return;

    case QK_SHR:
        code_xop(c, uns ? X_UL_SHR : X_L_SHR);
        return;

    default: /* xor */
        code_xop(c, X_L_XOR);
    }
}

/* Operator op of a uint, its operands pushed: and, or, xor and shl by the int ops. */
static void qcg_uarith(Code *const c, uint8_t *const right, const QKind op, const bool wrap)
{
    switch (op) {
    case QK_PLUS:
        code_xop(c, wrap ? X_IW_ADD : X_U_ADD);
        return;

    case QK_MINUS:
        code_xop(c, wrap ? X_IW_SUB : X_U_SUB);
        return;

    case QK_STAR:
        code_xop(c, wrap ? X_IW_MUL : X_U_MUL);
        return;

    case QK_DIV:
        code_xop(c, X_U_DIV);
        return;

    case QK_MOD:
        code_xop(c, X_U_MOD);
        return;

    case QK_POW:
        qcg_xint(c, QI_POW, QY_UINT, wrap);
        return;

    case QK_BAND:
        qcg_arith(c, right, QY_INT, QA_AND);
        return;

    case QK_BOR:
        qcg_arith(c, right, QY_INT, QA_OR);
        return;

    case QK_SHL:
        code_call(c, K_QSHL);
        return;

    case QK_SHR:
        code_xop(c, X_U_SHR);
        return;

    default:
        code_op(c, O_I_XOR);
    }
}

/* Operator op of integer type w other than an int's, or in wrap(..), its operands pushed: a long's,
   a ulong's, a uint's by their ops; an int cell's by the int ops, wrapped by IW's, then its range
   checked or its bits kept (qcg_narrow). A shift keeps w's bits; and, or, xor stay in w's range. */
void qcg_iarith(QCg *const g, uint8_t *const right, const QType w, const QKind op, const bool wrap)
{
    Code *const c = g->c;

    if (qast_wide(w)) {
        qcg_warith(c, right, w, op, wrap);
        return;
    }

    if (w == QY_UINT) {
        qcg_uarith(c, right, op, wrap);
        return;
    }

    static const uint8_t qa[] = {
        [QK_PLUS] = QA_ADD,
        [QK_MINUS] = QA_SUB,
        [QK_STAR] = QA_MUL,
        [QK_DIV] = QA_DIV,
        [QK_BAND] = QA_AND,
        [QK_BOR] = QA_OR
    };

    static const uint8_t iw[] = {[QA_ADD] = X_IW_ADD, [QA_SUB] = X_IW_SUB, [QA_MUL] = X_IW_MUL};

    switch (op) {
    case QK_PLUS:
    case QK_MINUS:
    case QK_STAR:
        if (wrap)
            code_xop(c, iw[qa[op]]);
        else
            qcg_arith(c, right, QY_INT, qa[op]);

        qcg_narrow(c, QY_INT, w, wrap);
        return;

    case QK_DIV:
        if (wrap && w == QY_INT) { /* -2^31 div -1 */
            qcg_xint(c, QI_DIV, w, true);
            return;
        }

        qcg_arith(c, right, QY_INT, QA_DIV);
        qcg_narrow(c, QY_INT, w, wrap);
        return;

    case QK_POW:
        if (wrap) {
            qcg_xint(c, QI_POW, w, true);
            return;
        }

        code_call(c, K_IPOW);
        qcg_narrow(c, QY_INT, w, false);
        return;

    case QK_MOD:
        code_op(c, O_I_MOD);
        return;

    case QK_BAND:
    case QK_BOR:
        qcg_arith(c, right, QY_INT, qa[op]);
        return;

    case QK_SHL:
        code_call(c, K_QSHL);

        if (w != QY_INT)
            qcg_narrow(c, QY_INT, w, true);

        return;

    case QK_SHR:
        code_call(c, K_QSHR);
        return;

    default:
        code_op(c, O_I_XOR);
    }
}

static void qcg_binary(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QType w = e->work;
    const bool shift = e->op == QK_SHL || e->op == QK_SHR;
    qcg_as(g, e->a, w);
    qcg_hold(g, w, 1);
    uint8_t *const right = c->out;
    qcg_as(g, e->b, shift ? QY_INT : w);
    qcg_hold(g, w, -1);

    if (qast_integer(w) && (w != QY_INT || e->wrap) && e->op != QK_SLASH) {
        qcg_iarith(g, right, w, e->op, e->wrap);
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
            code_xop(c, X_D_POW);
        else
            code_call(c, w == QY_INT ? K_IPOW : K_FPOW);

        return;

    case QK_DIV:
        qcg_arith(c, right, QY_INT, QA_DIV);
        return;

    case QK_MOD:
        code_op(c, O_I_MOD);
        return;

    case QK_BAND:
        qcg_arith(c, right, QY_INT, QA_AND);
        return;

    case QK_BOR:
        qcg_arith(c, right, QY_INT, QA_OR);
        return;

    case QK_SHL:
        code_call(c, K_QSHL);
        return;

    case QK_SHR:
        code_call(c, K_QSHR);
        return;

    default: /* xor */
        code_op(c, O_I_XOR);
    }
}

/* - or ~ of integer type t: all but an int cell's - outside wrap(..). */
static void qcg_iunary(Code *const c, const QType t, const QKind op, const bool wrap)
{
    if (op == QK_MINUS && qast_wide(t))
        code_xop(c, wrap ? X_LW_NEG : X_L_NEG);
    else if (op == QK_MINUS) { /* wrapped, as a uint's always is */
        code_xop(c, X_IW_NEG);
        qcg_narrow(c, QY_INT, t, true);
    } else if (qast_wide(t))
        code_xop(c, X_L_NOT);
    else if (qast_unsigned(t) && t != QY_UINT) { /* ~ of a ubyte or a ushort: its bits flipped */
        code_int(c, (1u << qast_bits(t)) - 1);
        code_op(c, O_I_XOR);
    } else
        code_op(c, O_I_NOT);
}

static void qcg_unary(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QType t = e->type;
    qcg_expr(g, e->a);

    if (qast_integer(t) && (!qcg_icell(t) || e->wrap || e->op != QK_MINUS))
        qcg_iunary(c, t, e->op, e->wrap);
    else if (e->op == QK_MINUS) {
        code_op(c, qcg_pick(t, O_I_NEG, O_F_NEG, O_D_NEG, 0));

        if (qast_integer(t))
            qcg_narrow(c, QY_INT, t, false);
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

/* f"..": its parts joined, each a literal's text, a value's string() or its spec's QX.FMT. */
static void qcg_fstring(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    uint32_t held = 0;

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

        qcg_hold(g, QY_STRING, 1);

        if (++held == UINT8_MAX || (k + 1 == e->n && held > 1)) {
            code_xop(c, X_S_JOIN);
            code_byte(c, (uint8_t)held);
            qcg_hold(g, QY_STRING, 1 - (int32_t)held);
            held = 1;
        }
    }

    qcg_hold(g, QY_STRING, -(int32_t)held);
}

void qcg_expr(QCg *const g, const QExpr *const e)
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
        if (e->builtin == QB_NONETEXT) {
            code_int(g->c, 0);
            code_int(g->c, QRT_NONE);
            code_call(g->c, K_QSTR);
            return;
        }

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

/* A variable's zero: 0, 0.0, false, "". */
void qcg_zero(QCg *const g, const QType t)
{
    const QExpr zero = {.kind = QE_CONST, .type = t};

    qcg_const(g, &zero, t);
}
