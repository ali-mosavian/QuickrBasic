/* Quickr's semantic pass: names resolved, types checked, conversions made explicit, constants
 * folded.
 *
 * # How it works
 * Names live on a stack of bindings, a block's scope a mark into it. The passes:
 *   1. each fn's name; then the module's consts, in order; then each fn's signature, its
 *      defaults folded with those consts in sight;
 *   2. the module's statements, fn bodies skipped: a var or let is seen from its line on;
 *   3. each fn's body, the module's names (its fns, consts, vars and lets) below it.
 *
 * An expression gets its type bottom up. A literal is untyped until its place types it: a
 * binary operator's other side, a declaration's as, a parameter's type; alone, an int or a
 * double. Two values meet in one type, widening where nothing is lost (int to double, single to
 * double; int and single meet in double). / of ints is a double. An expression of constants
 * folds to one, so a const, a default and a for's step are values here.
 *
 * Every error is reported and the pass goes on; QY_ERROR marks what it spoiled, so it is said
 * once.
 *
 * # Example
 *    var r as single = 6     6: untyped, typed single by the as
 *    let d = r * 2.5         2.5 fits a single: r * 2.5 is a single
 *    let h = 7 / 2           3.5: an untyped double, a double by the let
 */
#include "qr/qsem.h"
#include <stdarg.h>

enum { QSEM_SAY = 160 }; /* an error's text at most */

static const double QSEM_FLT_MAX = 3.4028234663852886e38;

typedef struct {
    const QTok *name;
    QVar *var;
    QFn *fn;
} QBind;

typedef struct {
    QProg *prog;
    QMem *mem;
    QBind *binds;
    uint32_t nbinds, capbinds;
    uint32_t *marks;
    uint32_t nmarks, capmarks;
    QFn *fn; /* the fn checked, null the module */
    QVar *lastvar;
    bool early; /* the module's consts checked, before its other names are bound */
} QSem;

static QExpr *qsem_expr(QSem *S, QExpr *e, QType hint);
static QExpr *qsem_coerce(QSem *S, QExpr *e, QType to);
static void qsem_simple(QSem *S, QStmt *s);

/* ---- errors */

/* An error at t: the parts, to a null, joined. */
static void qsem_say(QSem *const S, const QTok *const t, ...)
{
    char text[QSEM_SAY];
    uint32_t n = 0;
    va_list ap;
    va_start(ap, t);

    for (const char *p; (p = va_arg(ap, const char *));) {
        const uint32_t k = (uint32_t)strlen(p);
        const uint32_t room = k < QSEM_SAY - 1 - n ? k : QSEM_SAY - 1 - n;
        memcpy(text + n, p, room);
        n += room;
    }

    va_end(ap);
    qast_error(&S->prog->errs, t, "{}", text, n);
}

/* t's text as a C string, for qsem_say. */
static const char *qsem_text(QSem *const S, const QTok *const t)
{
    char *const s = qast_alloc(S->mem, t->len + 1);
    memcpy(s, t->text, t->len);
    return s;
}

static const char *qsem_tname(const QType t) { return qast_typename(t); }

/* A type's name with its article: an int, a single. */
static const char *qsem_aname(const QType t)
{
    static const char *const names[] = {
        [QY_VOID] = "nothing",
        [QY_INT] = "an int",
        [QY_SINGLE] = "a single",
        [QY_DOUBLE] = "a double",
        [QY_STRING] = "a string",
        [QY_BOOL] = "a bool",
        [QY_ERROR] = "an error",
    };

    return names[t];
}

/* ---- names */

static bool qsem_same(const QTok *const a, const QTok *const b)
{
    return a->len == b->len && !memcmp(a->text, b->text, a->len);
}

static bool qsem_is(const QTok *const t, const char *const word)
{
    return t->len == strlen(word) && !memcmp(t->text, word, t->len);
}

static void qsem_push(QSem *const S)
{
    S->marks = qast_room(S->mem, S->marks, S->nmarks, &S->capmarks, sizeof *S->marks);
    S->marks[S->nmarks++] = S->nbinds;
}

static void qsem_pop(QSem *const S)
{
    if (S->nmarks)
        S->nbinds = S->marks[--S->nmarks];
}

/* name bound in the innermost scope; an error if it already is. */
static void qsem_bind(QSem *const S, const QTok *const name, QVar *const var, QFn *const fn)
{
    const uint32_t from = S->nmarks ? S->marks[S->nmarks - 1] : 0;

    for (uint32_t i = from; i < S->nbinds; i++)
        if (qsem_same(S->binds[i].name, name)) {
            qsem_say(S, name, qsem_text(S, name), " is already declared", nullptr);
            return;
        }

    S->binds = qast_room(S->mem, S->binds, S->nbinds, &S->capbinds, sizeof *S->binds);
    S->binds[S->nbinds++] = (QBind){.name = name, .var = var, .fn = fn};
}

static const QBind *qsem_find(const QSem *const S, const QTok *const name)
{
    for (uint32_t i = S->nbinds; i-- > 0;)
        if (qsem_same(S->binds[i].name, name))
            return &S->binds[i];

    return nullptr;
}

