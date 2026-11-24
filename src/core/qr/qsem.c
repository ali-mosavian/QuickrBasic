/* Quickr's semantic pass: names resolved, types checked, conversions made explicit, constants
 * folded.
 *
 * # How it works
 * Names live on a stack of bindings, a block's scope a mark into it. The passes:
 *   1. each fn's and type's name; then the module's consts, in order; then each type's fields
 *      and each fn's signature, their defaults folded with those consts in sight;
 *   2. the module's statements, fn bodies skipped: a var or let is seen from its line on;
 *   3. each fn's body, the module's names (its fns, consts, vars and lets) below it;
 *   4. each move checked against the uses after it.
 *
 * A compound type is an entry of QProg.tys; lists, arrays and tuples are interned, so a type is
 * the same as another when its number is. A record, tuple or list lives in a place: a value of
 * one is built straight into the place it goes to, or first in a temporary when it reads that
 * place, or is no place's and its address is wanted. A for's element and a case's datum name
 * the place they stand for (QVar.place), so they cost nothing. A for over a list borrows it till
 * end for: no change to it but through its element.
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
    QType ty; /* a type's name */
} QBind;

/* A for over a list: the list's variable, borrowed till its end for. */
typedef struct {
    const QVar *root;
    const QStmt *loop;
} QLoan;

/* A match open: its values seen, an enum's or a bool's. */
typedef struct {
    QStmt *s;
    uint8_t *seen;
    uint32_t nseen;
    bool other; /* case else */
} QOpenMatch;

/* A variable moved: an error if it is used after, or in a loop it is declared outside. */
typedef struct {
    const QExpr *e;
    QVar *var;
    uint32_t at, loop; /* their tokens; loop UINT32_MAX none */
} QMove;

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
    QLoan *loans;
    uint32_t nloans, caploans;
    QOpenMatch *matches;
    uint32_t nmatches, capmatches;
    QMove *moves;
    uint32_t nmoves, capmoves;
    uint32_t *loops; /* the open loops' first tokens */
    uint32_t nloops, caploops;
} QSem;

static QExpr *qsem_expr(QSem *S, QExpr *e, QType hint);
static QExpr *qsem_coerce(QSem *S, QExpr *e, QType to);
static void qsem_simple(QSem *S, QStmt *s);
static const QBind *qsem_find(const QSem *S, const QTok *name);
static bool qsem_isconst(const QExpr *e);
static bool qsem_byplace(const QSem *S, QType t);
static QVar *qsem_root(const QExpr *e);
static QExpr *qsem_addressed(QSem *S, QExpr *e);
static bool qsem_changes(QSem *S, const QExpr *e, const QTok *at);

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

/* a, b and c joined, in the arena. */
static const char *qsem_join(
    QSem *const S,
    const char *const a,
    const char *const b,
    const char *const c
)
{
    const size_t na = strlen(a), nb = strlen(b), nc = strlen(c);
    char *const s = qast_alloc(S->mem, na + nb + nc + 1);
    memcpy(s, a, na);
    memcpy(s + na, b, nb);
    memcpy(s + na + nb, c, nc);
    return s;
}

