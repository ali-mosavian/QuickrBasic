/* Quickr's semantic pass: types as written, constants, and types meeting.
 *
 * # How it works
 * A compound type is an entry of QProg.tys; lists, arrays and tuples are interned. A literal is
 * untyped until its place types it; two values meet in one type, widening where nothing is lost;
 * a conversion is made explicit, a constant's folded.
 */
#include "qr/qsem_int.h"

static QType qsem_gentype(QSem *S, QFn *g, const QTok *t);

static const double QSEM_FLT_MAX = 3.4028234663852886e38;

/* ---- types */

/* The scalar types a name may give, and the numbers' conversions. */
static const QType qsem_scalars[] = {
    QY_INT,
    QY_SINGLE,
    QY_DOUBLE,
    QY_STRING,
    QY_BOOL,
    QY_BYTE,
    QY_SHORT,
    QY_LONG,
    QY_UBYTE,
    QY_USHORT,
    QY_UINT,
    QY_ULONG,
};

/* A scalar type's name's type, QY_VOID none. */
QType qsem_scalar(const QTok *const t)
{
    for (uint32_t i = 0; i < countof(qsem_scalars); i++)
        if (qsem_is(t, qast_typename(qsem_scalars[i])))
            return qsem_scalars[i];

    return QY_VOID;
}

/* Is a value of t moved, never copied by itself: it holds a list, or one with drop? */
bool qsem_owns(const QSem *const S, const QType t)
{
    return qast_compound(t) && (qast_ty(S->prog, t)->owns || qast_drops(S->prog, t));
}

QType qsem_newty(QSem *const S, const QTyKind kind)
{
    QTys *const ts = &S->prog->tys;
    ts->at = qast_room(S->mem, ts->at, ts->n, &ts->cap, sizeof *ts->at);
    ts->at[ts->n] = (QTy){.kind = kind, .state = 2};

    return QY_FIRST + ts->n++;
}

/* The list of elem (count 0), or its array of count; interned. */
QType qsem_listof(QSem *const S, const QType elem, const uint32_t count)
{
    const QTyKind kind = count ? QT_ARRAY : QT_LIST;
    const QTys *const ts = &S->prog->tys;

    for (uint32_t i = 0; i < ts->n; i++)
        if (ts->at[i].kind == kind && ts->at[i].elem == elem && ts->at[i].count == count)
            return QY_FIRST + i;

    const bool owns = !count || qsem_owns(S, elem);
    const QType t = qsem_newty(S, kind);
    QTy *const y = qast_ty(S->prog, t);
    y->elem = elem;
    y->count = count;
    y->owns = owns;
    return t;
}

/* The tuple of n items' types; interned. */
QType qsem_tupleof(QSem *const S, const QType *const items, const uint32_t n)
{
    const QTys *const ts = &S->prog->tys;

    for (uint32_t i = 0; i < ts->n; i++) {
        const QTy *const y = &ts->at[i];
        bool same = y->kind == QT_TUPLE && y->nfields == n;

        for (uint32_t k = 0; same && k < n; k++)
            same = y->fields[k].type == items[k];
        if (same)
            return QY_FIRST + i;
    }

    QField *const fields = qast_alloc(S->mem, n * sizeof *fields);
    bool owns = false;

    for (uint32_t k = 0; k < n; k++) {
        fields[k] = (QField){.type = items[k]};

        owns |= qsem_owns(S, items[k]);
    }

    const QType t = qsem_newty(S, QT_TUPLE);
    QTy *const y = qast_ty(S->prog, t);
    y->fields = fields;
    y->nfields = y->capfields = n;
    y->owns = owns;
    return t;
}

/* The map from key to val; interned. Its record's fields: its keys, its values and its slots,
   lists in that order (qr/qrt.c). */
