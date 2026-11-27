/* Quickr's semantic pass: expressions: operators, built-in calls and f-strings, and the dispatch.
 *
 * # How it works
 * qsem_expr checks an expression by its kind, each kind's check its own file's; an operator's
 * constants fold. A built-in call (print, a conversion) is checked here.
 */
#include "qr/qsem_int.h"

static QExpr *qsem_intop(QSem *S, QExpr *e);

/* ---- operators */

static bool qsem_arith(const QKind op)
{
    return op == QK_PLUS || op == QK_MINUS || op == QK_STAR || op == QK_SLASH || op == QK_POW;
}

/* An integer operator's (div, mod, bits) constant result in type w, as the VM's; false: an error
   said. */
static bool qsem_ifold(
    QSem *const S,
    const QExpr *const e,
    const int64_t a,
    const int64_t b,
    const QType w,
    int64_t *const r
)
{
    const uint32_t bits = qast_bits(w);
    const uint64_t ua = qast_unsigned(w) ? (uint64_t)a
                                         : (uint64_t)qsem_trunc(a, w) &
                                               (bits == 64 ? ~0ull : ((uint64_t)1 << bits) - 1);

    switch (e->op) {
    case QK_DIV:
    case QK_MOD:
        if (!b) {
            qsem_say(S, e->tok, "division by zero", nullptr);
            return false;
        }

        if (w == QY_ULONG) {
            *r = (int64_t)(e->op == QK_DIV ? (uint64_t)a / (uint64_t)b : (uint64_t)a % (uint64_t)b);
            return true;
        }

        if (a == INT64_MIN && b == -1) {
            qsem_say(S, e->tok, "the value doesn't fit ", qsem_aname(S, w), nullptr);
            return false;
        }

        *r = e->op == QK_DIV ? a / b : a % b;
        return true;

    case QK_BAND:
        *r = a & b;
        return true;

    case QK_BOR:
        *r = a | b;
        return true;

    case QK_BXOR:
        *r = a ^ b;
        return true;

    default: /* shl shr: the bits shifted out dropped */
        if (b < 0 || b >= bits) {
            qsem_say(S, e->tok, "a shift's count is 0 to ", qsem_uint(S, bits - 1), nullptr);
            return false;
        }

        if (e->op == QK_SHL)
            *r = qsem_trunc((int64_t)(ua << b), w);
        else
            *r = qast_unsigned(w) || a >= 0 ? (int64_t)((uint64_t)a >> b) : ~(~a >> b);

        return true;
    }
}

/* + - * ^ of integer constants of type w (QY_INT for untyped ones), exact; in wrap(..), w's bits
   kept. false: too large, said. */
static bool qsem_iarith(
    QSem *const S,
    const QExpr *const e,
    const int64_t a,
    const int64_t b,
    const QType w,
    int64_t *const r
)
{
    const bool u = w == QY_ULONG;
    bool over = false;
    uint64_t ur = 0;

    switch (e->op) {
    case QK_PLUS:
        over = u ? __builtin_add_overflow((uint64_t)a, (uint64_t)b, &ur)
                 : __builtin_add_overflow(a, b, r);
        break;

    case QK_MINUS:
        over = u ? __builtin_sub_overflow((uint64_t)a, (uint64_t)b, &ur)
                 : __builtin_sub_overflow(a, b, r);
        break;

    case QK_STAR:
        over = u ? __builtin_mul_overflow((uint64_t)a, (uint64_t)b, &ur)
                 : __builtin_mul_overflow(a, b, r);
        break;

    default: { /* ^: the VM's, 0 for a negative power but of 1 and -1 */
        uint64_t p = 1;
        const bool small = a == 0 || a == 1 || (!u && a == -1) || (!u && b < 0);

        if (small) /* |a| > 1 else: past 64 bits within 64 */
            p = (uint64_t)(a == 1 || !b ? 1 : a == -1 ? (b & 1 ? -1 : 1) : 0);

        for (uint64_t i = 0; !small && i < (uint64_t)b && !over; i++)
            over = u ? __builtin_mul_overflow(p, (uint64_t)a, &p)
                     : __builtin_mul_overflow((int64_t)p, a, (int64_t *)&p);

        ur = p;
        *r = (int64_t)p;
    }
    }

    if (u || e->op == QK_POW)
        *r = (int64_t)ur;
    if (e->wrap) {
        *r = qsem_trunc(*r, w);
        return true;
    }

    if (over)
        qsem_say(S, e->tok, "the value is too large", nullptr);

    return !over;
}