/* n in decimal, in the arena. */
static const char *qsem_uint(QSem *const S, uint32_t n)
{
    char rev[12];
    uint32_t k = 0;

    do {
        rev[k++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);

    char *const s = qast_alloc(S->mem, k + 1);

    for (uint32_t i = 0; i < k; i++)
        s[i] = rev[k - 1 - i];

    return s;
}

/* Type t's name as written: int, Vec, int[], Vec[4], (int, string). */
static const char *qsem_tyname(QSem *const S, const QType t)
{
    if (!qast_compound(t))
        return qast_typename(t);

    const QTy *const y = qast_ty(S->prog, t);

    switch (y->kind) {
    case QT_REC:
    case QT_ENUM:
        return qsem_text(S, y->name);

    case QT_LIST:
        return qsem_join(S, qsem_tyname(S, y->elem), "[]", "");

    case QT_ARRAY:
        return qsem_join(
            S,
            qsem_join(S, qsem_tyname(S, y->elem), "[", qsem_uint(S, y->count)),
            "]",
            ""
        );

    case QT_MAP:
        return qsem_join(
            S,
            qsem_join(S, qsem_tyname(S, y->elem), "[", qsem_tyname(S, y->key)),
            "]",
            ""
        );

    default: {
        const char *s = "(";

        for (uint32_t k = 0; k < y->nfields; k++)
            s = qsem_join(S, s, k ? ", " : "", qsem_tyname(S, y->fields[k].type));

        return qsem_join(S, s, ")", "");
    }
    }
}

/* A type's name with its article: an int, a single, a Vec. */
static const char *qsem_aname(QSem *const S, const QType t)
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

    if (t < QY_FIRST)
        return names[t];

    const char *const n = qsem_tyname(S, t);
    return qsem_join(S, strchr("AEIOUaeiou", n[0]) ? "an " : "a ", n, "");
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

static void qsem_bindty(QSem *const S, const QTok *const name, const QType t)
{
    qsem_bind(S, name, nullptr, nullptr);
    S->binds[S->nbinds - 1].ty = t;
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

/* ---- types */

static const char *const qsem_later[] =
    {"byte", "short", "long", "ubyte", "ushort", "uint", "ulong"};

/* A scalar type's name's type, QY_VOID none; later: a later milestone's. */
static QType qsem_scalar(const QTok *const t, bool *const later)
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

    *later = false;

    for (uint32_t i = 0; i < countof(types); i++)
        if (qsem_is(t, types[i].name))
            return types[i].type;

    for (uint32_t i = 0; i < countof(qsem_later); i++)
        *later |= qsem_is(t, qsem_later[i]);

    return QY_VOID;
}

static bool qsem_owns(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->owns;
}

static QType qsem_newty(QSem *const S, const QTyKind kind)
{
    QTys *const ts = &S->prog->tys;
    ts->at = qast_room(S->mem, ts->at, ts->n, &ts->cap, sizeof *ts->at);
    ts->at[ts->n] = (QTy){.kind = kind, .state = 2};

    return QY_FIRST + ts->n++;
}

/* The list of elem (count 0), or its array of count; interned. */
static QType qsem_listof(QSem *const S, const QType elem, const uint32_t count)
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
static QType qsem_tupleof(QSem *const S, const QType *const items, const uint32_t n)
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

static bool qsem_ismap(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->kind == QT_MAP;
}

/* A type's name: QY_ERROR after saying why when it is none. */
static QType qsem_typename(QSem *const S, const QTok *const t)
{
    bool later;
    const QType k = qsem_scalar(t, &later);

    if (k != QY_VOID)
        return k;

    if (later) {
        qsem_say(S, t, qsem_text(S, t), " is not supported yet", nullptr);
        return QY_ERROR;
    }

    const QBind *const b = qsem_find(S, t);

    if (b && b->ty)
        return b->ty;

    qsem_say(S, t, "unknown type ", qsem_text(S, t), nullptr);
    return QY_ERROR;
}

/* Does T[e] name a map's key type, e a type's name? */
static bool qsem_keyed(const QSem *const S, const QExpr *const e)
{
    if (e->kind != QE_NAME)
        return false;

    const QBind *const b = qsem_find(S, e->tok);
    bool later;
    return (b && b->ty) || qsem_scalar(e->tok, &later) != QY_VOID || later;
}

/* V[K]: a map, K an int, a string or a plain enum. */
static QType qsem_maptype(QSem *const S, const QTypeEx *const x, const QType val)
{
    const QType key = qsem_typename(S, x->count->tok);

    if (key == QY_ERROR)
        return QY_ERROR;

    if (key != QY_INT && key != QY_STRING && !qast_tag(S->prog, key)) {
        qsem_say(S, x->count->tok, "a map's key is an int, a string or an enum", nullptr);
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

/* A type as written: QY_ERROR after saying why when it is none. */
static QType qsem_type(QSem *const S, const QTypeEx *const x)
{
    if (x->kind == QX_NAME)
        return qsem_typename(S, x->tok);

    if (x->kind == QX_TUPLE) {
        QType *const items = qast_alloc(S->mem, x->n * sizeof *items);

        for (uint32_t k = 0; k < x->n; k++)
            if ((items[k] = qsem_type(S, x->items[k])) == QY_ERROR)
                return QY_ERROR;

        return qsem_tupleof(S, items, x->n);
    }

    const QType elem = qsem_type(S, x->elem);

    if (elem == QY_ERROR)
        return QY_ERROR;

    if (qast_array(S->prog, elem)) {
        qsem_say(S, x->tok, "lists of lists are not supported yet", nullptr);
        return QY_ERROR;
    }

    if (x->kind == QX_LIST)
        return qsem_listof(S, elem, 0);

    if (qsem_keyed(S, x->count))
        return qsem_maptype(S, x, elem);

    const uint32_t n = qsem_count(S, x->count);
    return n ? qsem_listof(S, elem, n) : QY_ERROR;
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
            qsem_aname(S, bad->type),
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
            qsem_say(S, e->tok, op, " takes numbers, not ", qsem_aname(S, bad->type), nullptr);

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
        qsem_say(S, e->tok, "- takes a number, not ", qsem_aname(S, a->type), nullptr);
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

static QExpr *qsem_has(QSem *S, QExpr *e);

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
        if (qsem_byplace(S, v->type))
            qsem_say(
                S,
                v->tok,
                "print takes numbers, strings and bools, not ",
                qsem_aname(S, v->type),
                nullptr
            );

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

    if (qsem_isconst(x))
        return qsem_fold_conv(S, x, to);
    if (x->type == to)
        return x;

    QExpr *const c = qsem_conv(S, x, to);
    c->trunc = to == QY_INT;
    return c;
}

/* A mut argument: a var's place (qsem_changes said why not), of the parameter's type. */
static QExpr *qsem_mutarg(QSem *const S, QExpr *const v, const QParam *const p)
{
    if (v->type == QY_ERROR)
        return v;

    if (!qast_place(v)) {
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
            qsem_aname(S, p->var->type),
            ", not ",
            qsem_aname(S, v->type),
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
        const QVar *const v = f->params[k].mut && m->type != QY_ERROR ? qsem_root(m) : nullptr;

        for (uint32_t j = 0; v && j < f->npar; j++)
            if (j != k && qsem_root(e->argv[j]) == v)
                qsem_say(
                    S,
                    e->argv[j]->tok,
                    qsem_text(S, v->name),
                    " is lent mut in this call",
                    nullptr
                );
    }
}

/* args matched to n names, by position then by name: each name's, null none. who's: "has no
   parameter" or "has no field" (what). */
static const QArg **qsem_given(
    QSem *const S,
    const QArg *const args,
    const QTok *const *const names,
    const uint32_t n,
    const QTok *const who,
    const char *const what
)
{
    const QArg **const given = qast_alloc(S->mem, (n ? n : 1) * sizeof *given);
    uint32_t pos = 0;
    bool named = false;

    for (const QArg *a = args; a; a = a->next) {
        uint32_t k = pos;

        if (a->name) {
            named = true;
            k = 0;

            while (k < n && !qsem_same(names[k], a->name))
                k++;

            if (k == n) {
                qsem_say(S, a->name, qsem_text(S, who), what, qsem_text(S, a->name), nullptr);
                continue;
            }
        } else if (named) {
            qsem_say(S, a->e->tok, "a value by position after one by name", nullptr);
            continue;
        } else if (pos++ >= n) {
            qsem_say(S, a->e->tok, "too many values for ", qsem_text(S, who), nullptr);
            break;
        }

        if (given[k])
            qsem_say(
                S,
                a->name ? a->name : a->e->tok,
                qsem_text(S, names[k]),
                " given twice",
                nullptr
            );

        given[k] = a;
    }

    return given;
}

/* A call of fn f: its arguments by parameter, defaults where left out. */
static QExpr *qsem_fncall(QSem *const S, QExpr *const e, QFn *const f)
{
    e->fn = f;
    e->type = f->result;
    e->nargv = f->npar;
    e->argv = qast_alloc(S->mem, (f->npar ? f->npar : 1) * sizeof *e->argv);
    const QTok **const names = qast_alloc(S->mem, (f->npar ? f->npar : 1) * sizeof *names);

    for (uint32_t k = 0; k < f->npar; k++)
        names[k] = f->params[k].name;

    const QArg **const given =
        qsem_given(S, e->args, names, f->npar, f->name, " has no parameter ");

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

        const bool lend = p->mut && a->mut && qsem_changes(S, a->e, a->e->tok);
        QExpr *const v = qsem_expr(S, a->e, p->var->type);

        if (p->mut && !a->mut)
            qsem_say(S, a->e->tok, qsem_text(S, p->name), " is mut: write mut before it", nullptr);
        if (!p->mut && a->mut)
            qsem_say(S, a->mut, qsem_text(S, p->name), " is not mut", nullptr);
        if (p->mut && a->mut && !lend)
            v->type = QY_ERROR;

        e->argv[k] =
            p->mut ? qsem_mutarg(S, v, p) : qsem_addressed(S, qsem_coerce(S, v, p->var->type));
    }

    qsem_lent(S, e, f);
    return e;
}

/* ---- places, moves */

/* A checked place's variable: its name's, its fields' and elements' base's; null none. */
static QVar *qsem_root(const QExpr *e)
{
    while (e->kind == QE_FIELD || e->kind == QE_INDEX)
        e = e->a;

    return e->kind == QE_NAME ? e->var : nullptr;
}

/* An unchecked place's variable as bound: a for's element itself, not the list it names. */
static QVar *qsem_rawroot(const QSem *const S, const QExpr *e)
{
    while (e->kind == QE_FIELD || e->kind == QE_INDEX)
        e = e->a;

    const QBind *const b = e->kind == QE_NAME ? qsem_find(S, e->tok) : nullptr;
    return b ? b->var : nullptr;
}

/* The for's loan of variable v, the innermost; null none. */
static const QLoan *qsem_loan(const QSem *const S, const QVar *const v)
{
    for (uint32_t i = S->nloans; i-- > 0;)
        if (S->loans[i].root == v)
            return &S->loans[i];

    return nullptr;
}

/* Unchecked place e changed at at: its variable a var, a mut parameter or a for mut's element,
   and no list a for borrows. False: an error said, a for mut's loan by qsem_name. */
static bool qsem_changes(QSem *const S, const QExpr *const e, const QTok *const at)
{
    static const char *const why[] = {
        [QV_LET] = " is a let: it can't change",
        [QV_CONST] = " is a const: it can't change",
        [QV_PARAM] = " is a parameter: make it mut to change it",
        [QV_LOOP] = " is the loop's counter: it can't change",
        [QV_TEMP] = " can't change",
        [QV_RESULT] = " can't change",
    };

    const QVar *const v = qsem_rawroot(S, e);

    if (!v)
        return true; /* not a place: said where it is checked */

    const char *const name = qsem_text(S, v->name);

    if (v->kind == QV_EACH) {
        if (!v->mut)
            qsem_say(S, e->tok, name, " is the for's element: write for mut ", name, nullptr);

        return v->mut;
    }

    if (v->kind != QV_VAR && v->kind != QV_MUT) {
        qsem_say(S, e->tok, name, why[v->kind], nullptr);
        return false;
    }

    const QLoan *const l = qsem_loan(S, v);

    if (l && !l->loop->mut)
        qsem_say(S, at, name, " is borrowed by the for over it: it can't change here", nullptr);

    return !l; /* a for mut's: qsem_name says so */
}

static uint32_t qsem_at(const QSem *const S, const QTok *const t)
{
    return (uint32_t)(t - S->prog->src.toks);
}

/* Value e of a type that owns a list, taken whole: a local's name moves (qsem_moved checks it
   later), leaving its fn when it returns; any other place is copied, which copy() writes out. */
static QExpr *qsem_owned(QSem *const S, QExpr *const e, const bool returned)
{
    if (!qsem_owns(S, e->type) || !qast_place(e))
        return e;

    QVar *const v = e->kind == QE_NAME ? e->var : nullptr;

    if (!v || (v->kind != QV_LET && v->kind != QV_VAR)) {
        qsem_say(S, e->tok, "only a whole let or var moves: write copy(..)", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (returned && v->fn)
        return e;

    S->moves = qast_room(S->mem, S->moves, S->nmoves, &S->capmoves, sizeof *S->moves);
    S->moves[S->nmoves++] = (QMove){
        .e = e,
        .var = v,
        .at = qsem_at(S, e->tok),
        .loop = S->nloops ? S->loops[S->nloops - 1] : UINT32_MAX,
    };

    return e;
}

/* Each move: its variable not used after it, nor moved in a loop it is declared outside. */
static void qsem_moved(QSem *const S)
{
    for (uint32_t i = 0; i < S->nmoves; i++) {
        const QMove *const m = &S->moves[i];
        const char *const name = qsem_text(S, m->var->name);
        const char *const why = m->var->last > m->at ? " is used after it moves here"
                                : m->var->shared     ? " is seen by a fn: it can't move"
                                : m->loop != UINT32_MAX && qsem_at(S, m->var->name) < m->loop
                                    ? " moves in a loop it is declared outside"
                                    : nullptr;

        if (why)
            qsem_say(S, m->e->tok, name, why, ": write copy(", name, ")", nullptr);
    }
}

/* Does e, or any part of it, hit(part, v)? */
static bool qsem_any(
    const QExpr *const e,
    bool (*const hit)(const QExpr *, const QVar *),
    const QVar *const v
)
{
    if (!e)
        return false;
    if (hit(e, v) || qsem_any(e->a, hit, v) || qsem_any(e->b, hit, v) || qsem_any(e->c, hit, v))
        return true;

    for (uint32_t k = 0; k < e->nargv; k++)
        if (qsem_any(e->argv[k], hit, v))
            return true;

    for (uint32_t k = 0; e->kind != QE_NEW && k < e->n; k++)
        if (qsem_any(e->items[k], hit, v))
            return true;

    return false;
}

static bool qsem_names(const QExpr *const e, const QVar *const v)
{
    return e->kind == QE_NAME && e->var == v;
}

static bool qsem_calls(const QExpr *const e, const QVar *const)
{
    return e->kind == QE_CALL || e->kind == QE_LIST;
}

/* Is variable v read in e? */
static bool qsem_mentions(const QExpr *const e, const QVar *const v)
{
    return qsem_any(e, qsem_names, v);
}

static bool qsem_byplace(const QSem *const S, const QType t)
{
    return qast_compound(t) && !qast_tag(S->prog, t);
}

/* A record's or list's value e built straight into variable v's place, unless it reads v or calls
   a fn that may see v (a module's): then in a temporary first. */
static QExpr *qsem_into(QSem *const S, const QVar *const v, QExpr *const e)
{
    if (!qsem_byplace(S, e->type) || !qast_fresh(e) || e->var)
        return e;

    const bool call = e->kind == QE_CALL || e->kind == QE_LIST;

    if (qsem_mentions(e, v) || (call && (!v || !v->fn)))
        e->var = qsem_var(S, e->tok, e->type, QV_TEMP);

    return e;
}

/* A record or list that is no place, where its address is wanted: built in a temporary. */
static QExpr *qsem_addressed(QSem *const S, QExpr *const e)
{
    if (qsem_byplace(S, e->type) && qast_fresh(e) && !e->var)
        e->var = qsem_var(S, e->tok, e->type, QV_TEMP);

    return e;
}

/* A value for a place of type to: converted, a move noted. */
static QExpr *qsem_value(QSem *const S, QExpr *const e, const QType to)
{
    return qsem_owned(S, qsem_coerce(S, qsem_expr(S, e, to), to), false);
}

/* ---- records, enums, tuples */

/* e, a QE_CALL with args or a name alone, made a QE_NEW of type t: its n fields' values by
   position or by name, a default or zero where left out; all: each needed. */
static QExpr *qsem_build(
    QSem *const S,
    QExpr *const e,
    const QType t,
    const QField *const fields,
    const uint32_t n,
    const QTok *const who,
    const bool all
)
{
    const QArg *const args = e->kind == QE_CALL ? e->args : nullptr;
    const QTok **const names = qast_alloc(S->mem, (n ? n : 1) * sizeof *names);

    for (uint32_t k = 0; k < n; k++)
        names[k] = fields[k].name;

    const QArg **const given = qsem_given(S, args, names, n, who, " has no field ");
    e->kind = QE_NEW;
    e->type = t;
    e->nargv = n;
    e->argv = qast_alloc(S->mem, (n ? n : 1) * sizeof *e->argv);

    for (const QArg *a = args; a; a = a->next)
        if (a->mut)
            qsem_say(S, a->mut, "a field takes a value, not mut", nullptr);

    for (uint32_t k = 0; k < n; k++) {
        if (given[k]) {
            e->argv[k] = qsem_value(S, given[k]->e, fields[k].type);
            continue;
        }

        if (all && !fields[k].def)
            qsem_say(
                S,
                e->tok,
                qsem_text(S, who),
                " needs a value for ",
                qsem_text(S, names[k]),
                nullptr
            );

        e->argv[k] = fields[k].def; /* null: zero */
    }

    return e;
}

/* Enum t's value e->tok, call its data's: a plain enum's an int, one with data a QE_NEW. */
static QExpr *qsem_variant(QSem *const S, QExpr *const e, const QType t, const bool call)
{
    const QTy *const y = qast_ty(S->prog, t);
    const char *const name = qsem_text(S, y->name);
    e->type = QY_ERROR;

    if (y->kind != QT_ENUM) {
        qsem_say(S, e->tok, name, " is a type: ", name, "(..) makes one", nullptr);
        return e;
    }

    uint32_t k = 0;

    while (k < y->nvars && !qsem_same(y->vars[k].name, e->tok))
        k++;

    if (k == y->nvars) {
        qsem_say(S, e->tok, name, " has no value ", qsem_text(S, e->tok), nullptr);
        return e;
    }

    const QVariant *const v = &y->vars[k];

    if (call && !v->n) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " carries no data", nullptr);
        return e;
    }

    if (!y->data)
        return qsem_const(S, e->tok, t, (QVal){.i = k});

    if (v->n && !call) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " carries data: give it in (..)", nullptr);
        return e;
    }

    e->field = k;
    return qsem_build(S, e, t, v->fields, v->n, v->name, true);
}