static QType qsem_mapof(QSem *const S, const QType key, const QType val)
{
    const QTys *const ts = &S->prog->tys;

    for (uint32_t i = 0; i < ts->n; i++)
        if (ts->at[i].kind == QT_MAP && ts->at[i].key == key && ts->at[i].elem == val)
            return QY_FIRST + i;

    const QType lists[] =
        {qsem_listof(S, key, 0), qsem_listof(S, val, 0), qsem_listof(S, QY_INT, 0)};

    QField *const fields = qast_alloc(S->mem, countof(lists) * sizeof *fields);

    for (uint32_t k = 0; k < countof(lists); k++)
        fields[k] = (QField){.type = lists[k]};

    const QType t = qsem_newty(S, QT_MAP);
    QTy *const y = qast_ty(S->prog, t);
    y->key = key;
    y->elem = val;
    y->fields = fields;
    y->nfields = y->capfields = countof(lists);
    y->owns = true;
    return t;
}

/* T? (QT_OPT) or T! (QT_RES) of val; interned. T?'s fields: has, val; T!'s: err, val, then its
   error's code, message and line. */
QType qsem_wrapof(QSem *const S, const QTyKind kind, const QType val)
{
    const QTys *const ts = &S->prog->tys;

    for (uint32_t i = 0; i < ts->n; i++)
        if (ts->at[i].kind == kind && ts->at[i].fields[QF_VAL].type == val)
            return QY_FIRST + i;

    const QType types[] = {QY_BOOL, val, QY_INT, QY_STRING, QY_INT};

    const uint32_t n = kind == QT_OPT ? 2 : countof(types);
    QField *const fields = qast_alloc(S->mem, n * sizeof *fields);

    for (uint32_t k = 0; k < n; k++)
        fields[k] = (QField){.type = types[k]};

    const bool owns = qsem_owns(S, val);
    const QType t = qsem_newty(S, kind);
    QTy *const y = qast_ty(S->prog, t);
    y->fields = fields;
    y->nfields = y->capfields = n;
    y->owns = owns;
    return t;
}

/* The fn type of n parameters' types and a result (QY_VOID none); interned. */
QType qsem_fnof(QSem *const S, const QType *const params, const uint32_t n, const QType result)
{
    const QTys *const ts = &S->prog->tys;

    for (uint32_t i = 0; i < ts->n; i++) {
        const QTy *const y = &ts->at[i];
        bool same = y->kind == QT_FN && y->nfields == n && y->elem == result;

        for (uint32_t k = 0; same && k < n; k++)
            same = y->fields[k].type == params[k];
        if (same)
            return QY_FIRST + i;
    }

    QField *const fields = qast_alloc(S->mem, (n ? n : 1) * sizeof *fields);

    for (uint32_t k = 0; k < n; k++)
        fields[k] = (QField){.type = params[k]};

    const QType t = qsem_newty(S, QT_FN);
    QTy *const y = qast_ty(S->prog, t);
    y->fields = fields;
    y->nfields = y->capfields = n;
    y->elem = result;
    return t;
}

/* Is t a T? or a T!, of kind if not QT_REC? */
bool qsem_wraps(const QSem *const S, const QType t, const QTyKind kind)
{
    if (!qast_compound(t))
        return false;

    const QTyKind k = qast_ty(S->prog, t)->kind;
    return kind == QT_REC ? k == QT_OPT || k == QT_RES : k == kind;
}

QType qsem_inner(const QSem *const S, const QType t)
{
    return qast_ty(S->prog, t)->fields[QF_VAL].type;
}

bool qsem_isgen(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->gen;
}

/* Is t a generic record as declared: no fields, each instance its own type? */
bool qsem_model(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->ntp && !qast_ty(S->prog, t)->of;
}

bool qsem_ismap(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->kind == QT_MAP;
}

/* A type's name: QY_ERROR after saying why when it is none. */
static QType qsem_typename(QSem *const S, const QTok *const t)
{
    const QType k = qsem_scalar(t);

    if (k != QY_VOID)
        return k;

    const QBind *const b = qsem_find(S, t);

    if (b && b->ty)
        return b->ty;
    if (b && b->fn && b->fn->ytx)
        return qsem_gentype(S, b->fn, t);

    qsem_say(S, t, "unknown type ", qsem_text(S, t), nullptr);
    return QY_ERROR;
}