static QVar *qsem_var(QSem *const S, const QTok *const name, const QType type, const QVarKind kind)
{
    QVar *const v = qast_alloc(S->mem, sizeof *v);
    *v = (QVar){
        .name = name,
        .type = type,
        .kind = kind,
        .fn = S->fn,
    };

    if (S->lastvar)
        S->lastvar->next = v;
    else
        S->prog->vars = v;

    S->lastvar = v;
    return v;
}

/* A type's name: QY_ERROR after saying why when it is none. */
static QType qsem_type(QSem *const S, const QTok *const t)
{
    static const struct {
        const char *name;
        QType type;
    } types[] = {
        {"int", QY_INT},
        {"single", QY_SINGLE},
        {"double", QY_DOUBLE},
        {"string", QY_STRING},
        {"bool", QY_BOOL},
    };

    static const char *const later[] =
        {"byte", "short", "long", "ubyte", "ushort", "uint", "ulong"};

    for (uint32_t i = 0; i < countof(types); i++)
        if (qsem_is(t, types[i].name))
            return types[i].type;

    for (uint32_t i = 0; i < countof(later); i++)
        if (qsem_is(t, later[i])) {
            qsem_say(S, t, later[i], " is not supported yet", nullptr);
            return QY_ERROR;
        }

    qsem_say(S, t, "unknown type ", qsem_text(S, t), nullptr);
    return QY_ERROR;
}

/* ---- constants */

static QExpr *qsem_const(QSem *const S, const QTok *const tok, const QType type, const QVal v)
{
    QExpr *const e = qast_expr(S->mem, QE_CONST, tok);
    e->type = type;
    e->val = v;
    return e;
}

static QExpr *qsem_bool(QSem *const S, const QTok *const tok, const bool b)
{
    return qsem_const(S, tok, QY_BOOL, (QVal){.i = b});
}

static bool qsem_isconst(const QExpr *const e) { return e->kind == QE_CONST; }

static bool qsem_fits_int(const int64_t v) { return v >= INT32_MIN && v <= INT32_MAX; }

/* A numeric constant's value as a double. */
static double qsem_num(const QExpr *const e)
{
    return e->type == QY_INT ? (double)e->val.i : e->val.d;
}

/* e, constant, its literal's text if it is one, for an error. */
static const char *qsem_what(QSem *const S, const QExpr *const e)
{
    const bool lit = e->tok->kind == QK_INT || e->tok->kind == QK_REAL;
    return lit ? qsem_text(S, e->tok) : "the value";
}

/* The pieces of a then b. */
static const QPiece *qsem_cat(QSem *const S, const QPiece *const a, const QPiece *const b)
{
    if (!a)
        return b;

    QPiece *const p = qast_alloc(S->mem, sizeof *p);
    p->lit = a->lit;
    p->next = qsem_cat(S, a->next, b);
    return p;
}

/* ---- types meeting */

/* Can a value of from become a to by itself? */
static bool qsem_widens(const QType from, const QType to)
{
    return from == to || (to == QY_DOUBLE && (from == QY_INT || from == QY_SINGLE));
}

/* Does untyped constant e fit type to? */
static bool qsem_fits(const QExpr *const e, const QType to)
{
    if (e->type == QY_INT)
        return to == QY_INT ? qsem_fits_int(e->val.i) : qast_numeric(to);
    if (to == QY_SINGLE)
        return e->val.d <= QSEM_FLT_MAX && e->val.d >= -QSEM_FLT_MAX;

    return to == QY_DOUBLE;
}

/* The type two numeric values meet in; an untyped constant takes the other's type if it fits. */
static QType qsem_meet(const QExpr *const a, const QExpr *const b)
{
    if (a->untyped && b->untyped)
        return a->type == QY_DOUBLE || b->type == QY_DOUBLE ? QY_DOUBLE : QY_INT;
    if (a->untyped)
        return qsem_fits(a, b->type) ? b->type : QY_DOUBLE;
    if (b->untyped)
        return qsem_fits(b, a->type) ? a->type : QY_DOUBLE;
    if (a->type == b->type)
        return a->type;

    return QY_DOUBLE; /* int and single meet there too */
}

/* An untyped constant as type to: its value converted. */
static void qsem_retype(QExpr *const e, const QType to)
{
    if (e->type == QY_INT && to != QY_INT)
        e->val.d = (double)e->val.i;
    if (to == QY_SINGLE)
        e->val.d = (double)(float)e->val.d;

    e->type = to;
    e->untyped = false;
}