static bool qsem_isrec(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->kind == QT_REC;
}

/* a.f: a record's field; Name.v, an enum's value. */
static QExpr *qsem_field(QSem *const S, QExpr *const e)
{
    if (e->a->kind == QE_NAME) {
        const QBind *const b = qsem_find(S, e->a->tok);

        if (b && b->ty)
            return qsem_variant(S, e, b->ty, false);
    }

    const QExpr *const a = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    e->type = QY_ERROR;

    if (a->type == QY_ERROR)
        return e;

    if (!qsem_isrec(S, a->type)) {
        qsem_say(S, e->tok, qsem_aname(S, a->type), " has no fields", nullptr);
        return e;
    }

    const QTy *const y = qast_ty(S->prog, a->type);

    for (uint32_t k = 0; k < y->nfields; k++)
        if (qsem_same(y->fields[k].name, e->tok)) {
            e->field = k;
            e->type = y->fields[k].type;
            return e;
        }

    qsem_say(S, e->tok, qsem_text(S, y->name), " has no field ", qsem_text(S, e->tok), nullptr);
    return e;
}

/* a[i]: a list's or an array's element; a constant i past an array's, an error. */
/* A slice's end, an int; null left out. */
static QExpr *qsem_end(QSem *const S, QExpr *const e)
{
    return e ? qsem_coerce(S, qsem_expr(S, e, QY_INT), QY_INT) : nullptr;
}

/* s[a..b], a[a..b]: a new string, or a new list of copies, of the elements from a up to b, each
   end clamped to them; a left out is 0, b the length. */
static QExpr *qsem_slice(QSem *const S, QExpr *const e)
{
    QExpr *const a = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    e->b = qsem_end(S, e->b);
    e->c = qsem_end(S, e->c);
    e->kind = QE_CALL;
    e->builtin = QB_SLICE;
    e->type = QY_ERROR;

    if (a->type == QY_ERROR || (e->b && e->b->type == QY_ERROR) || (e->c && e->c->type == QY_ERROR))
        return e;

    if (a->type != QY_STRING &&
        (!qast_compound(a->type) || qast_ty(S->prog, a->type)->kind != QT_LIST)) {
        qsem_say(
            S,
            e->tok,
            "a slice takes a string or a list, not ",
            qsem_aname(S, a->type),
            nullptr
        );
        return e;
    }

    e->type = a->type == QY_STRING ? QY_STRING : qsem_listof(S, qast_ty(S->prog, a->type)->elem, 0);
    return e;
}