/* Generator g's kept state's type, named at t: its resumable instance's, made if none yet. */
static QType qsem_gentype(QSem *const S, QFn *const g, const QTok *const t)
{
    if (g->ntp || qsem_takesfn(g)) {
        qsem_say(
            S,
            t,
            qsem_text(S, t),
            " is generic or takes a function: its state has no name",
            nullptr
        );
        return QY_ERROR;
    }

    S->genkept = true;
    const QFn *const i = qsem_instance(S, qast_expr(S->mem, QE_CALL, t), g);
    S->genkept = false;
    return i ? i->gentype : QY_ERROR;
}

/* A type's name, m.T or a.b.T: T in the module its qualifiers name. */
static QType qsem_qualtype(
    QSem *const S,
    const QTok *const *const quals,
    const uint32_t n,
    const QTok *const t
)
{
    if (!n)
        return qsem_typename(S, t);

    const QBind *m = qsem_find(S, quals[0]);

    for (uint32_t i = 1; m && m->scope && !m->alias && i < n; i++)
        m = qsem_member(S, m, quals[i]);

    if (!m || !m->scope || m->alias) {
        if (!m)
            qsem_say(S, quals[0], "unknown module ", qsem_text(S, quals[0]), nullptr);
        else if (m->ty != QY_ERROR)
            qsem_say(S, quals[n - 1], qsem_text(S, quals[n - 1]), " is not a module", nullptr);

        return QY_ERROR;
    }

    const QBind *const b = qsem_usable(S, qsem_member(S, m, t), t);

    if (b->ty)
        return b->ty;

    qsem_say(S, t, qsem_text(S, t), " is not a type", nullptr);
    return QY_ERROR;
}

/* Does T[e] name a type, the old spelling of dict[e, T]? */
static bool qsem_keyed(QSem *const S, const QExpr *const e)
{
    if (e->kind == QE_FIELD && qsem_ismod(S, e->a)) {
        const QBind *const b = qsem_qual(S, e);
        return b && b->ty;
    }

    if (e->kind != QE_NAME)
        return false;

    const QBind *const b = qsem_find(S, e->tok);
    return (b && b->ty) || qsem_scalar(e->tok) != QY_VOID;
}

/* dict[K, V]: K an int, a string or a plain enum. */
static QType qsem_maptype(QSem *const S, const QTypeEx *const x, const QType val)
{
    const QType key = qsem_type(S, x->items[0]);

    if (key == QY_ERROR)
        return QY_ERROR;

    if (key != QY_INT && key != QY_STRING && !qast_tag(S->prog, key)) {
        qsem_say(S, x->items[0]->tok, "a dict's key is an int, a string or an enum", nullptr);
        return QY_ERROR;
    }

    return qsem_mapof(S, key, val);
}

/* T[n]'s n: a constant above 0; 0 after an error. */
static uint32_t qsem_count(QSem *const S, QExpr *const e)
{
    const QExpr *const n = qsem_coerce(S, qsem_expr(S, e, QY_INT), QY_INT);

    if (n->type == QY_ERROR)
        return 0;

    if (!qsem_isconst(n) || n->val.i < 1) {
        qsem_say(S, n->tok, "an array's size is a constant above 0", nullptr);
        return 0;
    }

    return (uint32_t)n->val.i;
}

/* T[n], count n as written, at at. */
static QType qsem_arrayof(QSem *const S, const QTok *const at, const QType elem, QExpr *const count)
{
    if (qsem_keyed(S, count)) {
        qsem_say(S, at, "V[K] is written dict[K, V]", nullptr);
        return QY_ERROR;
    }

    const uint32_t n = qsem_count(S, count);
    return n ? qsem_listof(S, elem, n) : QY_ERROR;
}