/* Numeric constant e converted to to, folded: int by truncation, with its range checked. */
static QExpr *qsem_fold_conv(QSem *const S, QExpr *const e, const QType to)
{
    if (to == QY_INT && e->type == QY_INT && !qsem_fits_int(e->val.i)) {
        qsem_say(S, e->tok, qsem_what(S, e), " doesn't fit an int", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (to == QY_INT && e->type != QY_INT) {
        const double d = e->val.d;

        if (!(d > -2147483649.0 && d < 2147483648.0)) {
            qsem_say(S, e->tok, qsem_what(S, e), " doesn't fit an int", nullptr);
            e->type = QY_ERROR;
            return e;
        }

        e->val.i = (int64_t)d;
    } else if (to != QY_INT) {
        e->val.d = qsem_num(e);

        if (to == QY_SINGLE)
            e->val.d = (double)(float)e->val.d;
    }

    e->type = to;
    e->untyped = false;
    return e;
}

/* An untyped constant given its own type: an int, or a double. */
static QExpr *qsem_settle(QSem *const S, QExpr *const e)
{
    if (!e->untyped)
        return e;

    if (e->type == QY_INT && !qsem_fits_int(e->val.i)) {
        qsem_say(S, e->tok, qsem_what(S, e), " doesn't fit an int", nullptr);
        e->type = QY_ERROR;
    }

    e->untyped = false;
    return e;
}

static QExpr *qsem_conv(QSem *const S, QExpr *const a, const QType to)
{
    QExpr *const e = qast_expr(S->mem, QE_CONV, a->tok);
    e->a = a;
    e->type = to;
    return e;
}

/* Why e is no to: a narrowing names its conversion. */
static void qsem_mismatch(QSem *const S, const QExpr *const e, const QType to)
{
    const QType from = e->type;

    if (from == QY_VOID) {
        qsem_say(S, e->tok, "the call returns nothing", nullptr);
        return;
    }

    if ((qast_numeric(from) && qast_numeric(to)) || to == QY_STRING)
        qsem_say(
            S,
            e->tok,
            qsem_aname(from),
            " is not ",
            qsem_aname(to),
            ": write ",
            qsem_tname(to),
            "(x)",
            nullptr
        );
    else
        qsem_say(S, e->tok, qsem_aname(from), " is not ", qsem_aname(to), nullptr);
}

/* e as type to, by itself: an untyped constant typed, a widening made explicit. */
static QExpr *qsem_coerce(QSem *const S, QExpr *const e, const QType to)
{
    if (e->type == QY_ERROR || to == QY_ERROR)
        return e;

    if (e->untyped) {
        if (qast_numeric(to) && qsem_fits(e, to)) {
            qsem_retype(e, to);
            return e;
        }

        if (qast_numeric(to) && (to != QY_INT || e->type == QY_INT)) {
            qsem_say(S, e->tok, qsem_what(S, e), " doesn't fit ", qsem_aname(to), nullptr);
            e->type = QY_ERROR;
            return e;
        }

        qsem_settle(S, e);
    }

    if (e->type == to)
        return e;

    if (qast_numeric(e->type) && qsem_widens(e->type, to))
        return qsem_isconst(e) ? qsem_fold_conv(S, e, to) : qsem_conv(S, e, to);

    qsem_mismatch(S, e, to);
    e->type = QY_ERROR;
    return e;
}

/* ---- operators */

static bool qsem_arith(const QKind op)
{
    return op == QK_PLUS || op == QK_MINUS || op == QK_STAR || op == QK_SLASH || op == QK_POW;
}

/* An int operator's (div, mod, bits) constant result, as the VM's; false: an error said. */
static bool qsem_ifold(
    QSem *const S,
    const QExpr *const e,
    const int64_t a,
    const int64_t b,
    int64_t *const r
)
{
    const uint32_t ua = (uint32_t)a;

    switch (e->op) {
    case QK_DIV:
    case QK_MOD:
        if (!b) {
            qsem_say(S, e->tok, "division by zero", nullptr);
            return false;
        }

        if (a == INT32_MIN && b == -1) {
            qsem_say(S, e->tok, "the value doesn't fit an int", nullptr);
            return false;
        }

        *r = e->op == QK_DIV ? a / b : a % b;
        return true;

    case QK_AMP:
        *r = a & b;
        return true;

    case QK_BAR:
        *r = a | b;
        return true;

    case QK_CARET:
        *r = a ^ b;
        return true;

    default: /* << >> */
        if (b < 0 || b > 31) {
            qsem_say(S, e->tok, "a shift's count is 0 to 31", nullptr);
            return false;
        }

        *r = e->op == QK_SHL ? (int32_t)(ua << b) : (int32_t)(a < 0 ? ~(~ua >> b) : ua >> b);
        return true;
    }
}

/* |v| under 2^bits. */
static bool qsem_small(const int64_t v, const uint32_t bits)
{
    const int64_t lim = (int64_t)1 << bits;
    return v > -lim && v < lim;
}

/* + - * ** of int constants, exact; false: too large, said. */
static bool qsem_iarith(
    QSem *const S,
    const QExpr *const e,
    const int64_t a,
    const int64_t b,
    int64_t *const r
)
{
    bool ok = qsem_small(a, 32) && qsem_small(b, 32);

    switch (e->op) {
    case QK_PLUS:
        *r = a + b;
        break;

    case QK_MINUS:
        *r = a - b;
        break;

    case QK_STAR:
        *r = a * b;
        break;

    default: { /* **: the VM's, 0 for a negative power but of 1 and -1 */
        int64_t p = 1;

        if (b < 0)
            p = a == 1 ? 1 : a == -1 ? (b & 1 ? -1 : 1) : 0;

        for (int64_t i = 0; i < b && ok; i++) {
            ok = qsem_small(p, 32);
            p *= a;
        }

        *r = p;
    }
    }

    if (!ok)
        qsem_say(S, e->tok, "the value is too large", nullptr);

    return ok;
}

/* + - * / ** of float constants in type t. */
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

    default: { /* ** of a whole power; a fractional one is not folded */
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

    if (e->op == QK_POW && w != QY_INT && qsem_num(b) != (double)(int64_t)qsem_num(b)) {
        e->a = qsem_coerce(S, a, w); /* a fractional power: the VM's */
        e->b = qsem_coerce(S, b, w);
        e->type = e->work = w;
        return e;
    }

    QVal v = {};

    if (w == QY_INT) {
        if (!qsem_iarith(S, e, a->val.i, b->val.i, &v.i)) {
            e->type = QY_ERROR;
            return e;
        }

        if (!untyped && !qsem_fits_int(v.i)) {
            qsem_say(S, e->tok, "the value doesn't fit an int", nullptr);
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

/* a xor b: bools. */
static QExpr *qsem_xor(QSem *const S, QExpr *const e)
{
    e->a = qsem_coerce(S, e->a, QY_BOOL);
    e->b = qsem_coerce(S, e->b, QY_BOOL);
    e->type = e->work = e->a->type == QY_ERROR || e->b->type == QY_ERROR ? QY_ERROR : QY_BOOL;

    if (e->type == QY_BOOL && qsem_isconst(e->a) && qsem_isconst(e->b))
        return qsem_bool(S, e->tok, e->a->val.i != e->b->val.i);

    return e;
}

/* e's operands as ints: div, mod and the bit operators. */
static QExpr *qsem_intop(QSem *const S, QExpr *const e)
{
    if (e->a->type != QY_INT || e->b->type != QY_INT) {
        const QExpr *const bad = e->a->type != QY_INT ? e->a : e->b;
        qsem_say(
            S,
            e->tok,
            qast_spelling(e->op),
            " takes ints, not ",
            qsem_aname(bad->type),
            nullptr
        );
        e->type = QY_ERROR;
        return e;
    }

    e->a = qsem_coerce(S, e->a, QY_INT);
    e->b = qsem_coerce(S, e->b, QY_INT);
    e->type = e->work = QY_INT;

    if (e->a->type == QY_ERROR || e->b->type == QY_ERROR) {
        e->type = QY_ERROR;
        return e;
    }

    if (!qsem_isconst(e->a) || !qsem_isconst(e->b))
        return e;

    int64_t r;

    if (!qsem_ifold(S, e, e->a->val.i, e->b->val.i, &r)) {
        e->type = QY_ERROR;
        return e;
    }

    return qsem_const(S, e->a->tok, QY_INT, (QVal){.i = r});
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
            qsem_say(S, e->tok, op, " takes numbers, not ", qsem_aname(bad->type), nullptr);

        e->type = QY_ERROR;
        return e;
    }

    QType w = qsem_meet(a, b);

    if (e->op == QK_SLASH && w == QY_INT)
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

    if (e->op == QK_NOT) {
        e->a = qsem_coerce(S, a, QY_BOOL);
        e->type = QY_BOOL;

        if (qsem_isconst(e->a) && e->a->type == QY_BOOL)
            return qsem_bool(S, e->tok, !e->a->val.i);

        return e;
    }

    if (e->op == QK_TILDE) {
        e->a = qsem_coerce(S, a, QY_INT);
        e->type = e->a->type == QY_ERROR ? QY_ERROR : QY_INT;

        if (qsem_isconst(e->a) && e->type == QY_INT)
            return qsem_const(S, e->tok, QY_INT, (QVal){.i = ~e->a->val.i});

        return e;
    }

    if (!qast_numeric(a->type)) {
        qsem_say(S, e->tok, "- takes a number, not ", qsem_aname(a->type), nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (!qsem_isconst(a))
        return e;

    a->tok = e->tok;

    if (a->type == QY_INT && !a->untyped && a->val.i == INT32_MIN) {
        qsem_say(S, e->tok, "the value doesn't fit an int", nullptr);
        a->type = QY_ERROR;
    }

    a->val.i = -a->val.i;
    a->val.d = -a->val.d;
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

    if (a->type == b->type && a->type == QY_BOOL) {
        if (op == QK_EQ || op == QK_NE)
            return QY_BOOL;

        qsem_say(S, at, qast_spelling(op), " takes numbers or strings, not bools", nullptr);
        return QY_ERROR;
    }

    qsem_say(S, at, "can't compare ", qsem_aname(a->type), " with ", qsem_aname(b->type), nullptr);
    return QY_ERROR;
}

/* A relation's truth of constants of type w. */
static bool qsem_holds(const QKind op, const QExpr *const a, const QExpr *const b)
{
    const double x = a->type == QY_BOOL ? (double)a->val.i : qsem_num(a);
    const double y = b->type == QY_BOOL ? (double)b->val.i : qsem_num(b);

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

/* a and b, a or b: bools, short-circuit; folded when a is a constant. */
static QExpr *qsem_logic(QSem *const S, QExpr *const e)
{
    e->a = qsem_coerce(S, qsem_expr(S, e->a, QY_VOID), QY_BOOL);
    e->b = qsem_coerce(S, qsem_expr(S, e->b, QY_VOID), QY_BOOL);
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
            qsem_aname(a->type),
            " and ",
            qsem_aname(b->type),
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

/* x in a..b: a <= x < b, x once. */
static QExpr *qsem_in(QSem *const S, QExpr *const e)
{
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

/* A built-in's arguments: no names, no mut; count of them, ~0 any. */
static bool qsem_plain(QSem *const S, const QExpr *const e, const uint32_t count)
{
    uint32_t n = 0;

    for (const QArg *a = e->args; a; a = a->next, n++)
        if (a->name || a->mut) {
            qsem_say(
                S,
                a->name ? a->name : a->mut,
                qsem_text(S, e->tok),
                "() takes plain values",
                nullptr
            );
            return false;
        }

    if (count != ~0u && n != count) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), "() takes one value", nullptr);
        return false;
    }

    return true;
}

static QExpr *qsem_print(QSem *const S, QExpr *const e)
{
    e->type = QY_VOID;
    e->builtin = QB_PRINT;

    if (!qsem_plain(S, e, ~0u))
        return e;

    uint32_t n = 0;

    for (const QArg *a = e->args; a; a = a->next)
        n++;

    e->argv = qast_alloc(S->mem, (n ? n : 1) * sizeof *e->argv);
    e->nargv = n;
    uint32_t i = 0;

    for (const QArg *a = e->args; a; a = a->next, i++) {
        QExpr *const v = qsem_settle(S, qsem_expr(S, a->e, QY_VOID));

        if (v->type == QY_VOID)
            qsem_mismatch(S, v, QY_STRING);

        e->argv[i] = v;
    }

    return e;
}

/* int(x), single(x), double(x), string(x). */
static QExpr *qsem_convert(QSem *const S, QExpr *const e, const QType to)
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

    if (!qast_numeric(x->type)) {
        qsem_say(
            S,
            e->tok,
            qsem_tname(to),
            "() takes a number, not ",
            qsem_aname(x->type),
            nullptr
        );
        return e;
    }

    if (qsem_isconst(x))
        return qsem_fold_conv(S, x, to);
    if (x->type == to)
        return x;

    QExpr *const c = qsem_conv(S, x, to);
    c->trunc = to == QY_INT;
    return c;
}

/* The argument named n among f's parameters: its index, npar none. */
static uint32_t qsem_param(const QFn *const f, const QTok *const n)
{
    for (uint32_t k = 0; k < f->npar; k++)
        if (qsem_same(f->params[k].name, n))
            return k;

    return f->npar;
}

/* A mut argument: a var of the parameter's type. */
static QExpr *qsem_mutarg(QSem *const S, QExpr *const v, const QParam *const p)
{
    if (v->kind != QE_NAME || !v->var || (v->var->kind != QV_VAR && v->var->kind != QV_MUT)) {
        if (v->type != QY_ERROR)
            qsem_say(S, v->tok, "mut takes a var", nullptr);

        v->type = QY_ERROR;
        return v;
    }

    if (v->type != p->var->type && v->type != QY_ERROR) {
        qsem_say(
            S,
            v->tok,
            "mut ",
            qsem_text(S, p->name),
            " is ",
            qsem_aname(p->var->type),
            ", not ",
            qsem_aname(v->type),
            nullptr
        );
        v->type = QY_ERROR;
    }

    return v;
}

/* Call e's mut arguments each its only way to its var: no other argument the same var. */
static void qsem_lent(QSem *const S, const QExpr *const e, const QFn *const f)
{
    for (uint32_t k = 0; k < f->npar; k++) {
        const QExpr *const m = e->argv[k];

        if (!f->params[k].mut || m->kind != QE_NAME || !m->var)
            continue;

        for (uint32_t j = 0; j < f->npar; j++)
            if (j != k && e->argv[j]->kind == QE_NAME && e->argv[j]->var == m->var)
                qsem_say(
                    S,
                    e->argv[j]->tok,
                    qsem_text(S, m->tok),
                    " is lent mut in this call",
                    nullptr
                );
    }
}

/* A call of fn f: its arguments by parameter, defaults where left out. */
static QExpr *qsem_fncall(QSem *const S, QExpr *const e, QFn *const f)
{
    e->fn = f;
    e->type = f->result;
    e->nargv = f->npar;
    e->argv = qast_alloc(S->mem, (f->npar ? f->npar : 1) * sizeof *e->argv);
    const QArg **const given = qast_alloc(S->mem, (f->npar ? f->npar : 1) * sizeof *given);
    uint32_t pos = 0;
    bool named = false;

    for (const QArg *a = e->args; a; a = a->next) {
        uint32_t k = pos;

        if (a->name) {
            named = true;
            k = qsem_param(f, a->name);

            if (k == f->npar) {
                qsem_say(
                    S,
                    a->name,
                    qsem_text(S, f->name),
                    " has no parameter ",
                    qsem_text(S, a->name),
                    nullptr
                );
                continue;
            }
        } else if (named) {
            qsem_say(S, a->e->tok, "a value by position after one by name", nullptr);
            continue;
        } else if (pos++ >= f->npar) {
            qsem_say(S, a->e->tok, "too many values for ", qsem_text(S, f->name), nullptr);
            break;
        }

        if (given[k])
            qsem_say(
                S,
                a->name ? a->name : a->e->tok,
                qsem_text(S, f->params[k].name),
                " given twice",
                nullptr
            );

        given[k] = a;
    }

    for (uint32_t k = 0; k < f->npar; k++) {
        const QParam *const p = &f->params[k];
        const QArg *const a = given[k];

        if (!a) {
            if (!p->def)
                qsem_say(
                    S,
                    e->tok,
                    qsem_text(S, f->name),
                    " needs a value for ",
                    qsem_text(S, p->name),
                    nullptr
                );

            e->argv[k] = p->def ? p->def : qsem_const(S, e->tok, QY_ERROR, (QVal){});

            continue;
        }

        if (!p->var) {
            e->argv[k] = qsem_const(S, e->tok, QY_ERROR, (QVal){});

            continue;
        }

        QExpr *const v = qsem_expr(S, a->e, p->var->type);

        if (p->mut && !a->mut)
            qsem_say(S, a->e->tok, qsem_text(S, p->name), " is mut: write mut before it", nullptr);
        if (!p->mut && a->mut)
            qsem_say(S, a->mut, qsem_text(S, p->name), " is not mut", nullptr);

        e->argv[k] = p->mut ? qsem_mutarg(S, v, p) : qsem_coerce(S, v, p->var->type);
    }

    qsem_lent(S, e, f);
    return e;
}

static QExpr *qsem_call(QSem *const S, QExpr *const e)
{
    const QBind *const b = qsem_find(S, e->tok);

    if (b && b->fn)
        return qsem_fncall(S, e, b->fn);

    if (b) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is not a fn", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    static const struct {
        const char *name;
        QType to;
    } convs[] = {
        {"int", QY_INT},
        {"single", QY_SINGLE},
        {"double", QY_DOUBLE},
        {"string", QY_STRING},
    };

    if (qsem_is(e->tok, "print"))
        return qsem_print(S, e);

    for (uint32_t i = 0; i < countof(convs); i++)
        if (qsem_is(e->tok, convs[i].name))
            return qsem_convert(S, e, convs[i].to);

    static const char *const later[] =
        {"byte", "short", "long", "ubyte", "ushort", "uint", "ulong", "len", "copy"};

    for (uint32_t i = 0; i < countof(later); i++)
        if (qsem_is(e->tok, later[i])) {
            qsem_say(S, e->tok, later[i], "() is not supported yet", nullptr);
            e->type = QY_ERROR;
            return e;
        }

    qsem_say(S, e->tok, "unknown fn ", qsem_text(S, e->tok), nullptr);
    e->type = QY_ERROR;
    return e;
}

static QExpr *qsem_name(QSem *const S, QExpr *const e)
{
    const QBind *const b = qsem_find(S, e->tok);
    e->type = QY_ERROR;

    if (!b) {
        qsem_say(
            S,
            e->tok,
            S->early ? "unknown const " : "unknown name ",
            qsem_text(S, e->tok),
            nullptr
        );
        return e;
    }

    if (b->fn) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is a fn: call it", nullptr);
        return e;
    }

    QVar *const v = b->var;

    if (v->constant) {
        QExpr *const k = qsem_const(S, e->tok, v->type, v->val);
        k->untyped = v->untyped;
        return k;
    }

    e->var = v;
    e->type = v->type;
    return e;
}

/* e checked: its type set, constants folded; hint the type its place wants, QY_VOID none. */
static QExpr *qsem_expr(QSem *const S, QExpr *const e, const QType hint)
{
    if (!e)
        return qsem_const(S, &S->prog->src.toks[0], QY_ERROR, (QVal){});

    switch (e->kind) {
    case QE_CONST:
        return e;

    case QE_NAME:
        return qsem_name(S, e);

    case QE_UNARY:
        return qsem_unary(S, e);

    case QE_BINARY:
        return qsem_binary(S, e);

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
        return qsem_call(S, e);

    default:
        return e;
    }
}

/* ---- statements */

/* One declaration of let, var or const. */
static void qsem_decl(QSem *const S, QDecl *const d, const QKind kw)
{
    const QType t = d->type ? qsem_type(S, d->type) : QY_VOID;
    QExpr *v = d->value ? qsem_expr(S, d->value, t) : nullptr;

    if (!v && kw != QK_VAR)
        qsem_say(S, d->name, "a ", qast_spelling(kw), " needs a value", nullptr);
    if (!v && !d->type)
        qsem_say(S, d->name, qsem_text(S, d->name), " needs a type or a value", nullptr);

    if (v && d->type)
        v = qsem_coerce(S, v, t);
    else if (v && !(kw == QK_CONST && v->untyped))
        v = qsem_settle(S, v);

    if (v && v->type == QY_VOID) {
        qsem_mismatch(S, v, QY_INT);
        v->type = QY_ERROR;
    }

    d->value = v;
    const QType type = d->type ? t : v ? v->type : QY_ERROR;

    if (kw == QK_CONST) {
        QVar *const c = qsem_var(S, d->name, type, QV_CONST);
        c->constant = true;

        if (v && !qsem_isconst(v) && v->type != QY_ERROR) {
            qsem_say(
                S,
                v->tok,
                qsem_text(S, d->name),
                " is not a constant: a const's value is known before the run",
                nullptr
            );
            c->type = QY_ERROR;
        }

        if (v && qsem_isconst(v)) {
            c->val = v->val;
            c->untyped = v->untyped;
            c->type = v->type;
        } else
            c->type = QY_ERROR;

        d->var = c;
    } else
        d->var = qsem_var(S, d->name, type, kw == QK_LET ? QV_LET : QV_VAR);

    qsem_bind(S, d->name, d->var, nullptr);
}

static void qsem_decls(QSem *const S, QStmt *const s)
{
    for (QDecl *d = s->decls; d; d = d->next)
        qsem_decl(S, d, s->op);
}

static void qsem_assign(QSem *const S, QStmt *const s)
{
    QExpr *const t = s->target = qsem_name(S, s->target);

    if (t->type == QY_ERROR) {
        qsem_expr(S, s->e, QY_VOID);
        return;
    }

    if (t->kind != QE_NAME) { /* a const's, folded */
        qsem_say(S, t->tok, qsem_text(S, t->tok), " is a const: it can't change", nullptr);
        return;
    }

    static const char *const why[] = {
        [QV_LET] = " is a let: it can't change",
        [QV_PARAM] = " is a parameter: make it mut to change it",
        [QV_LOOP] = " is the loop's counter: it can't change",
    };

    const QVarKind k = t->var->kind;

    if (k != QV_VAR && k != QV_MUT) {
        qsem_say(S, t->tok, qsem_text(S, t->tok), why[k], nullptr);
        qsem_expr(S, s->e, QY_VOID);
        return;
    }

    if (s->op == QK_EQ) {
        s->e = qsem_coerce(S, qsem_expr(S, s->e, t->type), t->type);
        return;
    }

    static const QKind ops[] = {[QK_ADDEQ - QK_ADDEQ] = QK_PLUS, QK_MINUS, QK_STAR, QK_SLASH};

    QExpr *const x = qast_expr(S->mem, QE_BINARY, s->tok);
    x->op = ops[s->op - QK_ADDEQ];
    x->a = qast_expr(S->mem, QE_NAME, t->tok);
    x->b = s->e;
    s->e = qsem_coerce(S, qsem_expr(S, x, t->type), t->type);
}

static void qsem_cond_of(QSem *const S, QStmt *const s)
{
    s->e = qsem_coerce(S, qsem_expr(S, s->e, QY_BOOL), QY_BOOL);
}

/* for i in a..b [step s]: ints; a constant step not 0. Its limit and step are variables, as
   I.NEXT's (qr/qcg.c); by its step if constant, else 0. */
static void qsem_for(QSem *const S, QStmt *const s)
{
    s->e = qsem_coerce(S, qsem_expr(S, s->e, QY_INT), QY_INT);
    s->to = qsem_coerce(S, qsem_expr(S, s->to, QY_INT), QY_INT);
    s->by = 1;

    if (s->step) {
        s->step = qsem_coerce(S, qsem_expr(S, s->step, QY_INT), QY_INT);
        s->by = 0;

        if (qsem_isconst(s->step) && s->step->type == QY_INT) {
            s->by = (int32_t)s->step->val.i;

            if (!s->by)
                qsem_say(S, s->step->tok, "a step of 0 never ends", nullptr);
        }
    }

    s->stp = qsem_var(S, s->tok, QY_INT, QV_TEMP);
    s->lim = qsem_var(S, s->tok, QY_INT, QV_TEMP);

    qsem_push(S);

    if (!s->decls) /* its name not parsed */
        return;

    s->var = qsem_var(S, s->decls->name, QY_INT, QV_LOOP);
    qsem_bind(S, s->decls->name, s->var, nullptr);
}

static void qsem_return(QSem *const S, QStmt *const s)
{
    const QFn *const f = s->fn;

    if (!s->e) {
        if (f->result != QY_VOID && f->result != QY_ERROR)
            qsem_say(S, s->tok, "return needs ", qsem_aname(f->result), nullptr);

        return;
    }

    if (f->result == QY_VOID) {
        qsem_say(
            S,
            s->e->tok,
            qsem_text(S, f->name),
            " returns nothing: it has no as type",
            nullptr
        );
        qsem_expr(S, s->e, QY_VOID);
        return;
    }

    s->e = qsem_coerce(S, qsem_expr(S, s->e, f->result), f->result);
}

/* A statement a one-line if may take, or any not of a block. */
static void qsem_simple(QSem *const S, QStmt *const s)
{
    switch (s->kind) {
    case QS_DECL:
        qsem_decls(S, s);
        return;

    case QS_ASSIGN:
        qsem_assign(S, s);
        return;

    case QS_CALL:
        s->e = qsem_expr(S, s->e, QY_VOID);
        return;

    case QS_RETURN:
        qsem_return(S, s);
        return;

    default:
        return;
    }
}

/* Does the clause whose last statement is last always return? */
static bool qsem_returns(const QStmt *const last)
{
    if (last->kind == QS_RETURN)
        return true;
    if (last->kind == QS_ENDDO)
        return !last->link->broken;
    if (last->kind != QS_ENDIF)
        return false;

    bool other = false;

    for (const QStmt *c = last->link; c != last; c = c->link) {
        if (!qsem_returns(c->link->prev))
            return false;

        other |= c->kind == QS_ELSE;
    }

    return other;
}

static void qsem_stmt(QSem *const S, QStmt *const s)
{
    switch (s->kind) {
    case QS_IF:
        qsem_cond_of(S, s);

        if (s->oneline)
            qsem_simple(S, s->body);
        else
            qsem_push(S);

        return;

    case QS_ELSEIF:
        qsem_pop(S);
        qsem_cond_of(S, s);
        qsem_push(S);
        return;

    case QS_ELSE:
        qsem_pop(S);
        qsem_push(S);
        return;

    case QS_WHILE:
        qsem_cond_of(S, s);
        qsem_push(S);
        return;

    case QS_DO:
        qsem_push(S);
        return;

    case QS_FOR:
        qsem_for(S, s);
        return;

    case QS_ENDIF:
    case QS_ENDWHILE:
    case QS_ENDDO:
    case QS_ENDFOR:
        qsem_pop(S);
        return;

    case QS_ENDFN: {
        const QFn *const f = s->fn;

        if (f->result != QY_VOID && f->result != QY_ERROR && !qsem_returns(s->prev))
            qsem_say(
                S,
                s->tok,
                qsem_text(S, f->name),
                " can end without returning ",
                qsem_aname(f->result),
                nullptr
            );

        return;
    }

    default:
        qsem_simple(S, s);
    }
}

/* Past fn statement s's body: its end fn's next; the program's end when it has none. */
static QStmt *qsem_past(const QStmt *const s) { return s->fn->end ? s->fn->end->next : nullptr; }

/* ---- the passes */

/* The module's consts, outside blocks and fns, in order. */
static void qsem_consts(QSem *const S)
{
    uint32_t depth = 0;

    for (QStmt *s = S->prog->first; s;) {
        if (s->kind == QS_FN) {
            s = qsem_past(s);
            continue;
        }

        const bool opens = (s->kind == QS_IF && !s->oneline) || s->kind == QS_WHILE ||
                           s->kind == QS_DO || s->kind == QS_FOR;
        const bool closes = s->kind == QS_ENDIF || s->kind == QS_ENDWHILE || s->kind == QS_ENDDO ||
                            s->kind == QS_ENDFOR;

        depth += opens;
        depth -= closes && depth;

        if (!depth && s->kind == QS_DECL && s->op == QK_CONST) {
            s->early = S->early = true;
            qsem_decls(S, s);
            S->early = false;
        }

        s = s->next;
    }
}

/* Fn f's parameters' and result's types, its defaults folded. */
static void qsem_signature(QSem *const S, QFn *const f)
{
    S->fn = f;
    bool defaults = false;

    for (uint32_t k = 0; k < f->npar; k++) {
        QParam *const p = &f->params[k];
        QType t = p->type ? qsem_type(S, p->type) : QY_VOID;

        if (p->def) {
            QExpr *d = qsem_expr(S, p->def, t);
            d = p->type ? qsem_coerce(S, d, t) : qsem_settle(S, d);

            if (!qsem_isconst(d) && d->type != QY_ERROR)
                qsem_say(S, d->tok, "a default is a constant", nullptr);
            if (p->mut)
                qsem_say(S, p->name, "a mut parameter has no default", nullptr);
            if (!p->type)
                t = d->type;

            p->def = d;
            defaults = true;
        } else if (defaults)
            qsem_say(
                S,
                p->name,
                qsem_text(S, p->name),
                " needs a default: one before it has one",
                nullptr
            );

        if (!p->type && !p->def) {
            qsem_say(S, p->name, qsem_text(S, p->name), " needs a type", nullptr);
            t = QY_ERROR;
        }

        p->var = qsem_var(S, p->name, t, p->mut ? QV_MUT : QV_PARAM);
    }

    f->result = f->rtype ? qsem_type(S, f->rtype) : QY_VOID;

    if (f->result != QY_VOID)
        f->res = qsem_var(S, f->name, f->result, QV_RESULT);

    f->sig = true;
    S->fn = nullptr;
}

/* The module's statements, fn bodies and early consts skipped. */
static void qsem_module(QSem *const S)
{
    for (QStmt *s = S->prog->first; s;) {
        if (s->kind == QS_FN) {
            s = qsem_past(s);
            continue;
        }

        if (!s->early)
            qsem_stmt(S, s);

        s = s->next;
    }
}

/* Fn f's body, its parameters in its scope. */
static void qsem_body(QSem *const S, QFn *const f)
{
    if (!f->end || !f->sig)
        return;

    S->fn = f;
    const uint32_t marks = S->nmarks;
    qsem_push(S);

    for (uint32_t k = 0; k < f->npar; k++)
        qsem_bind(S, f->params[k].name, f->params[k].var, nullptr);

    for (QStmt *s = f->def->next; s != f->end; s = s->next)
        qsem_stmt(S, s);

    qsem_stmt(S, f->end);

    while (S->nmarks > marks)
        qsem_pop(S);

    S->fn = nullptr;
}

void qsem_program(QProg *const p, QMem *const m)
{
    QSem S = {.prog = p, .mem = m};

    for (QFn *f = p->fns; f; f = f->next)
        qsem_bind(&S, f->name, nullptr, f);

    qsem_consts(&S);

    for (QFn *f = p->fns; f; f = f->next)
        qsem_signature(&S, f);

    qsem_module(&S);

    while (S.nmarks)
        qsem_pop(&S);

    for (QFn *f = p->fns; f; f = f->next)
        qsem_body(&S, f);
}