static QExpr *qsem_index(QSem *const S, QExpr *const e)
{
    const QExpr *const a = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));

    if (a->type == QY_STRING) { /* a byte, read only */
        e->b = qsem_coerce(S, qsem_expr(S, e->b, QY_INT), QY_INT);
        e->kind = QE_CALL;
        e->builtin = QB_BYTE;
        e->type = e->b->type == QY_ERROR ? QY_ERROR : QY_INT;
        return e;
    }

    if (qsem_ismap(S, a->type)) {
        const QTy *const y = qast_ty(S->prog, a->type);
        e->b = qsem_coerce(S, qsem_expr(S, e->b, y->key), y->key);
        e->type = e->b->type == QY_ERROR ? QY_ERROR : y->elem;
        return e;
    }

    const QExpr *const i = e->b = qsem_coerce(S, qsem_expr(S, e->b, QY_INT), QY_INT);
    e->type = QY_ERROR;

    if (a->type == QY_ERROR || i->type == QY_ERROR)
        return e;

    if (!qast_array(S->prog, a->type)) {
        qsem_say(S, e->tok, qsem_aname(S, a->type), " has no elements", nullptr);
        return e;
    }

    const QTy *const y = qast_ty(S->prog, a->type);

    if (qsem_isconst(i) && (i->val.i < 0 || (y->count && i->val.i >= y->count)))
        qsem_say(S, i->tok, "the index is past the elements", nullptr);

    e->type = y->elem;
    return e;
}

/* (a, b) for a place of type hint, a tuple's: a QE_NEW of it; else of its items' types. */
static QExpr *qsem_tuple(QSem *const S, QExpr *const e, const QType hint)
{
    const bool fits = qast_compound(hint) && qast_ty(S->prog, hint)->kind == QT_TUPLE &&
                      qast_ty(S->prog, hint)->nfields == e->n;
    const QField *const fields = fits ? qast_ty(S->prog, hint)->fields : nullptr;
    QType *const items = qast_alloc(S->mem, e->n * sizeof *items);

    for (uint32_t k = 0; k < e->n; k++) {
        QExpr *const v =
            fields ? qsem_value(S, e->items[k], fields[k].type)
                   : qsem_owned(S, qsem_settle(S, qsem_expr(S, e->items[k], QY_VOID)), false);
        e->items[k] = v;
        items[k] = v->type;

        if (v->type == QY_ERROR || v->type == QY_VOID) {
            e->type = QY_ERROR;
            return e;
        }
    }

    e->type = fields ? hint : qsem_tupleof(S, items, e->n);
    e->kind = QE_NEW;
    e->argv = e->items;
    e->nargv = e->n;
    e->n = 0;
    return e;
}

/* ---- lists */

/* A list's method: a.push(x), a.pop(), a.insert(i, x), a.remove(i); a mutable list. */
static QExpr *qsem_listop(QSem *const S, QExpr *const e, const QBuiltin op)
{
    static const uint32_t counts[] =
        {[QB_PUSH] = 1, [QB_POP] = 0, [QB_INSERT] = 2, [QB_REMOVE] = 1};

    static const char *const say[] = {
        [QB_PUSH] = "push() takes a value",
        [QB_POP] = "pop() takes no values",
        [QB_INSERT] = "insert() takes an index and a value",
        [QB_REMOVE] = "remove() takes an index",
    };

    const bool ok = qsem_changes(S, e->a, e->tok);
    QExpr *const a = e->a = qsem_expr(S, e->a, QY_VOID);
    e->kind = QE_LIST;
    e->builtin = op;
    e->type = QY_ERROR;
    uint32_t n = 0;

    for (const QArg *x = e->args; x; x = x->next)
        n++;

    if (!ok || a->type == QY_ERROR)
        return e;

    if (qsem_ismap(S, a->type) && op == QB_REMOVE && n == 1 && qsem_plain(S, e, ~0u)) {
        const QType key = qast_ty(S->prog, a->type)->key;
        e->argv = qast_alloc(S->mem, sizeof *e->argv);
        e->nargv = 1;
        e->argv[0] = qsem_coerce(S, qsem_expr(S, e->args->e, key), key);
        e->type = QY_VOID;
        return e;
    }

    if (!qast_compound(a->type) || qast_ty(S->prog, a->type)->kind != QT_LIST) {
        const bool array = qast_array(S->prog, a->type);
        qsem_say(
            S,
            e->tok,
            array ? "an array's size is fixed" : "only a list has ",
            array ? "" : qsem_text(S, e->tok),
            array ? "" : "()",
            nullptr
        );
        return e;
    }

    if (n != counts[op] || !qsem_plain(S, e, ~0u)) {
        qsem_say(S, e->tok, say[op], nullptr);
        return e;
    }

    const QType elem = qast_ty(S->prog, a->type)->elem;
    e->argv = qast_alloc(S->mem, 2 * sizeof *e->argv);
    e->nargv = n;
    const QArg *x = e->args;

    if (op == QB_INSERT || op == QB_REMOVE) {
        e->argv[0] = qsem_coerce(S, qsem_expr(S, x->e, QY_INT), QY_INT);
        x = x->next;
    }

    if (op == QB_PUSH || op == QB_INSERT)
        e->argv[n - 1] = qsem_addressed(S, qsem_value(S, x->e, elem));

    e->type = op == QB_POP ? elem : QY_VOID;
    return e;
}

/* Map m's keys, the list its record's first field. */
static QExpr *qsem_keysof(QSem *const S, QExpr *const m)
{
    QExpr *const f = qast_expr(S->mem, QE_FIELD, m->tok);
    f->a = m;
    f->field = 0;
    f->type = qast_ty(S->prog, m->type)->fields[0].type;
    return f;
}

/* m.keys(): a new list of map m's keys, in their order. */
static QExpr *qsem_keys(QSem *const S, QExpr *const e)
{
    QExpr *const m = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    e->type = QY_ERROR;

    if (m->type == QY_ERROR)
        return e;

    if (!qsem_ismap(S, m->type) || e->args) {
        qsem_say(
            S,
            e->tok,
            qsem_ismap(S, m->type) ? "keys() takes no values" : "only a map has keys()",
            nullptr
        );
        return e;
    }

    e->builtin = QB_COPY;
    e->a = qsem_keysof(S, m);
    e->type = e->a->type;
    return e;
}

/* len(a): a list's elements, a map's, a string's bytes, an array's its size. */
static QExpr *qsem_len(QSem *const S, QExpr *const e, QExpr *const recv)
{
    e->type = QY_ERROR;

    if (!recv && !qsem_plain(S, e, 1))
        return e;
    if (recv && e->args) {
        qsem_say(S, e->tok, "len() takes no values", nullptr);
        return e;
    }

    QExpr *a = qsem_addressed(S, qsem_expr(S, recv ? recv : e->args->e, QY_VOID));

    if (a->type == QY_ERROR)
        return e;

    if (qsem_ismap(S, a->type))
        a = qsem_keysof(S, a);

    if (a->type == QY_STRING) {
        e->kind = QE_CALL;
        e->builtin = QB_LEN;
        e->a = a;
        e->type = QY_INT;
        return e;
    }

    if (!qast_array(S->prog, a->type)) {
        qsem_say(
            S,
            a->tok,
            "len() takes a list, a map, a string or an array, not ",
            qsem_aname(S, a->type),
            nullptr
        );
        return e;
    }

    const QTy *const y = qast_ty(S->prog, a->type);

    if (y->kind == QT_ARRAY)
        return qsem_const(S, e->tok, QY_INT, (QVal){.i = y->count});

    e->kind = QE_CALL;
    e->builtin = QB_LEN;
    e->a = a;
    e->type = QY_INT;
    return e;
}

/* copy(x): a copy of x, a record's or a list's made whole. */
static QExpr *qsem_copy(QSem *const S, QExpr *const e)
{
    e->type = QY_ERROR;

    if (!qsem_plain(S, e, 1))
        return e;

    QExpr *const a = qsem_settle(S, qsem_expr(S, e->args->e, QY_VOID));

    if (!qsem_byplace(S, a->type))
        return a;

    e->builtin = QB_COPY;
    e->a = qsem_addressed(S, a);
    e->type = a->type;
    return e;
}