/* Name[x]'s x, a name, as the value it names: an array's count. */
static QExpr *qsem_countof(QSem *const S, const QTypeEx *const x)
{
    QExpr *e = qast_expr(S->mem, QE_NAME, x->nquals ? x->quals[0] : x->tok);

    for (uint32_t k = 1; k <= x->nquals; k++) {
        QExpr *const f = qast_expr(S->mem, QE_FIELD, k < x->nquals ? x->quals[k] : x->tok);
        f->a = e;
        e = f;
    }

    return e;
}

/* Name[T, U] as written: a generic record's instance; Name[N] an array, when Name is not
   generic. */
static QType qsem_gentyped(QSem *const S, const QTypeEx *const x)
{
    const QTypeEx *const n = x->elem;
    const QType t = qsem_qualtype(S, n->quals, n->nquals, n->tok);

    if (t == QY_ERROR || qsem_model(S, t))
        return t == QY_ERROR ? t : qsem_given_types(S, t, x->items, x->n, n->tok);
    if (x->n == 1 && x->items[0]->kind == QX_NAME)
        return qsem_arrayof(S, x->tok, t, qsem_countof(S, x->items[0]));

    qsem_say(S, n->tok, qsem_text(S, n->tok), " is not generic: it takes no types", nullptr);
    return QY_ERROR;
}

/* A type as written: QY_ERROR after saying why when it is none. */
QType qsem_type(QSem *const S, const QTypeEx *const x)
{
    if (x->kind == QX_NAME) {
        const QType t = qsem_qualtype(S, x->quals, x->nquals, x->tok);

        if (!qsem_model(S, t))
            return t;

        const char *const name = qsem_text(S, x->tok);
        qsem_say(S, x->tok, name, " is generic: give its types, ", name, "[T]", nullptr);
        return QY_ERROR;
    }

    if (x->kind == QX_GEN)
        return qsem_gentyped(S, x);

    if (x->kind == QX_TUPLE || x->kind == QX_FN) {
        QType *const items = qast_alloc(S->mem, (x->n ? x->n : 1) * sizeof *items);

        for (uint32_t k = 0; k < x->n; k++)
            if ((items[k] = qsem_type(S, x->items[k])) == QY_ERROR)
                return QY_ERROR;

        if (x->kind == QX_TUPLE)
            return qsem_tupleof(S, items, x->n);

        const QType r = x->elem ? qsem_type(S, x->elem) : QY_VOID;
        return r == QY_ERROR ? QY_ERROR : qsem_fnof(S, items, x->n, r);
    }

    const QType elem = qsem_type(S, x->elem);

    if (elem == QY_ERROR)
        return QY_ERROR;

    if (x->kind == QX_OPT || x->kind == QX_RES) {
        if (qsem_wraps(S, elem, QT_REC)) {
            qsem_say(S, x->tok, "T or none and T or Error take a T that is neither", nullptr);
            return QY_ERROR;
        }

        return qsem_wrapof(S, x->kind == QX_OPT ? QT_OPT : QT_RES, elem);
    }

    if (x->kind == QX_DICT)
        return qsem_maptype(S, x, elem);
    if (x->kind == QX_LIST)
        return qsem_listof(S, elem, 0);

    return qsem_arrayof(S, x->tok, elem, x->count);
}

/* ---- constants */

QExpr *qsem_const(QSem *const S, const QTok *const tok, const QType type, const QVal v)
{
    QExpr *const e = qast_expr(S->mem, QE_CONST, tok);
    e->type = type;
    e->val = v;
    return e;
}

QExpr *qsem_bool(QSem *const S, const QTok *const tok, const bool b)
{
    return qsem_const(S, tok, QY_BOOL, (QVal){.i = b});
}

bool qsem_isconst(const QExpr *const e) { return e->kind == QE_CONST; }

static bool qsem_fits_int(const int64_t v) { return v >= INT32_MIN && v <= INT32_MAX; }