/* + - * / ^ of float constants in type t. */
static double qsem_ffold(const QKind op, const double a, const double b, const QType t)
{
    double r;

    switch (op) {
    case QK_PLUS:
        r = a + b;
        break;

    case QK_MINUS:
        r = a - b;
        break;

    case QK_STAR:
        r = a * b;
        break;

    case QK_SLASH:
        r = a / b;
        break;

    default: { /* ^ of a whole power; a fractional one is not folded */
        r = 1;
        const int64_t n = (int64_t)(b < 0 ? -b : b);

        for (int64_t i = 0; i < n; i++)
            r *= a;
        if (b < 0)
            r = 1 / r;
    }
    }

    return t == QY_SINGLE ? (double)(float)r : r;
}

/* a op b of numeric constants in type w, untyped when both were. */
static QExpr *qsem_afold(
    QSem *const S,
    QExpr *const e,
    QExpr *const a,
    QExpr *const b,
    const QType w
)
{
    const bool untyped = a->untyped && b->untyped;

    if (e->op == QK_SLASH && !qsem_num(b)) {
        qsem_say(S, e->tok, "division by zero", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (e->op == QK_POW && !qast_integer(w) && qsem_num(b) != (double)(int64_t)qsem_num(b)) {
        e->a = qsem_coerce(S, a, w); /* a fractional power: the VM's */
        e->b = qsem_coerce(S, b, w);
        e->type = e->work = w;
        return e;
    }

    QVal v = {};

    if (qast_integer(w)) {
        if (!qsem_iarith(S, e, a->val.i, b->val.i, w, &v.i)) {
            e->type = QY_ERROR;
            return e;
        }

        if (!untyped && !qsem_inrange(v.i, w == QY_ULONG, w)) {
            qsem_say(S, e->tok, "the value doesn't fit ", qsem_aname(S, w), nullptr);
            e->type = QY_ERROR;
            return e;
        }
    } else
        v.d = qsem_ffold(e->op, qsem_num(a), qsem_num(b), w);

    QExpr *const k = qsem_const(S, a->tok, w, v);
    k->untyped = untyped;
    return k;
}

/* a + b of strings: one constant when both are. */
static QExpr *qsem_strcat(QSem *const S, QExpr *const e)
{
    e->type = e->work = QY_STRING;

    if (!qsem_isconst(e->a) || !qsem_isconst(e->b))
        return e;

    return qsem_const(S, e->a->tok, QY_STRING, (QVal){.s = qsem_cat(S, e->a->val.s, e->b->val.s)});
}

/* a and b, a or b, a xor b, its operands checked, of two integers: bitwise, as op; of a bool and
   an integer, an error. Null: neither an integer, a logic operator's. */
static QExpr *qsem_bits(QSem *const S, QExpr *const e, const QKind op)
{
    const QType a = e->a->type, b = e->b->type;

    if (qast_integer(a) && qast_integer(b)) {
        e->kind = QE_BINARY;
        e->op = op;
        return qsem_intop(S, e);
    }

    if (!(qast_integer(a) && b == QY_BOOL) && !(a == QY_BOOL && qast_integer(b)))
        return nullptr;

    qsem_say(
        S,
        e->tok,
        qast_spelling(op),
        " takes two bools or two integers, not ",
        qsem_aname(S, a),
        " and ",
        qsem_aname(S, b),
        nullptr
    );
    e->type = QY_ERROR;
    return e;
}

/* a xor b: bools, or integers' bits. */
static QExpr *qsem_xor(QSem *const S, QExpr *const e)
{
    QExpr *const bits = qsem_bits(S, e, QK_BXOR);

    if (bits)
        return bits;

    e->a = qsem_coerce(S, e->a, QY_BOOL);
    e->b = qsem_coerce(S, e->b, QY_BOOL);
    e->type = e->work = e->a->type == QY_ERROR || e->b->type == QY_ERROR ? QY_ERROR : QY_BOOL;

    if (e->type == QY_BOOL && qsem_isconst(e->a) && qsem_isconst(e->b))
        return qsem_bool(S, e->tok, e->a->val.i != e->b->val.i);

    return e;
}

/* e's operands as integers of one type: div, mod and the bit operators; a shift's count an
   int. */
static QExpr *qsem_intop(QSem *const S, QExpr *const e)
{
    if (!qast_integer(e->a->type) || !qast_integer(e->b->type)) {
        const QExpr *const bad = !qast_integer(e->a->type) ? e->a : e->b;
        qsem_say(
            S,
            e->tok,
            qast_spelling(e->op),
            " takes integers, not ",
            qsem_aname(S, bad->type),
            nullptr
        );
        e->type = QY_ERROR;
        return e;
    }

    const bool shift = e->op == QK_SHL || e->op == QK_SHR;
    const QType w = shift ? (e->a->untyped ? QY_INT : e->a->type) : qsem_meet(e->a, e->b);
    e->a = qsem_coerce(S, e->a, w);
    e->b = qsem_coerce(S, e->b, shift ? QY_INT : w);
    e->type = e->work = w;

    if (e->a->type == QY_ERROR || e->b->type == QY_ERROR) {
        e->type = QY_ERROR;
        return e;
    }

    if (!qsem_isconst(e->a) || !qsem_isconst(e->b))
        return e;

    int64_t r;

    if (!qsem_ifold(S, e, e->a->val.i, e->b->val.i, w, &r)) {
        e->type = QY_ERROR;
        return e;
    }

    return qsem_const(S, e->a->tok, w, (QVal){.i = r});
}

static QExpr *qsem_binary(QSem *const S, QExpr *const e)
{
    e->a = qsem_expr(S, e->a, QY_VOID);
    e->b = qsem_expr(S, e->b, QY_VOID);
    QExpr *const a = e->a, *const b = e->b;

    if (a->type == QY_ERROR || b->type == QY_ERROR) {
        e->type = QY_ERROR;
        return e;
    }

    if ((qsem_isrec(S, a->type) || qsem_isrec(S, b->type)) && qsem_arith(e->op))
        return qsem_operator(S, e, e->op, a, b);
    if (e->op == QK_PLUS && a->type == QY_STRING && b->type == QY_STRING)
        return qsem_strcat(S, e);
    if (e->op == QK_XOR)
        return qsem_xor(S, e);
    if (!qsem_arith(e->op))
        return qsem_intop(S, e);

    if (!qast_numeric(a->type) || !qast_numeric(b->type)) {
        const QExpr *const bad = qast_numeric(a->type) ? b : a;
        const char *const op = qast_spelling(e->op);

        if (e->op == QK_PLUS && (a->type == QY_STRING || b->type == QY_STRING))
            qsem_say(S, e->tok, "+ joins two strings: write string(x)", nullptr);
        else
            qsem_say(S, e->tok, op, " takes numbers, not ", qsem_aname(S, bad->type), nullptr);

        e->type = QY_ERROR;
        return e;
    }

    QType w = qsem_meet(a, b);

    if (e->op == QK_SLASH && qast_integer(w))
        w = QY_DOUBLE;

    if (qsem_isconst(a) && qsem_isconst(b)) {
        if (!(a->untyped && b->untyped)) {
            e->a = qsem_coerce(S, a, w);
            e->b = qsem_coerce(S, b, w);
        }

        if (e->a->type == QY_ERROR || e->b->type == QY_ERROR) {
            e->type = QY_ERROR;
            return e;
        }

        return qsem_afold(S, e, e->a, e->b, w);
    }

    e->a = qsem_coerce(S, a, w);
    e->b = qsem_coerce(S, b, w);
    e->type = e->work = w;
    return e;
}

static QExpr *qsem_unary(QSem *const S, QExpr *const e)
{
    QExpr *const a = e->a = qsem_expr(S, e->a, QY_VOID);
    e->type = a->type;

    if (a->type == QY_ERROR)
        return e;

    if (e->op == QK_NOT && qast_integer(a->type))
        e->op = QK_BNOT;

    if (e->op == QK_NOT) {
        e->a = qsem_coerce(S, a, QY_BOOL);
        e->type = QY_BOOL;

        if (qsem_isconst(e->a) && e->a->type == QY_BOOL)
            return qsem_bool(S, e->tok, !e->a->val.i);

        return e;
    }

    if (e->op == QK_BNOT) {
        const QType t = qast_integer(a->type) && !a->untyped ? a->type : QY_INT;
        e->a = qsem_coerce(S, a, t);
        e->type = e->a->type == QY_ERROR ? QY_ERROR : t;

        if (qsem_isconst(e->a) && e->type == t)
            return qsem_const(S, e->tok, t, (QVal){.i = qsem_trunc(~e->a->val.i, t)});

        return e;
    }

    if (qsem_isrec(S, a->type))
        return qsem_operator(S, e, QK_MINUS, a, nullptr);

    if (!qast_numeric(a->type)) {
        qsem_say(S, e->tok, "- takes a number, not ", qsem_aname(S, a->type), nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (qast_unsigned(a->type) && !e->wrap) {
        qsem_say(S, e->tok, "- takes a signed number, not ", qsem_aname(S, a->type), nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (!qsem_isconst(a))
        return e;

    a->tok = e->tok;

    if (a->val.big) {
        qsem_say(S, e->tok, "the value doesn't fit a long", nullptr);
        a->type = QY_ERROR;
        return a;
    }

    a->val.i = qast_integer(a->type) ? (int64_t)(0 - (uint64_t)a->val.i) : a->val.i;
    a->val.d = -a->val.d;

    if (qast_integer(a->type) && e->wrap)
        a->val.i = qsem_trunc(a->val.i, a->type);
    else if (qast_integer(a->type) && !a->untyped && !qsem_inrange(a->val.i, false, a->type)) {
        qsem_say(S, e->tok, "the value doesn't fit ", qsem_aname(S, a->type), nullptr);
        a->type = QY_ERROR;
    }

    return a;
}

/* Two values compared with op: the type they meet in, QY_ERROR when they can't. */
static QType qsem_cmptype(
    QSem *const S,
    const QExpr *const a,
    const QExpr *const b,
    const QKind op,
    const QTok *const at
)
{
    if (a->type == QY_ERROR || b->type == QY_ERROR)
        return QY_ERROR;
    if (qast_numeric(a->type) && qast_numeric(b->type))
        return qsem_meet(a, b);

    if (a->type == b->type && a->type == QY_STRING)
        return QY_STRING;

    if (a->type == b->type && (a->type == QY_BOOL || qast_tag(S->prog, a->type))) {
        if (op == QK_EQ || op == QK_NE)
            return a->type;

        qsem_say(
            S,
            at,
            qast_spelling(op),
            " takes numbers or strings, not ",
            qsem_aname(S, a->type),
            nullptr
        );
        return QY_ERROR;
    }

    qsem_say(
        S,
        at,
        "can't compare ",
        qsem_aname(S, a->type),
        " with ",
        qsem_aname(S, b->type),
        nullptr
    );
    return QY_ERROR;
}

/* A relation's truth of constants of type w; a bool or a tag by its val.i. */
static bool qsem_holds(const QKind op, const QExpr *const a, const QExpr *const b)
{
    const double x = qast_numeric(a->type) ? qsem_num(a) : (double)a->val.i;
    const double y = qast_numeric(b->type) ? qsem_num(b) : (double)b->val.i;

    switch (op) {
    case QK_EQ:
        return x == y;

    case QK_NE:
        return x != y;

    case QK_LT:
        return x < y;

    case QK_LE:
        return x <= y;

    case QK_GT:
        return x > y;

    default:
        return x >= y;
    }
}

/* a < b <= c: each pair's type; a middle that is no constant gets a temporary. */
static QExpr *qsem_chain(QSem *const S, QExpr *const e)
{
    e->works = qast_alloc(S->mem, e->n * sizeof *e->works);
    e->temps = qast_alloc(S->mem, e->n * sizeof *e->temps);
    bool consts = true, bad = false;

    for (uint32_t i = 0; i < e->n; i++) {
        e->items[i] = qsem_expr(S, e->items[i], QY_VOID);
        consts &= qsem_isconst(e->items[i]);
    }

    if (e->n == 2 && qsem_hasrel(S, e->ops[0], e->items[0], e->items[1]))
        return qsem_relation(S, e, e->ops[0], e->items[0], e->items[1]);

    for (uint32_t i = 0; e->n > 2 && i < e->n; i++)
        if (qsem_isrec(S, e->items[i]->type)) {
            qsem_say(S, e->tok, "a chain of comparisons takes numbers or strings", nullptr);
            e->type = QY_ERROR;
            return e;
        }

    for (uint32_t k = 0; k + 1 < e->n; k++) {
        e->works[k] = qsem_cmptype(S, e->items[k], e->items[k + 1], e->ops[k], e->tok);
        bad |= e->works[k] == QY_ERROR;
    }

    e->type = bad ? QY_ERROR : QY_BOOL;

    if (bad)
        return e;

    for (uint32_t i = 0; i < e->n; i++)
        if (!qsem_isconst(e->items[i]) && i > 0 && i + 1 < e->n)
            e->temps[i] = qsem_var(S, e->items[i]->tok, e->items[i]->type, QV_TEMP);

    if (!consts || e->works[0] == QY_STRING)
        return e;

    bool all = true;

    for (uint32_t k = 0; k + 1 < e->n; k++)
        all &= qsem_holds(e->ops[k], e->items[k], e->items[k + 1]);

    return qsem_bool(S, e->tok, all);
}

/* a and b, a or b: bools, short-circuit, folded when a is a constant; or integers' bits. */
static QExpr *qsem_logic(QSem *const S, QExpr *const e)
{
    e->a = qsem_expr(S, e->a, QY_VOID);
    e->b = qsem_expr(S, e->b, QY_VOID);
    QExpr *const bits = qsem_bits(S, e, e->kind == QE_AND ? QK_BAND : QK_BOR);

    if (bits)
        return bits;

    e->a = qsem_coerce(S, e->a, QY_BOOL);
    e->b = qsem_coerce(S, e->b, QY_BOOL);
    e->type = e->a->type == QY_ERROR || e->b->type == QY_ERROR ? QY_ERROR : QY_BOOL;

    if (e->type == QY_ERROR || !qsem_isconst(e->a))
        return e;

    const bool left = e->a->val.i;
    const bool and = e->kind == QE_AND;
    return left == and? e->b : qsem_bool(S, e->tok, left);
}

/* a if c else b: the arms meet in one type, the hint's when both are untyped. */
static QExpr *qsem_cond(QSem *const S, QExpr *const e, const QType hint)
{
    e->c = qsem_coerce(S, qsem_expr(S, e->c, QY_VOID), QY_BOOL);
    e->a = qsem_expr(S, e->a, hint);
    e->b = qsem_expr(S, e->b, hint);
    QExpr *const a = e->a, *const b = e->b;

    if (a->type == QY_ERROR || b->type == QY_ERROR || e->c->type == QY_ERROR) {
        e->type = QY_ERROR;
        return e;
    }

    QType w = a->type;

    if (qast_numeric(a->type) && qast_numeric(b->type)) {
        w = qsem_meet(a, b);

        if (a->untyped && b->untyped && qast_numeric(hint) && qsem_fits(a, hint) &&
            qsem_fits(b, hint))
            w = hint;
    } else if (a->type != b->type) {
        qsem_say(
            S,
            e->tok,
            "the arms are ",
            qsem_aname(S, a->type),
            " and ",
            qsem_aname(S, b->type),
            nullptr
        );
        e->type = QY_ERROR;
        return e;
    }

    e->a = qsem_coerce(S, a, w);
    e->b = qsem_coerce(S, b, w);
    e->type = w;

    if (qsem_isconst(e->c))
        return e->c->val.i ? e->a : e->b;

    return e;
}

/* x in a..b: a <= x < b, x once; x in a, a list. */
static QExpr *qsem_in(QSem *const S, QExpr *const e)
{
    if (!e->c)
        return qsem_has(S, e);

    QExpr *const x = e->a = qsem_expr(S, e->a, QY_VOID);
    QExpr *const lo = e->b = qsem_expr(S, e->b, QY_VOID);
    QExpr *const hi = e->c = qsem_expr(S, e->c, QY_VOID);
    e->type = QY_BOOL;

    if (x->type == QY_ERROR || lo->type == QY_ERROR || hi->type == QY_ERROR) {
        e->type = QY_ERROR;
        return e;
    }

    if (!qast_numeric(x->type) || !qast_numeric(lo->type) || !qast_numeric(hi->type)) {
        qsem_say(S, e->tok, "in a..b takes numbers", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    const QType w1 = qsem_meet(x, lo), w2 = qsem_meet(x, hi);
    const QType w = w1 == w2 ? w1 : QY_DOUBLE;
    e->work = w;

    if (qsem_isconst(x) && qsem_isconst(lo) && qsem_isconst(hi)) {
        const double v = qsem_num(x);
        const bool in = qsem_num(lo) <= v && v < qsem_num(hi);
        return qsem_bool(S, e->tok, in != e->neg);
    }

    e->a = qsem_coerce(S, x, w);
    e->b = qsem_coerce(S, lo, w);
    e->c = qsem_coerce(S, hi, w);

    if (!qsem_isconst(e->a))
        e->var = qsem_var(S, e->tok, w, QV_TEMP);

    return e;
}

/* ---- calls */

/* A built-in's arguments: no names; count of them, ~0 any. */
bool qsem_plain(QSem *const S, const QExpr *const e, const uint32_t count)
{
    uint32_t n = 0;

    for (const QArg *a = e->args; a; a = a->next, n++)
        if (a->name) {
            qsem_say(S, a->name, qsem_text(S, e->tok), "() takes plain values", nullptr);
            return false;
        }

    if (count != ~0u && n != count) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), "() takes one value", nullptr);
        return false;
    }

    return true;
}

QExpr *qsem_print(QSem *const S, QExpr *const e)
{
    e->type = QY_VOID;
    e->builtin = QB_PRINT;

    if (!qsem_plain(S, e, ~0u))
        return e;

    uint32_t n = 0;

    for (const QArg *a = e->args; a; a = a->next)
        n++;

    e->argv = qast_alloc(S->mem, (n ? n : 1) * sizeof *e->argv);
    e->ops = qast_alloc(S->mem, (n ? n : 1) * sizeof *e->ops);
    e->nargv = n;
    uint32_t i = 0;

    for (const QArg *a = e->args; a; a = a->next, i++) {
        e->ops[i] = a->sep;
        QExpr *const v = qsem_settle(S, qsem_expr(S, a->e, QY_VOID));

        if (v->type == QY_VOID)
            qsem_mismatch(S, v, QY_STRING);

        e->argv[i] = qsem_spell(S, v, "print");
    }

    return e;
}

/* int(x), single(x), double(x), string(x). */
QExpr *qsem_convert(QSem *const S, QExpr *const e, const QType to)
{
    e->type = QY_ERROR;

    if (!qsem_plain(S, e, 1))
        return e;

    QExpr *const x = qsem_expr(S, e->args->e, to == QY_STRING ? QY_VOID : to);

    if (x->type == QY_ERROR)
        return e;

    if (to == QY_STRING) {
        if (x->type == QY_VOID) {
            qsem_mismatch(S, x, to);
            return e;
        }

        QExpr *const v = qsem_settle(S, x);
        return v->type == QY_STRING ? v : qsem_conv(S, v, QY_STRING);
    }

    if (qast_tag(S->prog, x->type) && to == QY_INT) { /* an enum's value's number */
        QExpr *const n = qast_alloc(S->mem, sizeof *n);
        *n = *x;
        n->type = QY_INT;
        return n;
    }

    if (!qast_numeric(x->type)) {
        qsem_say(
            S,
            e->tok,
            qsem_tname(to),
            "() takes a number, not ",
            qsem_aname(S, x->type),
            nullptr
        );
        return e;
    }

    x->wrap |= e->wrap;

    if (qsem_isconst(x))
        return qsem_fold_conv(S, x, to);
    if (x->type == to)
        return x;

    QExpr *const c = qsem_conv(S, x, to);
    c->trunc = qast_integer(to);
    c->wrap = e->wrap;
    return c;
}

/* Unchecked e and what is in it, lambdas aside, marked to wrap. */
static void qsem_wrapall(QExpr *const e)
{
    if (!e || e->kind == QE_LAMBDA)
        return;

    e->wrap = true;
    qsem_wrapall(e->a);
    qsem_wrapall(e->b);
    qsem_wrapall(e->c);

    for (QArg *a = e->kind == QE_CALL ? e->args : nullptr; a; a = a->next)
        qsem_wrapall(a->e);

    const bool items = e->kind == QE_CHAIN || e->kind == QE_TUPLE || e->kind == QE_FSTR;

    for (uint32_t k = 0; items && k < e->n; k++)
        qsem_wrapall(e->items[k]);
}

/* wrap(e): e's integer arithmetic and conversions keep their type's bits, never Overflow. */
QExpr *qsem_wrapped_ops(QSem *const S, QExpr *const e)
{
    if (!qsem_plain(S, e, 1)) {
        e->type = QY_ERROR;
        return e;
    }

    qsem_wrapall(e->args->e);
    return qsem_expr(S, e->args->e, QY_VOID);
}

/* A mut argument: a var's place (qsem_changes said why not), of the parameter's type. */
QExpr *qsem_mutarg(QSem *const S, QExpr *const v, const QParam *const p)
{
    if (v->type == QY_ERROR)
        return v;

    if (!qast_place(v)) {
        qsem_say(S, v->tok, "mut takes a var", nullptr);
        v->type = QY_ERROR;
        return v;
    }

    if (v->type != p->var->type && v->type != QY_ERROR &&
        !qsem_lendable(S, v->type, p->var->type)) {
        qsem_say(
            S,
            v->tok,
            "mut ",
            qsem_text(S, p->name),
            " is ",
            qsem_aname(S, p->var->type),
            ", not ",
            qsem_aname(S, v->type),
            nullptr
        );
        v->type = QY_ERROR;
    }

    return v;
}

/* ---- f-strings */

/* Is spec, a format spec, Python's for a value of type t? [[fill]align][sign][#][0][width]
   [,|_][.precision][type]. */
static bool qsem_specok(const QTok *const spec, const QType t)
{
    const char *p = spec->text, *const end = spec->text + spec->len;

    if (end - p >= 2 && strchr("<>^=", p[1]))
        p += 2;
    else if (p < end && strchr("<>^=", *p))
        p++;

    if (p < end && strchr("+- ", *p))
        p++;
    if (p < end && *p == '#')
        p++;
    if (p < end && *p == '0')
        p++;

    while (p < end && *p >= '0' && *p <= '9')
        p++;

    if (p < end && (*p == ',' || *p == '_'))
        p++;

    if (p < end && *p == '.') {
        const char *const digits = ++p;

        while (p < end && *p >= '0' && *p <= '9')
            p++;

        if (p == digits)
            return false;
    }

    const char *const types = qast_integer(t) ? "bcdoxXneEfFgG%" : qast_float(t) ? "eEfFgGn%" : "s";
    return p == end || (p + 1 == end && strchr(types, *p));
}

/* f"..": each value a number, a string or a bool, its spec one for its type. */
static QExpr *qsem_fstring(QSem *const S, QExpr *const e)
{
    e->type = QY_STRING;

    for (uint32_t k = 0; k < e->n; k++) {
        if (e->items[k]->tok->kind == QK_FPART)
            continue;

        QExpr *const u = qsem_settle(S, qsem_expr(S, e->items[k], QY_VOID));
        QExpr *const v = e->items[k] = u->type == QY_VOID ? u : qsem_spell(S, u, "an f-string");
        const QTok *const spec = e->specs[k];

        if (v->type == QY_ERROR) {
            e->type = QY_ERROR;
            continue;
        }

        if (v->type == QY_VOID) {
            qsem_mismatch(S, v, QY_STRING);
            e->type = QY_ERROR;
        } else if (spec && !qsem_specok(spec, v->type == QY_BOOL ? QY_STRING : v->type)) {
            qsem_say(
                S,
                spec,
                qsem_text(S, spec),
                " is no format for ",
                qsem_aname(S, v->type),
                nullptr
            );
            e->type = QY_ERROR;
        }
    }

    return e;
}

/* e checked: its type set, constants folded; hint the type its place wants, QY_VOID none. */
QExpr *qsem_expr(QSem *const S, QExpr *const e, const QType hint)
{
    if (!e)
        return qsem_const(S, &S->prog->src.toks[0], QY_ERROR, (QVal){});

    if (e->done)
        return e;

    if ((e->kind == QE_NAME || e->kind == QE_FIELD || e->kind == QE_INDEX) && e->type != QY_VOID)
        return e; /* a place checked already: a compound assignment's */

    QFn *const g = qsem_isgen(S, hint) ? qsem_genof(S, e) : nullptr;

    if (g) /* g(..) where its state goes: made */
        return qsem_state(S, e, g);

    switch (e->kind) {
    case QE_CONST:
        return e;

    case QE_NAME:
        return qsem_name(S, e);

    case QE_FIELD:
        return qsem_field(S, e);

    case QE_INDEX:
        return qsem_index(S, e);

    case QE_SLICE:
        return qsem_slice(S, e);

    case QE_TUPLE:
        return qsem_tuple(S, e, hint);

    case QE_FSTR:
        return qsem_fstring(S, e);

    case QE_UNARY:
        return qsem_unary(S, e);

    case QE_BINARY:
        return e->op == QK_ORELSE ? qsem_coalesce(S, e) : qsem_binary(S, e);

    case QE_CHAIN:
        return qsem_chain(S, e);

    case QE_AND:
    case QE_OR:
        return qsem_logic(S, e);

    case QE_COND:
        return qsem_cond(S, e, hint);

    case QE_IN:
        return qsem_in(S, e);

    case QE_CALL:
        if (!e->a && qsem_is(e->tok, "Error") && !qsem_find(S, e->tok))
            return qsem_error(S, e, hint);

        return qsem_call(S, e);

    case QE_NONE:
        if (qsem_wraps(S, hint, QT_OPT))
            return qsem_wrapped(S, e->tok, hint, false, nullptr);

        qsem_say(
            S,
            e->tok,
            "none needs a T or none place: write var x as T or none = none",
            nullptr
        );
        e->type = QY_ERROR;
        return e;

    case QE_IS:
        return qsem_isnone(S, e);

    case QE_TRY:
        return qsem_try(S, e);

    case QE_LAMBDA:
        qsem_say(S, e->tok, "a lambda is passed to a function's parameter", nullptr);
        e->type = QY_ERROR;
        return e;

    case QE_RANGE:
        qsem_say(S, e->tok, "a range goes in a for, or before a chain's method", nullptr);
        e->type = QY_ERROR;
        return e;

    default:
        return e;
    }
}