/* a.f(b): a list's method, an enum's value with data, else f(a, b) with mut implied. */
static QExpr *qsem_method(QSem *const S, QExpr *const e)
{
    static const struct {
        const char *name;
        QBuiltin op;
    } lists[] = {{"push", QB_PUSH}, {"pop", QB_POP}, {"insert", QB_INSERT}, {"remove", QB_REMOVE}};

    if (e->a->kind == QE_NAME) {
        const QBind *const b = qsem_find(S, e->a->tok);

        if (b && b->ty)
            return qsem_variant(S, e, b->ty, true);
    }

    const QBind *const b = qsem_find(S, e->tok);

    if (!b || !b->fn) {
        for (uint32_t i = 0; i < countof(lists); i++)
            if (qsem_is(e->tok, lists[i].name))
                return qsem_listop(S, e, lists[i].op);

        if (qsem_is(e->tok, "len"))
            return qsem_len(S, e, e->a);
        if (qsem_is(e->tok, "keys"))
            return qsem_keys(S, e);

        qsem_say(S, e->tok, "unknown fn ", qsem_text(S, e->tok), nullptr);
        e->type = QY_ERROR;
        return e;
    }

    QArg *const self = qast_alloc(S->mem, sizeof *self);
    self->e = e->a;
    self->next = e->args;

    if (b->fn->npar && b->fn->params[0].mut)
        self->mut = e->tok;

    e->args = self;
    e->a = nullptr;
    return qsem_fncall(S, e, b->fn);
}

/* x in a, a list: a's element type. */
static QExpr *qsem_has(QSem *const S, QExpr *const e)
{
    QExpr *const a = e->b = qsem_addressed(S, qsem_expr(S, e->b, QY_VOID));
    e->kind = QE_HAS;
    e->type = QY_ERROR;

    if (a->type == QY_ERROR) {
        qsem_expr(S, e->a, QY_VOID);
        return e;
    }

    if (qsem_ismap(S, a->type)) {
        const QType key = qast_ty(S->prog, a->type)->key;
        e->a = qsem_coerce(S, qsem_expr(S, e->a, key), key);
        e->type = e->a->type == QY_ERROR ? QY_ERROR : QY_BOOL;
        return e;
    }

    if (!qast_array(S->prog, a->type)) {
        qsem_say(
            S,
            e->tok,
            "in takes a range a..b, a list or a map, not ",
            qsem_aname(S, a->type),
            nullptr
        );
        return e;
    }

    const QType elem = qast_ty(S->prog, a->type)->elem;

    if (qsem_byplace(S, elem)) {
        qsem_say(S, e->tok, "in takes a list of numbers, strings or bools", nullptr);
        return e;
    }

    e->a = qsem_coerce(S, qsem_expr(S, e->a, elem), elem);
    e->type = e->a->type == QY_ERROR ? QY_ERROR : QY_BOOL;
    return e;
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

    const char *const types = t == QY_INT ? "bcdoxXneEfFgG%" : qast_float(t) ? "eEfFgGn%" : "s";
    return p == end || (p + 1 == end && strchr(types, *p));
}