/* Does integer v, a ulong's bits when t's or from's (unsigned 64), fit integer type t? */
bool qsem_inrange(const int64_t v, const bool big, const QType t)
{
    const uint32_t bits = qast_bits(t);

    if (big && (uint64_t)v > INT64_MAX)
        return t == QY_ULONG;
    if (qast_unsigned(t))
        return v >= 0 && (bits == 64 || v < (int64_t)1 << bits);

    return bits == 64 || (v >= -((int64_t)1 << (bits - 1)) && v < (int64_t)1 << (bits - 1));
}

/* Integer v kept to integer type t's bits, its sign extended: wrap(..)'s. */
int64_t qsem_trunc(const int64_t v, const QType t)
{
    const uint32_t bits = qast_bits(t);

    if (bits == 64)
        return v;

    const uint64_t m = ((uint64_t)1 << bits) - 1, u = (uint64_t)v & m;
    return qast_unsigned(t) || !(u >> (bits - 1)) ? (int64_t)u : (int64_t)(u | ~m);
}

/* A numeric constant's value as a double. */
double qsem_num(const QExpr *const e)
{
    if (e->type == QY_ULONG || e->val.big)
        return (double)(uint64_t)e->val.i;

    return qast_integer(e->type) ? (double)e->val.i : e->val.d;
}

/* e, constant, its literal's text if it is one, for an error. */
static const char *qsem_what(QSem *const S, const QExpr *const e)
{
    const bool lit = e->tok->kind == QK_INT || e->tok->kind == QK_REAL;
    return lit ? qsem_text(S, e->tok) : "the value";
}

/* The pieces of a then b. */
const QPiece *qsem_cat(QSem *const S, const QPiece *const a, const QPiece *const b)
{
    if (!a)
        return b;

    QPiece *const p = qast_alloc(S->mem, sizeof *p);
    p->lit = a->lit;
    p->next = qsem_cat(S, a->next, b);
    return p;
}

/* ---- types meeting */

/* Can a value of from become a to by itself, losing nothing: a narrower integer to a wider of its
   sign, an unsigned to a wider signed, an integer to a double, a single to a double? */
bool qsem_widens(const QType from, const QType to)
{
    if (from == to)
        return true;
    if (to == QY_DOUBLE)
        return qast_integer(from) || from == QY_SINGLE;
    if (!qast_integer(from) || !qast_integer(to) || qast_bits(from) >= qast_bits(to))
        return false;

    return qast_unsigned(from) == qast_unsigned(to) || qast_unsigned(from);
}

/* Does untyped constant e fit type to? */
bool qsem_fits(const QExpr *const e, const QType to)
{
    if (e->type == QY_INT)
        return qast_integer(to) ? qsem_inrange(e->val.i, e->val.big, to) : qast_numeric(to);
    if (to == QY_SINGLE)
        return e->val.d <= QSEM_FLT_MAX && e->val.d >= -QSEM_FLT_MAX;

    return to == QY_DOUBLE;
}

/* The type two numeric values meet in: one's when the other widens to it; an untyped constant
   takes the other's type, an integer's even when it doesn't fit (an error then). Floats meet in
   double, int and single too; integers neither of which widens keep a's, b's mismatch said. */
QType qsem_meet(const QExpr *const a, const QExpr *const b)
{
    if (a->untyped && b->untyped)
        return a->type == QY_DOUBLE || b->type == QY_DOUBLE ? QY_DOUBLE : QY_INT;
    if (a->untyped && (qsem_fits(a, b->type) || (a->type == QY_INT && qast_integer(b->type))))
        return b->type;
    if (b->untyped && (qsem_fits(b, a->type) || (b->type == QY_INT && qast_integer(a->type))))
        return a->type;
    if (a->untyped || b->untyped)
        return QY_DOUBLE;
    if (qsem_widens(a->type, b->type))
        return b->type;
    if (qsem_widens(b->type, a->type))
        return a->type;
    if (qast_integer(a->type) && qast_integer(b->type))
        return a->type;

    return QY_DOUBLE;
}