/* f"..": each value a number, a string or a bool, its spec one for its type. */
static QExpr *qsem_fstring(QSem *const S, QExpr *const e)
{
    e->type = QY_STRING;

    for (uint32_t k = 0; k < e->n; k++) {
        if (e->items[k]->tok->kind == QK_FPART)
            continue;

        QExpr *const v = e->items[k] = qsem_settle(S, qsem_expr(S, e->items[k], QY_VOID));
        const QTok *const spec = e->specs[k];

        if (v->type == QY_ERROR) {
            e->type = QY_ERROR;
            continue;
        }

        if (qast_compound(v->type) || v->type == QY_VOID) {
            qsem_say(
                S,
                v->tok,
                "an f-string takes numbers, strings and bools, not ",
                qsem_aname(S, v->type),
                nullptr
            );
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

static QExpr *qsem_call(QSem *const S, QExpr *const e)
{
    if (e->a)
        return qsem_method(S, e);

    const QBind *const b = qsem_find(S, e->tok);

    if (b && b->fn)
        return qsem_fncall(S, e, b->fn);

    if (b && b->ty && qsem_isrec(S, b->ty)) {
        const QTy *const y = qast_ty(S->prog, b->ty);
        return qsem_build(S, e, b->ty, y->fields, y->nfields, y->name, false);
    }

    if (b && b->ty) {
        qsem_say(S, e->tok, "an enum's value is Name.value", nullptr);
        e->type = QY_ERROR;
        return e;
    }

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

    if (qsem_is(e->tok, "len"))
        return qsem_len(S, e, nullptr);
    if (qsem_is(e->tok, "copy"))
        return qsem_copy(S, e);

    for (uint32_t i = 0; i < countof(qsem_later); i++)
        if (qsem_is(e->tok, qsem_later[i])) {
            qsem_say(S, e->tok, qsem_later[i], "() is not supported yet", nullptr);
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

    if (b->ty) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is a type", nullptr);
        return e;
    }

    QVar *const v = b->var;
    QVar *const root = v->place ? qsem_root(v->place) : v;
    const uint32_t at = qsem_at(S, e->tok);
    root->last = at > root->last ? at : root->last;
    root->shared |= S->fn && !root->fn && root->kind != QV_CONST;

    if (v->place) { /* a for's element, a case's datum: the place it names */
        QExpr *const p = qast_alloc(S->mem, sizeof *p);
        *p = *v->place;
        p->tok = e->tok;
        return p;
    }

    const QLoan *const l = qsem_loan(S, v);

    if (l && l->loop->mut) {
        const char *const name = qsem_text(S, e->tok);
        qsem_say(
            S,
            e->tok,
            name,
            " is borrowed by the for mut over it: reach it through ",
            qsem_text(S, l->loop->var->name),
            nullptr
        );
        return e;
    }

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

    if ((e->kind == QE_NAME || e->kind == QE_FIELD || e->kind == QE_INDEX) && e->type != QY_VOID)
        return e; /* a place checked already: a compound assignment's */

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
    if (v && kw != QK_CONST)
        v = qsem_owned(S, v, false);

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

    if (v && kw != QK_CONST)
        d->value = qsem_into(S, d->var, v);

    qsem_bind(S, d->name, d->var, nullptr);
}

/* let q, r = f(): a tuple's items, each a name's; built in s->temp first. */
static void qsem_destructure(QSem *const S, QStmt *const s)
{
    QDecl *last = s->decls;
    uint32_t n = 1;

    for (; last->next; last = last->next)
        n++;

    QExpr *const v = last->value = qsem_settle(S, qsem_expr(S, last->value, QY_VOID));
    const QTy *const y = qast_compound(v->type) ? qast_ty(S->prog, v->type) : nullptr;
    const bool fits = y && y->kind == QT_TUPLE && y->nfields == n;

    if (v->type != QY_ERROR && !fits)
        qsem_say(
            S,
            v->tok,
            qsem_aname(S, v->type),
            " is not ",
            qsem_uint(S, n),
            " values",
            nullptr
        );

    if (fits)
        qsem_addressed(S, v);

    uint32_t k = 0;

    for (QDecl *d = s->decls; d; d = d->next, k++) {
        const QType t = fits ? qast_ty(S->prog, v->type)->fields[k].type : QY_ERROR;
        d->var = qsem_var(S, d->name, t, s->op == QK_LET ? QV_LET : QV_VAR);
        qsem_bind(S, d->name, d->var, nullptr);
    }
}

static void qsem_decls(QSem *const S, QStmt *const s)
{
    if (!s->decls) /* not parsed */
        return;

    bool bare = s->decls->next != nullptr && s->op != QK_CONST;
    QDecl *d = s->decls;

    for (; d->next; d = d->next)
        bare &= !d->type && !d->value;

    if (bare && d->value && !d->type) {
        qsem_destructure(S, s);
        return;
    }

    for (d = s->decls; d; d = d->next)
        qsem_decl(S, d, s->op);
}

/* Unchecked place e, changed: checked, QY_ERROR when it can't change. */
static QExpr *qsem_target(QSem *const S, QExpr *const e, const QTok *const at)
{
    const bool ok = qsem_changes(S, e, at);
    QExpr *const t = qsem_expr(S, e, QY_VOID);

    if (ok && t->type != QY_ERROR && t->builtin == QB_BYTE) {
        qsem_say(S, t->tok, "a string doesn't change: build a new one", nullptr);
        t->type = QY_ERROR;
    }

    if (ok && t->type != QY_ERROR && !qast_place(t)) {
        qsem_say(S, t->tok, "only a variable, its fields and its elements change", nullptr);
        t->type = QY_ERROR;
    }

    if (!ok)
        t->type = QY_ERROR;

    return t;
}

/* a, b = b, a; a, b = f(): the values first, each in a temporary unless every place is a
   variable's (the stack holds them then). */
static void qsem_parallel(QSem *const S, QStmt *const s)
{
    QExpr *const to = s->target;
    bool names = true;

    for (uint32_t k = 0; k < to->n; k++) {
        to->items[k] = qsem_target(S, to->items[k], s->tok);
        names &= to->items[k]->kind == QE_NAME && !qsem_byplace(S, to->items[k]->type);
    }

    if (s->op != QK_EQ) {
        qsem_say(S, s->tok, "a, b = takes =", nullptr);
        return;
    }

    if (s->e->kind != QE_TUPLE) {
        QExpr *const v = s->e = qsem_settle(S, qsem_expr(S, s->e, QY_VOID));
        const QTy *const y = qast_compound(v->type) ? qast_ty(S->prog, v->type) : nullptr;
        bool fits = y && y->kind == QT_TUPLE && y->nfields == to->n;

        for (uint32_t k = 0; fits && k < to->n; k++)
            fits = to->items[k]->type == QY_ERROR || y->fields[k].type == to->items[k]->type;

        if (v->type != QY_ERROR && !fits)
            qsem_say(S, v->tok, qsem_aname(S, v->type), " doesn't fit these places", nullptr);

        if (fits)
            qsem_addressed(S, v);

        return;
    }

    if (s->e->n != to->n) {
        qsem_say(S, s->e->tok, "as many values as places", nullptr);
        return;
    }

    s->temps = names ? nullptr : qast_alloc(S->mem, to->n * sizeof *s->temps);

    for (uint32_t k = 0; k < to->n; k++) {
        QExpr *const v = s->e->items[k] = qsem_value(S, s->e->items[k], to->items[k]->type);

        if (s->temps)
            s->temps[k] = qsem_var(S, v->tok, v->type, QV_TEMP);
    }
}

static QExpr *qsem_nameof(QSem *const S, QVar *const v, const QTok *const tok)
{
    QExpr *const e = qast_expr(S->mem, QE_NAME, tok);
    e->var = v;
    e->type = v->type;
    return e;
}

/* a[i] += v reads and writes a[i]: each subscript of t into a temporary first (s->pre), so it is
   evaluated once. A constant stays; so does a variable, unless v calls a fn that may write it. */
static void qsem_hoist(QSem *const S, QStmt *const s, QExpr *const t)
{
    if (t->kind == QE_FIELD)
        qsem_hoist(S, s, t->a);
    if (t->kind != QE_INDEX)
        return;

    qsem_hoist(S, s, t->a);

    const QExpr *const b = t->b;
    const bool shared = b->kind == QE_NAME && (!b->var->fn || b->var->shared) &&
                        qsem_any(s->e, qsem_calls, nullptr);

    if (b->kind == QE_CONST || (b->kind == QE_NAME && !shared))
        return;

    QStmt *const pre = qast_alloc(S->mem, sizeof *pre);
    pre->kind = QS_ASSIGN;
    pre->op = QK_EQ;
    pre->tok = b->tok;
    pre->target = qsem_nameof(S, qsem_var(S, b->tok, b->type, QV_TEMP), b->tok);
    pre->e = t->b;
    t->b = pre->target;

    QStmt **at = &s->pre;

    while (*at)
        at = &(*at)->next;

    *at = pre;
}

static bool qsem_mapkey(const QSem *const S, const QExpr *const t)
{
    return t->kind == QE_INDEX && qsem_ismap(S, t->a->type);
}

/* m[k] = v: k added when not there. The key and the value first, each in a temporary unless
   simple: finding the key's place may grow the map, which nothing may see half done. */
static void qsem_mapset(QSem *const S, QStmt *const s, QExpr *const t)
{
    t->insert = true;
    QExpr *const v = s->e = qsem_into(S, qsem_root(t), qsem_value(S, s->e, t->type));

    if (!qsem_byplace(S, v->type) && !qsem_isconst(v) && v->kind != QE_NAME) {
        QStmt *const pre = qast_alloc(S->mem, sizeof *pre);
        pre->kind = QS_ASSIGN;
        pre->op = QK_EQ;
        pre->tok = v->tok;
        pre->target = qsem_nameof(S, qsem_var(S, v->tok, v->type, QV_TEMP), v->tok);
        pre->e = v;
        s->e = pre->target;
        s->pre = pre;
    }

    qsem_hoist(S, s, t);
}

static void qsem_assign(QSem *const S, QStmt *const s)
{
    if (s->target->kind == QE_TUPLE) {
        qsem_parallel(S, s);
        return;
    }

    QExpr *const t = s->target = qsem_target(S, s->target, s->tok);

    if (t->type == QY_ERROR) {
        qsem_expr(S, s->e, QY_VOID);
        return;
    }

    if (s->op == QK_EQ && qsem_mapkey(S, t)) {
        qsem_mapset(S, s, t);
        return;
    }

    if (s->op == QK_EQ) {
        s->e = qsem_into(S, qsem_root(t), qsem_value(S, s->e, t->type));
        return;
    }

    if (qsem_byplace(S, t->type)) {
        qsem_say(S, s->tok, qast_spelling(s->op), " takes numbers", nullptr);
        return;
    }

    qsem_hoist(S, s, t);

    static const QKind ops[] = {[QK_ADDEQ - QK_ADDEQ] = QK_PLUS, QK_MINUS, QK_STAR, QK_SLASH};

    QExpr *const x = qast_expr(S->mem, QE_BINARY, s->tok);
    x->op = ops[s->op - QK_ADDEQ];
    x->a = t;
    x->b = s->e;
    s->e = qsem_coerce(S, qsem_expr(S, x, t->type), t->type);
}

static void qsem_cond_of(QSem *const S, QStmt *const s)
{
    s->e = qsem_coerce(S, qsem_expr(S, s->e, QY_BOOL), QY_BOOL);
}

/* for i in a..b [step s]: ints; a constant step not 0. Its limit and step are variables, as
   I.NEXT's (qr/qcg.c); by its step if constant, else 0. */
static void qsem_loop(QSem *const S, const QStmt *const s)
{
    S->loops = qast_room(S->mem, S->loops, S->nloops, &S->caploops, sizeof *S->loops);
    S->loops[S->nloops++] = qsem_at(S, s->tok);
}

/* Variable v of a for, named name, naming list place a's element at the for's index. */
static QVar *qsem_eachvar(QSem *const S, QStmt *const s, const QTok *const name, QExpr *const a)
{
    const QType elem = qast_ty(S->prog, a->type)->elem;
    QExpr *const at = qast_expr(S->mem, QE_INDEX, name);
    at->a = a;
    at->b = qsem_nameof(S, s->index, name);
    at->type = elem;

    QVar *const v = qsem_var(S, name, elem, QV_EACH);
    v->place = at;
    qsem_bind(S, name, v, nullptr);
    return v;
}

static void qsem_lend(QSem *const S, const QVar *const root, const QStmt *const s)
{
    if (!root)
        return;

    S->loans = qast_room(S->mem, S->loans, S->nloans, &S->caploans, sizeof *S->loans);
    S->loans[S->nloans++] = (QLoan){.root = root, .loop = s};
}

/* for k [, v] in m, m a map: k its keys in their order, v their values; m borrowed shared. */
static void qsem_eachkey(QSem *const S, QStmt *const s, const QVar *const root)
{
    QExpr *const a = s->e;
    QExpr *const m = qast_fresh(a) && a->var ? qsem_nameof(S, a->var, a->tok) : a;

    if (s->mut)
        qsem_say(S, s->tok, "a map's keys don't change: assign m[k] for its values", nullptr);

    s->by = 1;
    s->stp = qsem_var(S, s->tok, QY_INT, QV_TEMP);
    s->lim = qsem_var(S, s->tok, QY_INT, QV_TEMP);
    s->index = qsem_var(S, s->tok, QY_INT, QV_TEMP);
    qsem_push(S);

    QExpr *const keys = qsem_keysof(S, m);
    s->var = qsem_eachvar(S, s, s->decls->name, keys);

    if (s->decls->next) {
        QExpr *const vals = qsem_keysof(S, m);
        vals->field = 1;
        vals->type = qast_ty(S->prog, m->type)->fields[1].type;
        qsem_eachvar(S, s, s->decls->next->name, vals);
    }

    qsem_lend(S, root, s);
}

/* for [i,] x in a, for mut x in a: a list or an array, borrowed by the loop; x names a[i], i
   counting as a range's for does. */
static void qsem_each(QSem *const S, QStmt *const s)
{
    const QVar *const root = qsem_rawroot(S, s->e);
    const bool ok = !s->mut || qsem_changes(S, s->e, s->tok);
    QExpr *const a = s->e = qsem_addressed(S, qsem_expr(S, s->e, QY_VOID));
    QType elem = QY_ERROR;

    if (qsem_ismap(S, a->type)) {
        qsem_eachkey(S, s, root);
        return;
    }

    if (a->type != QY_ERROR && !qast_array(S->prog, a->type))
        qsem_say(
            S,
            a->tok,
            "for takes a range a..b or a list, not ",
            qsem_aname(S, a->type),
            nullptr
        );
    else if (a->type != QY_ERROR && ok)
        elem = qast_ty(S->prog, a->type)->elem;

    s->by = 1;
    s->stp = qsem_var(S, s->tok, QY_INT, QV_TEMP);
    s->lim = qsem_var(S, s->tok, QY_INT, QV_TEMP);
    qsem_push(S);

    const QDecl *const x = s->decls->next ? s->decls->next : s->decls;
    s->index = s->decls->next ? qsem_var(S, s->decls->name, QY_INT, QV_LOOP)
                              : qsem_var(S, s->tok, QY_INT, QV_TEMP);

    if (s->decls->next)
        qsem_bind(S, s->decls->name, s->index, nullptr);

    QExpr *const at = qast_expr(S->mem, QE_INDEX, x->name);
    at->a = qast_fresh(a) && a->var ? qsem_nameof(S, a->var, a->tok) : a;
    at->b = qsem_nameof(S, s->index, x->name);
    at->type = elem;
    s->var = qsem_var(S, x->name, elem, QV_EACH);
    s->var->mut = s->mut;
    s->var->place = at;
    qsem_bind(S, x->name, s->var, nullptr);
    qsem_lend(S, root, s);
}

static void qsem_for(QSem *const S, QStmt *const s)
{
    qsem_loop(S, s);

    if (s->each) {
        qsem_each(S, s);
        return;
    }

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
            qsem_say(S, s->tok, "return needs ", qsem_aname(S, f->result), nullptr);

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

    s->e = qsem_owned(S, qsem_coerce(S, qsem_expr(S, s->e, f->result), f->result), true);
    s->e = qsem_into(S, f->res, s->e);
}

/* ---- match */

static bool qsem_isdata(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->kind == QT_ENUM && qast_ty(S->prog, t)->data;
}

/* match e: a number, a string, a bool or an enum's; a variable's read where it is, else held in
   s->temp; an enum with data's tag held, its data read at its place. */
static void qsem_match(QSem *const S, QStmt *const s)
{
    QExpr *const v = s->e = qsem_settle(S, qsem_expr(S, s->e, QY_VOID));
    const QType t = s->ty = v->type;
    S->matches = qast_room(S->mem, S->matches, S->nmatches, &S->capmatches, sizeof *S->matches);
    QOpenMatch *const m = &S->matches[S->nmatches++];
    const uint32_t n = qast_compound(t) && qast_ty(S->prog, t)->kind == QT_ENUM
                           ? qast_ty(S->prog, t)->nvars
                       : t == QY_BOOL ? 2
                                      : 0;
    *m = (QOpenMatch){.s = s, .seen = qast_alloc(S->mem, n + 1), .nseen = n};

    qsem_push(S);

    if (t == QY_ERROR || t == QY_VOID) {
        if (t == QY_VOID)
            qsem_mismatch(S, v, QY_INT);

        s->ty = QY_ERROR;
        return;
    }

    if (qsem_byplace(S, t) && !qsem_isdata(S, t)) {
        qsem_say(
            S,
            v->tok,
            "match takes a number, a string, a bool or an enum, not ",
            qsem_aname(S, t),
            nullptr
        );
        s->ty = QY_ERROR;
        return;
    }

    if (qsem_isdata(S, t)) {
        s->place = qast_fresh(qsem_addressed(S, v)) ? qsem_nameof(S, v->var, v->tok) : v;
        s->temp = qsem_var(S, v->tok, QY_INT, QV_TEMP);
        return;
    }

    if (v->kind != QE_NAME)
        s->temp = qsem_var(S, v->tok, t, QV_TEMP);
}

/* A data enum's pattern: Name.v, v, Name.v(a, b), v(a, b): its value, its data's names bound to
   its fields at the match's place. */
static void qsem_varpat(QSem *const S, const QOpenMatch *const m, QPat *const pt, const bool alone)
{
    const QExpr *const a = pt->a;
    const QTy *const y = qast_ty(S->prog, m->s->ty);
    const bool named = (a->kind == QE_CALL && !a->a) || a->kind == QE_NAME;
    const bool dotted = (a->kind == QE_CALL || a->kind == QE_FIELD) && a->a &&
                        a->a->kind == QE_NAME && qsem_same(a->a->tok, y->name);

    if (pt->kind != QP_VALUE || (!named && !dotted)) {
        qsem_say(S, pt->tok, "a case of ", qsem_text(S, y->name), " is one of its values", nullptr);
        return;
    }

    uint32_t k = 0;

    while (k < y->nvars && !qsem_same(y->vars[k].name, a->tok))
        k++;

    if (k == y->nvars) {
        qsem_say(S, a->tok, qsem_text(S, y->name), " has no value ", qsem_text(S, a->tok), nullptr);
        return;
    }

    pt->kind = QP_VARIANT;
    pt->variant = k;
    m->seen[k] = 1;
    const QVariant *const v = &y->vars[k];
    uint32_t n = 0;

    for (const QArg *x = a->kind == QE_CALL ? a->args : nullptr; x; x = x->next, n++) {
        if (x->name || x->mut || x->e->kind != QE_NAME || n >= v->n) {
            qsem_say(
                S,
                x->e->tok,
                n >= v->n ? "more names than its data" : "a case takes its data's names",
                nullptr
            );
            return;
        }

        if (!alone) {
            qsem_say(S, x->e->tok, "a case that names data has one pattern", nullptr);
            return;
        }

        QExpr *const f = qast_expr(S->mem, QE_FIELD, x->e->tok);
        f->a = m->s->place;
        f->field = v->first + n;
        f->type = y->fields[f->field].type;
        QVar *const d = qsem_var(S, x->e->tok, f->type, QV_LET);
        d->place = f;
        qsem_bind(S, x->e->tok, d, nullptr);
    }

    if (n && n != v->n)
        qsem_say(S, a->tok, qsem_text(S, a->tok), " has more data: name each", nullptr);
}

/* A case's pattern for match m: a value of its type, a to b, is op v. */
static void qsem_pat(QSem *const S, QOpenMatch *const m, QPat *const pt, const bool alone)
{
    const QType t = m->s->ty;

    if (t == QY_ERROR)
        return;

    if (qsem_isdata(S, t)) {
        qsem_varpat(S, m, pt, alone);
        return;
    }

    pt->a = qsem_coerce(S, qsem_expr(S, pt->a, t), t);

    if (pt->b)
        pt->b = qsem_coerce(S, qsem_expr(S, pt->b, t), t);

    const bool ordered = qast_numeric(t) || t == QY_STRING;

    if (pt->kind == QP_RANGE && !ordered)
        qsem_say(S, pt->tok, "to takes numbers or strings", nullptr);
    if (pt->kind == QP_IS && !ordered && pt->op != QK_EQ && pt->op != QK_NE)
        qsem_say(S, pt->tok, qast_spelling(pt->op), " takes numbers or strings", nullptr);

    if (pt->kind == QP_VALUE && m->nseen && qsem_isconst(pt->a) && pt->a->val.i >= 0 &&
        (uint64_t)pt->a->val.i < m->nseen)
        m->seen[pt->a->val.i] = 1;
}

static void qsem_case(QSem *const S, QStmt *const s)
{
    qsem_pop(S);
    qsem_push(S);

    if (!S->nmatches)
        return;

    QOpenMatch *const m = &S->matches[S->nmatches - 1];
    m->other |= !s->pats;

    for (QPat *pt = s->pats; pt; pt = pt->next)
        qsem_pat(S, m, pt, pt == s->pats && !pt->next);
}

/* end match: each value has its case, or there is a case else. */
static void qsem_endmatch(QSem *const S, const QStmt *const s)
{
    qsem_pop(S);

    if (!S->nmatches)
        return;

    QOpenMatch *const m = &S->matches[--S->nmatches];
    bool all = m->nseen > 0;
    uint32_t miss = 0;

    for (uint32_t k = 0; k < m->nseen; k++)
        if (!m->seen[k] && all) {
            all = false;
            miss = k;
        }

    m->s->full = m->other || all;

    if (m->s->full || m->s->ty == QY_ERROR)
        return;

    const char *const what = m->s->ty == QY_BOOL ? (miss ? "true" : "false")
                             : m->nseen ? qsem_text(S, qast_ty(S->prog, m->s->ty)->vars[miss].name)
                                        : nullptr;

    if (what)
        qsem_say(
            S,
            s->tok,
            "the match misses ",
            what,
            ": give it a case, or add case else",
            nullptr
        );
    else
        qsem_say(S, s->tok, "the match misses values: add case else", nullptr);
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
        s->e = qsem_addressed(S, qsem_expr(S, s->e, QY_VOID));
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

    if (last->kind == QS_ENDMATCH) {
        for (const QStmt *c = last->link->link; c != last; c = c->link)
            if (!qsem_returns(c->link->prev))
                return false;

        return last->link->full;
    }

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
        qsem_loop(S, s);
        qsem_cond_of(S, s);
        qsem_push(S);
        return;

    case QS_DO:
        qsem_loop(S, s);
        qsem_push(S);
        return;

    case QS_FOR:
        qsem_for(S, s);
        return;

    case QS_MATCH:
        qsem_match(S, s);
        return;

    case QS_CASE:
        qsem_case(S, s);
        return;

    case QS_ENDMATCH:
        qsem_endmatch(S, s);
        return;

    case QS_ENDFOR:
        if (S->nloans && S->loans[S->nloans - 1].loop == s->link)
            S->nloans--;

        [[fallthrough]];

    case QS_ENDWHILE:
    case QS_ENDDO:
        S->nloops -= S->nloops > 0;
        [[fallthrough]];

    case QS_ENDIF:
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
                qsem_aname(S, f->result),
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
                           s->kind == QS_DO || s->kind == QS_FOR || s->kind == QS_MATCH;
        const bool closes = s->kind == QS_ENDIF || s->kind == QS_ENDWHILE || s->kind == QS_ENDDO ||
                            s->kind == QS_ENDFOR || s->kind == QS_ENDMATCH;

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

/* A field of type t declared: d's name, type and default. */
static void qsem_addfield(QSem *const S, const QType t, QDecl *const d)
{
    QTy *const y = qast_ty(S->prog, t);

    for (uint32_t k = 0; k < y->nfields; k++)
        if (qsem_same(y->fields[k].name, d->name))
            qsem_say(S, d->name, qsem_text(S, d->name), " is already a field", nullptr);

    y->fields = qast_room(S->mem, y->fields, y->nfields, &y->capfields, sizeof *y->fields);
    y->fields[y->nfields++] = (QField){.name = d->name, .tx = d->type, .def = d->value};
}

/* An enum's value: its name, then its data's fields. */
static void qsem_addvariant(QSem *const S, const QType t, const QDecl *const d)
{
    QTy *const y = qast_ty(S->prog, t);

    for (uint32_t k = 0; k < y->nvars; k++)
        if (qsem_same(y->vars[k].name, d->name))
            qsem_say(S, d->name, qsem_text(S, d->name), " is already a value", nullptr);

    y->vars = qast_room(S->mem, y->vars, y->nvars, &y->capvars, sizeof *y->vars);
    QVariant *const v = &y->vars[y->nvars++];
    *v = (QVariant){.name = d->name};

    for (const QDecl *x = d->next; x; x = x->next)
        v->n++;

    v->fields = qast_alloc(S->mem, (v->n ? v->n : 1) * sizeof *v->fields);
    uint32_t k = 0;

    for (const QDecl *x = d->next; x; x = x->next)
        v->fields[k++] = (QField){.name = x->name, .tx = x->type, .def = x->value};

    y->data |= v->n > 0;
}

/* Each type and enum: a type, its name bound, its fields and values declared. */
static void qsem_types(QSem *const S)
{
    for (QStmt *s = S->prog->first; s; s = s->next) {
        if (s->kind != QS_TYPE && s->kind != QS_ENUM)
            continue;

        const QType t = s->ty = qsem_newty(S, s->kind == QS_TYPE ? QT_REC : QT_ENUM);
        QTy *const y = qast_ty(S->prog, t);
        y->name = s->decls->name;
        y->def = s;
        y->state = 0;
        qsem_bindty(S, s->decls->name, t);

        for (QStmt *m = s->next; m && m->kind != QS_ENDTYPE && m->kind != QS_ENDENUM; m = m->next)
            if (m->kind == QS_FIELD)
                for (QDecl *d = m->decls; d; d = d->next)
                    qsem_addfield(S, t, d);
            else if (m->kind == QS_VARIANT)
                qsem_addvariant(S, t, m->decls);

        if (s->kind == QS_ENUM && !qast_ty(S->prog, t)->nvars)
            qsem_say(S, s->tok, "an enum needs a value", nullptr);
    }
}

static void qsem_typed(QSem *S, QType t);

/* Field f of type t typed: its type, else its default's; a record by value typed first, never
   t itself. */
static void qsem_fieldtype(QSem *const S, const QType t, QField *const f)
{
    QType ft = f->tx ? qsem_type(S, f->tx) : QY_VOID;

    if (f->def) {
        QExpr *d = qsem_expr(S, f->def, ft);
        d = f->tx ? qsem_coerce(S, d, ft) : qsem_settle(S, d);

        if (!qsem_isconst(d) && d->type != QY_ERROR)
            qsem_say(S, d->tok, "a default is a constant", nullptr);
        if (!f->tx)
            ft = d->type;

        f->def = d;
    }

    if (!f->tx && !f->def) {
        qsem_say(S, f->name, qsem_text(S, f->name), " needs a type", nullptr);
        ft = QY_ERROR;
    }

    const QType inner = qast_compound(ft) && qast_ty(S->prog, ft)->kind == QT_ARRAY
                            ? qast_ty(S->prog, ft)->elem
                            : ft;

    if (qast_record(S->prog, inner) &&
        (!qast_compound(ft) || qast_ty(S->prog, ft)->kind != QT_LIST)) {
        qsem_typed(S, inner);

        if (qast_ty(S->prog, inner)->state == 1) {
            qsem_say(
                S,
                f->name,
                qsem_text(S, f->name),
                " holds its own type: hold it in a list",
                nullptr
            );
            ft = QY_ERROR;
        }
    }

    f->type = ft;
    qast_ty(S->prog, t)->owns |= qsem_owns(S, ft);
}

/* Type t's fields typed: an enum with data's its tag, then each value's data. */
static void qsem_typed(QSem *const S, const QType t)
{
    QTy *const y = qast_ty(S->prog, t);

    if (y->state)
        return;

    y->state = 1;

    if (y->kind == QT_ENUM && y->data) {
        uint32_t n = 1;

        for (uint32_t k = 0; k < y->nvars; k++)
            n += y->vars[k].n;

        QField *const fields = qast_alloc(S->mem, n * sizeof *fields);
        fields[0] = (QField){.name = y->name, .type = QY_INT};

        n = 1;

        for (uint32_t k = 0; k < y->nvars; k++) {
            QVariant *const v = &y->vars[k];
            v->first = n;

            for (uint32_t i = 0; i < v->n; i++)
                fields[n++] = v->fields[i];

            v->fields = &fields[v->first];
        }

        y->fields = fields;
        y->nfields = y->capfields = n;
    }

    for (uint32_t k = 0; k < qast_ty(S->prog, t)->nfields; k++)
        if (k || qast_ty(S->prog, t)->kind == QT_REC)
            qsem_fieldtype(S, t, &qast_ty(S->prog, t)->fields[k]);

    qast_ty(S->prog, t)->state = 2;
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
    f->byarg = qsem_byplace(S, f->result);

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

    qsem_types(&S);
    qsem_consts(&S);

    for (uint32_t i = 0; i < p->tys.n; i++)
        qsem_typed(&S, QY_FIRST + i);

    for (QFn *f = p->fns; f; f = f->next)
        qsem_signature(&S, f);

    qsem_module(&S);

    while (S.nmarks)
        qsem_pop(&S);

    for (QFn *f = p->fns; f; f = f->next)
        qsem_body(&S, f);

    qsem_moved(&S);
}