/* An untyped constant as type to: its value converted. */
static void qsem_retype(QExpr *const e, const QType to)
{
    if (e->type == QY_INT && !qast_integer(to))
        e->val.d = qsem_num(e);
    if (to == QY_SINGLE)
        e->val.d = (double)(float)e->val.d;

    e->type = to;
    e->untyped = false;
}

/* Numeric constant e converted to to, folded: an integer by truncation, its range checked, or
   kept to to's bits in wrap(..). */
QExpr *qsem_fold_conv(QSem *const S, QExpr *const e, const QType to)
{
    const bool big = e->type == QY_ULONG || e->val.big;

    if (qast_integer(to) && qast_integer(e->type) && e->wrap)
        e->val.i = qsem_trunc(e->val.i, to);

    if (qast_integer(to) && qast_integer(e->type) && !qsem_inrange(e->val.i, big, to)) {
        qsem_say(S, e->tok, qsem_what(S, e), " doesn't fit ", qsem_aname(S, to), nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (qast_integer(to) && !qast_integer(e->type)) {
        const double d = e->val.d, top = 9223372036854775808.0; /* 2^63 */
        const bool high = to == QY_ULONG && d >= top && d < 2 * top;
        const bool ok = high || (d >= -top && d < top);
        const int64_t v = high ? (int64_t)(uint64_t)d : ok ? (int64_t)d : 0;

        if (!ok || !qsem_inrange(v, high, to)) {
            qsem_say(S, e->tok, qsem_what(S, e), " doesn't fit ", qsem_aname(S, to), nullptr);
            e->type = QY_ERROR;
            return e;
        }

        e->val.i = v;
    } else if (!qast_integer(to)) {
        e->val.d = qsem_num(e);

        if (to == QY_SINGLE)
            e->val.d = (double)(float)e->val.d;
    }

    e->type = to;
    e->untyped = false;
    return e;
}

/* An untyped constant given its own type: an int, or a double. */
QExpr *qsem_settle(QSem *const S, QExpr *const e)
{
    if (!e->untyped)
        return e;

    if (e->type == QY_INT && (e->val.big || !qsem_fits_int(e->val.i))) {
        qsem_say(S, e->tok, qsem_what(S, e), " doesn't fit an int", nullptr);
        e->type = QY_ERROR;
    }

    e->untyped = false;
    return e;
}

QExpr *qsem_conv(QSem *const S, QExpr *const a, const QType to)
{
    QExpr *const e = qast_expr(S->mem, QE_CONV, a->tok);
    e->a = a;
    e->type = to;
    return e;
}

/* Why e is no to: a narrowing names its conversion. */
void qsem_mismatch(QSem *const S, const QExpr *const e, const QType to)
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
            qsem_aname(S, from),
            " is not ",
            qsem_aname(S, to),
            ": write ",
            qsem_tname(to),
            "(x)",
            nullptr
        );
    else
        qsem_say(S, e->tok, qsem_aname(S, from), " is not ", qsem_aname(S, to), nullptr);
}

QExpr *qsem_coerce(QSem *const S, QExpr *const e, const QType to)
{
    if (e->type == QY_ERROR || to == QY_ERROR)
        return e;

    if (e->type != to && qsem_wraps(S, to, QT_REC)) { /* a T for a T? or a T! */
        QExpr *const v = qsem_coerce(S, e, qsem_inner(S, to));
        return v->type == QY_ERROR ? v : qsem_wrap(S, to, v);
    }

    if (e->untyped) {
        if (qast_numeric(to) && qsem_fits(e, to)) {
            qsem_retype(e, to);
            return e;
        }

        if (qast_numeric(to) && (!qast_integer(to) || e->type == QY_INT)) {
            qsem_say(S, e->tok, qsem_what(S, e), " doesn't fit ", qsem_aname(S, to), nullptr);
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

/* A sum of t's: an integer narrower than an int's an int, as it widens to. */
QType qsem_sumof(const QType t) { return qast_integer(t) && qast_bits(t) < 32 ? QY_INT : t; }
