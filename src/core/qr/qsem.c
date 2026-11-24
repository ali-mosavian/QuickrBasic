/* Quickr's semantic pass: names resolved, types checked, conversions made explicit, constants
 * folded.
 *
 * # How it works
 * Names live on a stack of bindings, a block's scope a mark into it. A module's own names and
 * those its imports bind are its scope's: a name not on the stack is looked up in the scope of
 * the module its token is written in, so a generic's or a lambda's body sees its own module's
 * wherever it is checked. m.x is x of m's scope, if public; from m import X binds an alias, X
 * found in m's scope when used. The passes:
 *   1. each module's imports; each fn's and type's name; then each module's consts, in order;
 *      then each type's fields and each fn's signature, their defaults folded with those consts
 *      in sight;
 *   2. each module's statements, a module after those it imports, fn bodies skipped: a var or
 *      let is seen from its line on;
 *   3. each fn's body, its module's names (its fns, consts, vars and lets) in sight;
 *   4. each from's names checked public, each move against the uses after it.
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
#include "qr/qnat.h"
#include "qr/qpar.h"
#include <stdarg.h>

enum { QSEM_SAY = 160 }; /* an error's text at most */

static const double QSEM_FLT_MAX = 3.4028234663852886e38;

/* A name: a variable's, a fn's, a type's, or a module's (scope its QScope + 1); from m import X's
   alias, X found in m's scope when used. */
typedef struct {
    const QTok *name;
    QVar *var;
    QFn *fn;
    QType ty; /* a type's name */
    uint32_t scope;
    const QTok *alias;
    bool pub;
} QBind;

/* A module's names, or a package folder's modules (mod QMOD_NONE); ready once its statements are
   checked, its vars bound. */
typedef struct {
    QBind *at;
    uint32_t n, cap;
    uint32_t mod;
    bool ready;
} QScope;

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

/* A lambda being checked at its site: its scope from binding mark on; up, the one it is in. */
typedef struct QLamAt QLamAt;
struct QLamAt {
    QLam *lam;
    uint32_t mark;
    QLamAt *up;
    QBind **views; /* each of lam's captures as its body sees it */
    uint32_t capviews;
};

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
    QLamAt *lamat;           /* the lambda checked at its site, null none */
    uint32_t hidelo, hidehi; /* bindings not seen: a fn's, while a lambda's body is checked in it */
    bool passing; /* an argument for a fn parameter checked: a lambda bound may pass on */
    QStmt *genfor; /* the next generator instance: for x in g(..)'s, inline in this for */
    bool genkept;  /* the next generator instance: var it = g(..)'s, resumable */
    QScope *scopes; /* each module's, by number, then the package folders' */
    uint32_t nscopes, capscopes;
    bool pub; /* a public statement's names bound */
} QSem;

static QExpr *qsem_expr(QSem *S, QExpr *e, QType hint);
static QExpr *qsem_coerce(QSem *S, QExpr *e, QType to);
static void qsem_simple(QSem *S, QStmt *s);
static void qsem_yield(QSem *S, QStmt *s);
static void qsem_kept(QSem *S, QDecl *d, QKind kw, QFn *g);
static void qsem_keep(QSem *S, QFn *r);
static void qsem_fnbody(QSem *S, QFn *f);
static QFn *qsem_genof(QSem *S, const QExpr *e);
static QExpr *qsem_next(QSem *S, QExpr *e, QVar *v);
static QExpr *qsem_iter(QSem *S, QExpr *e, uint32_t k, QStmt *loop);
static void qsem_chainfor(QSem *S, QStmt *s);
static QVar *qsem_keptas(QSem *S, QExpr *e, QFn *g, const QTok *name);
static bool qsem_step(QSem *S, const QExpr *e, uint32_t *k);
static QExpr *qsem_operator(QSem *S, QExpr *e, QKind op, QExpr *a, QExpr *b);
static QExpr *qsem_relation(QSem *S, QExpr *e, QKind op, QExpr *a, QExpr *b);
static bool qsem_hasrel(const QSem *S, QKind op, const QExpr *a, const QExpr *b);
static bool qsem_isrec(const QSem *S, QType t);
static bool qsem_method_of(
    QSem *S,
    const QExpr *e,
    const char *const *names,
    uint32_t n,
    uint32_t *k
);
static const QBind *qsem_find(QSem *S, const QTok *name);
static bool qsem_isconst(const QExpr *e);
static bool qsem_byplace(const QSem *S, QType t);
static QVar *qsem_root(const QExpr *e);
static QExpr *qsem_addressed(QSem *S, QExpr *e);
static bool qsem_changes(QSem *S, const QExpr *e, const QTok *at);
static QExpr *qsem_get(QSem *S, QExpr *e);
static void qsem_signature(QSem *S, QFn *f);
static void qsem_native(QSem *S, QFn *f);
static QExpr *qsem_nameof(QSem *S, QVar *v, const QTok *tok);
static QExpr *qsem_icall(QSem *S, QExpr *e, QExpr *f);
static bool qsem_isrec(const QSem *S, QType t);
static QExpr *qsem_name(QSem *S, QExpr *e);
static QExpr *qsem_called(QSem *S, QExpr *e, const QBind *b);

/* ---- errors */

static const char *qsem_instname(QSem *S, const QFn *f);

/* An error at t: the parts, to a null, joined; in an instance, its name after them. */
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
    const char *const in = S->fn && S->fn->of ? qsem_instname(S, S->fn) : "";
    const uint32_t k = (uint32_t)strlen(in);
    const uint32_t room = k < QSEM_SAY - 1 - n ? k : QSEM_SAY - 1 - n;
    memcpy(text + n, in, room);
    qast_error(&S->prog->errs, t, "{}", text, n + room);
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

    case QT_OPT:
    case QT_RES:
        return qsem_join(
            S,
            qsem_tyname(S, y->fields[QF_VAL].type),
            y->kind == QT_OPT ? "?" : "!",
            ""
        );

    case QT_FN: {
        const char *s = "fn(";

        for (uint32_t k = 0; k < y->nfields; k++)
            s = qsem_join(S, s, k ? ", " : "", qsem_tyname(S, y->fields[k].type));

        s = qsem_join(S, s, ")", "");
        return y->elem == QY_VOID ? s : qsem_join(S, s, " as ", qsem_tyname(S, y->elem));
    }

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
        [QY_BYTE] = "a byte",
        [QY_SHORT] = "a short",
        [QY_LONG] = "a long",
        [QY_UBYTE] = "a ubyte",
        [QY_USHORT] = "a ushort",
        [QY_UINT] = "a uint",
        [QY_ULONG] = "a ulong",
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

/* The module token t is written in. */
static uint32_t qsem_modof(const QSem *const S, const QTok *const t)
{
    return t->line < S->prog->src.nlines ? S->prog->src.lines[t->line].mod : 0;
}

/* b bound in scope sc; an error if its name already is. */
static QBind *qsem_scoped(QSem *const S, QScope *const sc, const QBind b)
{
    for (uint32_t i = 0; i < sc->n; i++)
        if (qsem_same(sc->at[i].name, b.name)) {
            qsem_say(S, b.name, qsem_text(S, b.name), " is already declared", nullptr);
            return nullptr;
        }

    sc->at = qast_room(S->mem, sc->at, sc->n, &sc->cap, sizeof *sc->at);
    sc->at[sc->n] = b;
    return &sc->at[sc->n++];
}

/* b bound in the innermost scope, a module's own outside blocks and fns (a public statement's
   exported); an error if its name already is. */
static QBind *qsem_bound_as(QSem *const S, QBind b)
{
    if (!S->nmarks) {
        b.pub = S->pub;
        return qsem_scoped(S, &S->scopes[qsem_modof(S, b.name)], b);
    }

    for (uint32_t i = S->marks[S->nmarks - 1]; i < S->nbinds; i++)
        if (qsem_same(S->binds[i].name, b.name)) {
            qsem_say(S, b.name, qsem_text(S, b.name), " is already declared", nullptr);
            return nullptr;
        }

    S->binds = qast_room(S->mem, S->binds, S->nbinds, &S->capbinds, sizeof *S->binds);
    S->binds[S->nbinds] = b;
    return &S->binds[S->nbinds++];
}

static void qsem_bind(QSem *const S, const QTok *const name, QVar *const var, QFn *const fn)
{
    qsem_bound_as(S, (QBind){.name = name, .var = var, .fn = fn});
}

/* The bindings every scope sees: the module's own, below its first block's or fn's. */
static uint32_t qsem_floor(const QSem *const S) { return S->nmarks ? S->marks[0] : S->nbinds; }

static const QBind *qsem_capture(QSem *S, QLamAt *c, uint32_t i, const QTok *use);

/* What names nothing, said already. */
static const QBind qsem_spoilt = {.ty = QY_ERROR};

static const QBind *qsem_member(QSem *S, const QBind *m, const QTok *name);
static const QBind *qsem_exported(const QSem *S, uint32_t k, const QTok *name);

/* name in module k's scope, an alias followed; null none. */
static const QBind *qsem_global(QSem *const S, const uint32_t k, const QTok *const name)
{
    const QScope *const sc = &S->scopes[k];

    for (uint32_t i = sc->n; i-- > 0;) {
        const QBind *const b = &sc->at[i];

        if (!qsem_same(b->name, name))
            continue;
        if (!b->alias)
            return b;

        const QBind *const x = qsem_exported(S, b->scope - 1, b->alias);

        if (x && x->pub)
            return x;
        if (!x && !S->scopes[b->scope - 1].ready)
            qsem_say(
                S,
                name,
                qsem_text(S, name),
                " is not there yet: its module's vars start after this one's",
                nullptr
            );

        return &qsem_spoilt; /* else said at its import */
    }

    return nullptr;
}

/* name among scope k's own, not its imports; null none. A package folder's are its modules,
   public each. */
static const QBind *qsem_exported(const QSem *const S, const uint32_t k, const QTok *const name)
{
    const QScope *const sc = &S->scopes[k];

    for (uint32_t i = 0; i < sc->n; i++)
        if (qsem_same(sc->at[i].name, name) && !sc->at[i].alias)
            return &sc->at[i];

    return nullptr;
}

/* name in module m (a module's binding): its public binding, else an error said. In a module
   whose statements are not checked yet, a var is not there. */
static const QBind *qsem_member(QSem *const S, const QBind *const m, const QTok *const name)
{
    const QBind *const b = qsem_exported(S, m->scope - 1, name);
    const QScope *const sc = &S->scopes[m->scope - 1];

    if (b && (b->pub || sc->mod == QMOD_NONE))
        return b;

    if (b) {
        qsem_say(S, name, qsem_text(S, name), " is not public in ", qsem_text(S, m->name), nullptr);
        return &qsem_spoilt;
    }

    if (sc->ready || sc->mod == QMOD_NONE)
        qsem_say(S, name, qsem_text(S, m->name), " has no ", qsem_text(S, name), nullptr);
    else
        qsem_say(
            S,
            name,
            qsem_text(S, name),
            " is not there yet: its module's vars start after this one's",
            nullptr
        );

    return &qsem_spoilt;
}

static const QBind *qsem_find(QSem *const S, const QTok *const name)
{
    for (uint32_t i = S->nbinds; i-- > 0;)
        if ((i < S->hidelo || i >= S->hidehi) && qsem_same(S->binds[i].name, name)) {
            QLamAt *const c = S->lamat;

            if (!c || i >= c->mark || i < qsem_floor(S))
                return &S->binds[i];

            return qsem_capture(S, c, i, name);
        }

    return qsem_global(S, qsem_modof(S, name), name);
}

/* Unchecked e as a name: a name's binding, or m.x's, m a module or a package's: a.b.x. Null
   when e is neither, or names nothing. */
static const QBind *qsem_qual(QSem *const S, const QExpr *const e)
{
    if (e->kind == QE_NAME && !e->a)
        return qsem_find(S, e->tok);
    if (e->kind != QE_FIELD && e->kind != QE_NAME)
        return nullptr;

    const QBind *const m = qsem_qual(S, e->a);
    return m && m->scope && !m->alias ? qsem_member(S, m, e->tok) : nullptr;
}

/* Is unchecked e a module's name, or a package's? */
static bool qsem_ismod(QSem *const S, const QExpr *const e)
{
    const QBind *const b = e->kind == QE_NAME || e->kind == QE_FIELD ? qsem_qual(S, e) : nullptr;
    return b && b->scope && !b->alias;
}

/* What unchecked name e binds: a name's, m.x's (e->a m) once qsem_field made it a name. */
static const QBind *qsem_lookup(QSem *const S, const QExpr *const e)
{
    return e->kind == QE_NAME ? qsem_qual(S, e) : nullptr;
}

static void qsem_bindty(QSem *const S, const QTok *const name, const QType t)
{
    qsem_bound_as(S, (QBind){.name = name, .ty = t});
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

/* ---- captures */

static uint32_t qsem_at(const QSem *S, const QTok *t);
static QVar *qsem_root(const QExpr *e);

/* The kind a capture's hidden parameter takes from v, what it stands for: changeable as v is. */
static QVarKind qsem_capkind(const QVar *const v)
{
    if (v->kind == QV_VAR || v->kind == QV_MUT || (v->kind == QV_EACH && v->mut))
        return QV_MUT;

    return v->kind == QV_EACH ? QV_LET : v->kind;
}

/* Binding i, outside lambda c's scope, as c's body sees it: as it is when no fn's storage (a
   module's, a const, a fn, a type); else c's hidden parameter for it, made on first use. A lambda
   c is in sees it first. The original's last use is use. */
static const QBind *qsem_capture(
    QSem *const S,
    QLamAt *const c,
    const uint32_t i,
    const QTok *const use
)
{
    QLam *const l = c->lam;

    for (uint32_t k = 0; k < l->ncaps; k++)
        if (l->caps[k]->at == i)
            return c->views[k];

    const QBind *const outer =
        c->up && i < c->up->mark ? qsem_capture(S, c->up, i, use) : &S->binds[i];
    QVar *const v = outer->var;

    if (S->binds[i].var) {
        QVar *const orig = S->binds[i].var;
        QVar *const root = orig->place ? qsem_root(orig->place) : orig;
        const uint32_t at = qsem_at(S, use);
        root->last = at > root->last ? at : root->last;
    }

    QBind *const view = qast_alloc(S->mem, sizeof *view);
    *view = *outer;

    if (v && v->lam) { /* a lambda bound to a parameter: no value to reach */
        qsem_say(
            S,
            use,
            qsem_text(S, use),
            " is a lambda: call it outside another lambda",
            nullptr
        );
        *view = (QBind){.name = outer->name, .ty = QY_ERROR};

        return view;
    }

    QCap *const cap = qast_alloc(S->mem, sizeof *cap);
    *cap = (QCap){
        .name = S->binds[i].name,
        .var = v,
        .fn = outer->fn,
        .ty = outer->ty,
        .at = i,
        .frame = v && !v->constant && v->fn,
    };

    if (cap->frame) {
        cap->param = qsem_var(S, cap->name, v->type, qsem_capkind(v));
        cap->param->fn = l->fn;
        cap->param->cap = cap;
        view->var = cap->param;
    }

    l->caps = qast_room(S->mem, l->caps, l->ncaps, &l->capcaps, sizeof *l->caps);
    c->views = qast_room(S->mem, c->views, l->ncaps, &c->capviews, sizeof *c->views);
    c->views[l->ncaps] = view;
    l->caps[l->ncaps++] = cap;
    return view;
}

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
static QType qsem_scalar(const QTok *const t)
{
    for (uint32_t i = 0; i < countof(qsem_scalars); i++)
        if (qsem_is(t, qast_typename(qsem_scalars[i])))
            return qsem_scalars[i];

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

/* T? (QT_OPT) or T! (QT_RES) of val; interned. T?'s fields: has, val; T!'s: err, val, then its
   error's code, message and line. */
static QType qsem_wrapof(QSem *const S, const QTyKind kind, const QType val)
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
static QType qsem_fnof(
    QSem *const S,
    const QType *const params,
    const uint32_t n,
    const QType result
)
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
static bool qsem_wraps(const QSem *const S, const QType t, const QTyKind kind)
{
    if (!qast_compound(t))
        return false;

    const QTyKind k = qast_ty(S->prog, t)->kind;
    return kind == QT_REC ? k == QT_OPT || k == QT_RES : k == kind;
}

static QType qsem_inner(const QSem *const S, const QType t)
{
    return qast_ty(S->prog, t)->fields[QF_VAL].type;
}

static bool qsem_isgen(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->kind == QT_GEN;
}

static bool qsem_ismap(const QSem *const S, const QType t)
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

    qsem_say(S, t, "unknown type ", qsem_text(S, t), nullptr);
    return QY_ERROR;
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

    const QBind *const b = qsem_member(S, m, t);

    if (b->ty)
        return b->ty;

    qsem_say(S, t, qsem_text(S, t), " is not a type", nullptr);
    return QY_ERROR;
}

/* Unchecked name e's type, a map's key's: T, or m.T. */
static QType qsem_nametype(QSem *const S, const QExpr *const e)
{
    if (e->kind == QE_NAME)
        return qsem_typename(S, e->tok);

    const QBind *const b = qsem_qual(S, e);

    if (b && b->ty)
        return b->ty;
    if (!b || b->ty != QY_ERROR)
        qsem_say(S, e->tok, "unknown type ", qsem_text(S, e->tok), nullptr);

    return QY_ERROR;
}

/* Does T[e] name a map's key type, e a type's name? */
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

/* V[K]: a map, K an int, a string or a plain enum. */
static QType qsem_maptype(QSem *const S, const QTypeEx *const x, const QType val)
{
    const QType key = qsem_nametype(S, x->count);

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
        return qsem_qualtype(S, x->quals, x->nquals, x->tok);

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
            qsem_say(S, x->tok, "a T? or T!'s T is neither", nullptr);
            return QY_ERROR;
        }

        return qsem_wrapof(S, x->kind == QX_OPT ? QT_OPT : QT_RES, elem);
    }

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

/* Does integer v, a ulong's bits when t's or from's (unsigned 64), fit integer type t? */
static bool qsem_inrange(const int64_t v, const bool big, const QType t)
{
    const uint32_t bits = qast_bits(t);

    if (big && (uint64_t)v > INT64_MAX)
        return t == QY_ULONG;
    if (qast_unsigned(t))
        return v >= 0 && (bits == 64 || v < (int64_t)1 << bits);

    return bits == 64 || (v >= -((int64_t)1 << (bits - 1)) && v < (int64_t)1 << (bits - 1));
}

/* Integer v kept to integer type t's bits, its sign extended: wrap(..)'s. */
static int64_t qsem_trunc(const int64_t v, const QType t)
{
    const uint32_t bits = qast_bits(t);

    if (bits == 64)
        return v;

    const uint64_t m = ((uint64_t)1 << bits) - 1, u = (uint64_t)v & m;
    return qast_unsigned(t) || !(u >> (bits - 1)) ? (int64_t)u : (int64_t)(u | ~m);
}

/* A numeric constant's value as a double. */
static double qsem_num(const QExpr *const e)
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

/* Can a value of from become a to by itself, losing nothing: a narrower integer to a wider of its
   sign, an unsigned to a wider signed, an integer to a double, a single to a double? */
static bool qsem_widens(const QType from, const QType to)
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
static bool qsem_fits(const QExpr *const e, const QType to)
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
static QType qsem_meet(const QExpr *const a, const QExpr *const b)
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
static QExpr *qsem_fold_conv(QSem *const S, QExpr *const e, const QType to)
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
static QExpr *qsem_settle(QSem *const S, QExpr *const e)
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
static QExpr *qsem_wrap(QSem *S, QType to, QExpr *v);

static QExpr *qsem_coerce(QSem *const S, QExpr *const e, const QType to)
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
static QType qsem_sumof(const QType t) { return qast_integer(t) && qast_bits(t) < 32 ? QY_INT : t; }

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

    case QK_AMP:
        *r = a & b;
        return true;

    case QK_BAR:
        *r = a | b;
        return true;

    case QK_CARET:
        *r = a ^ b;
        return true;

    default: /* << >>: the bits shifted out dropped */
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

/* + - * ** of integer constants of type w (QY_INT for untyped ones), exact; in wrap(..), w's bits
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

    default: { /* **: the VM's, 0 for a negative power but of 1 and -1 */
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

    if (qsem_isrec(S, a->type) && qsem_arith(e->op))
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

    if (e->op == QK_NOT) {
        e->a = qsem_coerce(S, a, QY_BOOL);
        e->type = QY_BOOL;

        if (qsem_isconst(e->a) && e->a->type == QY_BOOL)
            return qsem_bool(S, e->tok, !e->a->val.i);

        return e;
    }

    if (e->op == QK_TILDE) {
        const QType t = qast_integer(a->type) && !a->untyped ? a->type : QY_INT;
        e->a = qsem_coerce(S, a, t);
        e->type = e->a->type == QY_ERROR ? QY_ERROR : t;

        if (qsem_isconst(e->a) && e->type == t)
            return qsem_const(S, e->tok, t, (QVal){.i = qsem_trunc(~e->a->val.i, t)});

        return e;
    }

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
        if (qast_compound(v->type) && !qast_tag(S->prog, v->type))
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
static QExpr *qsem_wrapped_ops(QSem *const S, QExpr *const e)
{
    if (!qsem_plain(S, e, 1)) {
        e->type = QY_ERROR;
        return e;
    }

    qsem_wrapall(e->args->e);
    return qsem_expr(S, e->args->e, QY_VOID);
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

/* Call e's mut arguments each its only way to its var: no other argument the same var. A
   lambda's capture is lent mut when the lambda changes it. */
static void qsem_lent(QSem *const S, const QExpr *const e, const QFn *const f)
{
    for (uint32_t k = 0; k < f->npar; k++) {
        const QParam *const p = &f->params[k];
        const QExpr *const m = e->argv[k];
        const bool excl = p->hidden ? p->var->kind == QV_MUT : p->mut;
        const QVar *const v = excl && m->type != QY_ERROR ? qsem_root(m) : nullptr;

        for (uint32_t j = 0; v && j < f->npar; j++)
            if (j != k && qsem_root(e->argv[j]) == v)
                qsem_say(
                    S,
                    e->argv[j]->tok,
                    qsem_text(S, v->name),
                    p->hidden ? " is lent mut to the lambda in this call"
                              : " is lent mut in this call",
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
/* ---- generics */

enum { QSEM_INSTS = 64 }; /* a generic's instances at most: past it, one calls itself growing */

/* Instance f's type parameters bound to its types, in the scope open. */
static void qsem_typeargs(QSem *const S, const QFn *const f)
{
    for (uint32_t j = 0; f->of && j < f->ntp; j++)
        qsem_bindty(S, f->tparams[j], f->targs[j]);
}

/* Instance f's name as an error ends with it: " in f[int, Vec]"; none for a fn's instance for
   its lambdas, whose errors are its own. */
static const char *qsem_instname(QSem *const S, const QFn *const f)
{
    if (!f->ntp)
        return "";

    const char *s = qsem_join(S, " in ", qsem_text(S, f->name), "[");

    for (uint32_t j = 0; j < f->ntp; j++)
        s = qsem_join(S, s, j ? ", " : "", qsem_tyname(S, f->targs[j]));

    return qsem_join(S, s, "]", "");
}

/* Type as written x matched to type t: each type parameter of g it names found, into targs; false
   when one is found twice, two ways. */
static bool qsem_unify(
    QSem *const S,
    const QFn *const g,
    const QTypeEx *const x,
    const QType t,
    QType *const targs
)
{
    if (t == QY_ERROR)
        return true;

    if (x->kind == QX_NAME) {
        for (uint32_t j = 0; j < g->ntp; j++)
            if (qsem_same(g->tparams[j], x->tok)) {
                const bool fits = !targs[j] || targs[j] == t;
                targs[j] = targs[j] ? targs[j] : t;
                return fits;
            }

        return true;
    }

    const QTy *const y = qast_compound(t) ? qast_ty(S->prog, t) : nullptr;

    switch (x->kind) {
    case QX_LIST:
    case QX_ARRAY:
        return !y || (y->kind != QT_LIST && y->kind != QT_ARRAY) ||
               qsem_unify(S, g, x->elem, y->elem, targs);

    case QX_OPT:
    case QX_RES: /* a T for a T? too */
        return qsem_unify(S, g, x->elem, qsem_wraps(S, t, QT_REC) ? qsem_inner(S, t) : t, targs);

    case QX_TUPLE:
    case QX_FN: {
        const QTyKind kind = x->kind == QX_TUPLE ? QT_TUPLE : QT_FN;
        bool fits = true;

        for (uint32_t k = 0; y && y->kind == kind && k < x->n && k < y->nfields; k++)
            fits &= qsem_unify(S, g, x->items[k], y->fields[k].type, targs);

        if (y && y->kind == QT_FN && x->elem)
            fits &= qsem_unify(S, g, x->elem, y->elem, targs);

        return fits;
    }

    default:
        return true;
    }
}

/* Literal a given for parameter type x of generic g: each type parameter x names that is still
   open, or was typed by literals only, takes a's type (1 and 2.5 meet in double); false when one a
   typed value bound doesn't take a. */
static bool qsem_literal(
    QSem *const S,
    const QFn *const g,
    const QTypeEx *const x,
    const QExpr *const a,
    QType *const targs,
    bool *const bylit
)
{
    QType *const lit = qast_alloc(S->mem, g->ntp * sizeof *lit);
    qsem_unify(S, g, x, a->type, lit);

    for (uint32_t j = 0; j < g->ntp; j++) {
        if (lit[j] && targs[j] && !bylit[j] && !qsem_fits(a, targs[j]))
            return false;

        if (lit[j] && (!targs[j] || (bylit[j] && lit[j] == QY_DOUBLE))) {
            targs[j] = lit[j];
            bylit[j] = true;
        }
    }

    return true;
}

/* ---- lambdas */

static void qsem_stmt(QSem *S, QStmt *s);

/* Lambda l's block, as qsem_body checks a fn's: its result's variable, its statements. */
static void qsem_lamblock(QSem *const S, QLam *const l, const QType result)
{
    QFn *const f = l->fn;
    f->result = result;
    f->byarg = qsem_byplace(S, result);

    if (result != QY_VOID)
        f->res = qsem_var(S, l->tok, result, QV_RESULT);

    f->sig = true;
    const uint32_t marks = S->nmarks;

    for (QStmt *s = f->def->next; s && s != f->end; s = s->next)
        qsem_stmt(S, s);

    if (f->end)
        qsem_stmt(S, f->end);

    while (S->nmarks > marks)
        qsem_pop(S);
}

/* Lambda l's fn's parameters: its own, then each capture of a fn's storage, hidden, by
   reference. */
static void qsem_lamparams(QSem *const S, QLam *const l)
{
    QFn *const f = l->fn;
    uint32_t n = l->npar;

    for (uint32_t j = 0; j < l->ncaps; j++)
        n += l->caps[j]->frame;

    f->params = qast_alloc(S->mem, (n ? n : 1) * sizeof *f->params);
    memcpy(f->params, l->params, l->npar * sizeof *f->params);
    f->npar = l->npar;

    for (uint32_t j = 0; j < l->ncaps; j++)
        if (l->caps[j]->frame) {
            const QCap *const c = l->caps[j];
            f->params[f->npar++] = (QParam){
                .name = c->name,
                .mut = true,
                .hidden = true,
                .var = c->param,
            };

            f->nhidden++;
        }
}

/* Lambda e's site check for a place of fn type *t: its parameters typed as *t's (and as written,
   if, the same), its body checked here, each name it uses from outside a capture. With open, *t's
   result is the lambda's own, *t remade with it. False after an error. */
static bool qsem_lambda(QSem *const S, QExpr *const e, QType *const t, const bool open)
{
    QLam *const l = e->lam;
    const QTy *const y = qast_ty(S->prog, *t);
    const uint32_t n = y->nfields;
    const QField *const types = y->fields;
    const uint32_t errs = S->prog->errs.n;
    QType result = y->elem;
    e->type = QY_ERROR;

    if (l->npar != n) {
        qsem_say(
            S,
            l->tok,
            "the lambda takes ",
            n == 1 ? "one value" : qsem_join(S, qsem_uint(S, n), " values", ""),
            " here",
            nullptr
        );
        return false;
    }

    if (!l->fn) { /* a one-line's: its checks' variables, no code */
        l->fn = qast_alloc(S->mem, sizeof *l->fn);
        *l->fn = (QFn){.name = l->tok, .inl = true};
    }

    QLamAt c = {.lam = l, .mark = S->nbinds, .up = S->lamat};

    QFn *const fn = S->fn;
    S->lamat = &c;
    S->fn = l->fn;
    qsem_push(S);

    for (uint32_t k = 0; k < n; k++) {
        QParam *const p = &l->params[k];
        const QType w = p->type ? qsem_type(S, p->type) : types[k].type;

        if (w != types[k].type && w != QY_ERROR && types[k].type != QY_ERROR)
            qsem_say(
                S,
                p->name,
                qsem_text(S, p->name),
                " is ",
                qsem_aname(S, types[k].type),
                " here, not ",
                qsem_aname(S, w),
                nullptr
            );

        p->var = qsem_var(S, p->name, types[k].type, QV_PARAM);
        qsem_bind(S, p->name, p->var, nullptr);
    }

    if (l->rtype) {
        const QType r = qsem_type(S, l->rtype);

        if (!open && r != result && r != QY_ERROR)
            qsem_say(
                S,
                l->rtype->tok,
                "the lambda returns ",
                qsem_aname(S, result),
                " here",
                nullptr
            );

        result = r;
    } else if (open && !l->body)
        qsem_say(S, l->tok, "can't tell the lambda's result: write fn(..) as T", nullptr);

    if (!l->body)
        qsem_lamblock(S, l, result);
    else {
        QExpr *const v = qsem_expr(S, l->body, open ? QY_VOID : result);
        result = open ? qsem_settle(S, v)->type : result;
        l->body = result == QY_VOID ? v : qsem_coerce(S, v, result);
    }

    qsem_pop(S);
    S->lamat = c.up;
    S->fn = fn;
    qsem_lamparams(S, l);

    if (open) {
        QType *const items = qast_alloc(S->mem, (n ? n : 1) * sizeof *items);

        for (uint32_t k = 0; k < n; k++)
            items[k] = types[k].type;

        *t = result == QY_ERROR ? QY_ERROR : qsem_fnof(S, items, n, result);
    }

    l->type = e->type = *t;
    l->checked = S->prog->errs.n == errs && *t != QY_ERROR;
    return l->checked;
}

/* Capture c's value at a site that sees what it captured: its variable, or the place it
   names. */
static QExpr *qsem_capval(QSem *const S, const QCap *const c, const QTok *const tok)
{
    QVar *const v = c->var;

    if (v->place) {
        QExpr *const p = qast_alloc(S->mem, sizeof *p);
        *p = *v->place;
        p->tok = tok;
        return p;
    }

    QExpr *const e = qast_expr(S->mem, QE_NAME, tok);
    e->var = v;
    e->type = v->type;
    return e;
}

/* The parameter of the fn checked whose variable v is; null none. */
static const QParam *qsem_paramof(const QSem *const S, const QVar *const v)
{
    for (uint32_t k = 0; S->fn && k < S->fn->npar; k++)
        if (S->fn->params[k].var == v)
            return &S->fn->params[k];

    return nullptr;
}

/* Call e's hidden arguments from at, for fn parameter p bound to lambda l given as a: a literal's
   captures as this site sees them; one passed on, the fn checked's own hidden parameters for it. */
static void qsem_hiddenargs(
    QSem *const S,
    QExpr *const e,
    uint32_t at,
    const QLam *const l,
    const QExpr *const a
)
{
    const QParam *const from = a->kind == QE_NAME ? qsem_paramof(S, a->var) : nullptr;
    uint32_t n = 0;

    for (uint32_t j = 0; j < l->ncaps; j++) {
        if (!l->caps[j]->frame)
            continue;

        e->argv[at++] = from ? qsem_nameof(S, S->fn->params[from->capat + n].var, e->tok)
                             : qsem_capval(S, l->caps[j], e->tok);
        n++;
    }
}

/* Does type x name one of g's type parameters still open in targs? */
static bool qsem_open(const QFn *const g, const QTypeEx *const x, const QType *const targs)
{
    if (!x)
        return false;

    for (uint32_t j = 0; x->kind == QX_NAME && j < g->ntp; j++)
        if (!targs[j] && qsem_same(g->tparams[j], x->tok))
            return true;

    bool open = qsem_open(g, x->elem, targs);

    for (uint32_t k = 0; x->kind != QX_NAME && k < x->n; k++)
        open |= qsem_open(g, x->items[k], targs);

    return open;
}

/* Lambda e for g's parameter at: the fn type its site check takes, its parameters' read with
   targs bound; *open when its result names a type still open. QY_ERROR after an error. */
static QType qsem_lamtype(
    QSem *const S,
    const QFn *const g,
    const uint32_t at,
    const QType *const targs,
    bool *const open
)
{
    const QTypeEx *const x = g->params[at].type;
    *open = false;

    if (!x || x->kind != QX_FN) {
        qsem_say(S, g->params[at].name, "a lambda goes to a fn's parameter", nullptr);
        return QY_ERROR;
    }

    if (!g->ntp)
        return g->params[at].var->type;

    for (uint32_t k = 0; k < x->n; k++)
        if (qsem_open(g, x->items[k], targs)) {
            qsem_say(S, g->params[at].name, "can't tell the lambda's parameters' types", nullptr);
            return QY_ERROR;
        }

    *open = qsem_open(g, x->elem, targs);
    qsem_push(S);

    for (uint32_t j = 0; j < g->ntp; j++)
        if (targs[j])
            qsem_bindty(S, g->tparams[j], targs[j]);

    QType *const items = qast_alloc(S->mem, (x->n ? x->n : 1) * sizeof *items);
    QType t = QY_VOID;

    for (uint32_t k = 0; k < x->n && t != QY_ERROR; k++)
        t = items[k] = qsem_type(S, x->items[k]);

    const QType r = *open || !x->elem ? QY_VOID : qsem_type(S, x->elem);
    qsem_pop(S);
    return t == QY_ERROR || r == QY_ERROR ? QY_ERROR : qsem_fnof(S, items, x->n, r);
}

/* Named fn f as a lambda: called directly where it is bound. */
static QLam *qsem_aslam(QSem *const S, QFn *const f, const QType t)
{
    if (!f->aslam) {
        f->aslam = qast_alloc(S->mem, sizeof *f->aslam);
        *f->aslam = (QLam){
            .tok = f->name,
            .named = f,
            .type = t,
            .checked = true,
        };
    }

    return f->aslam;
}

/* What argument v binds its fn parameter to: a lambda, a named fn, a parameter bound to one passed
   on; null a fn value, passed as one. */
static QLam *qsem_bindof(QSem *const S, const QExpr *const v)
{
    if (v->kind == QE_LAMBDA)
        return v->lam;
    if (v->kind == QE_FNREF)
        return qsem_aslam(S, v->fn, v->type);

    return v->kind == QE_NAME && v->var ? v->var->lam : nullptr;
}

/* A chain's steps' and ends' names, by QI_* and QZ_*. */
static const char *const qsem_steps[] = {
    [QI_MAP] = "map",
    [QI_FILTER] = "filter",
    [QI_TAKE] = "take",
    [QI_SKIP] = "skip",
    [QI_ZIP] = "zip",
};
static const char *const qsem_ends[] = {
    [QZ_SUM] = "sum",
    [QZ_MIN] = "min",
    [QZ_MAX] = "max",
    [QZ_COUNT] = "count",
    [QZ_ANY] = "any",
    [QZ_ALL] = "all",
    [QZ_FIND] = "find",
    [QZ_FOLD] = "fold",
    [QZ_COLLECT] = "collect",
};
enum { QI_REV = QI_ZIP + 1 }; /* rev(): the source counted down */

static QVar *qsem_var(QSem *S, const QTok *name, QType type, QVarKind kind);

/* Instance i's parameters bound to their lambdas: each takes no value, and its lambda's captures
   of a fn's storage come last, hidden, by reference; changeable where the lambda changes them. A
   lambda not of its parameter's type, said at call e, leaves i unchecked. */
static void qsem_bound(QSem *const S, QFn *const i, const QExpr *const e)
{
    uint32_t n = i->npar;

    for (uint32_t k = 0; k < i->npar; k++)
        for (uint32_t j = 0; i->binds[k] && j < i->binds[k]->ncaps; j++)
            n += i->binds[k]->caps[j]->frame;

    QParam *const params = qast_alloc(S->mem, (n ? n : 1) * sizeof *params);
    memcpy(params, i->params, i->npar * sizeof *params);
    const uint32_t own = i->npar;
    i->params = params;

    for (uint32_t k = 0; k < own; k++) {
        QLam *const l = i->binds[k];
        QParam *const p = &params[k];

        if (!l)
            continue;

        if (p->var->type != l->type && p->var->type != QY_ERROR) {
            qsem_say(
                S,
                e->tok,
                qsem_text(S, l->tok),
                " is ",
                qsem_aname(S, l->type),
                ", not ",
                qsem_aname(S, p->var->type),
                nullptr
            );
            i->sig = false;
        }

        p->lam = p->var->lam = l;
        p->capat = i->npar;

        for (uint32_t j = 0; j < l->ncaps; j++) {
            const QCap *const c = l->caps[j];

            if (!c->frame)
                continue;

            QVar *const v = qsem_var(S, c->name, c->var->type, c->mut ? QV_MUT : QV_PARAM);
            v->fn = i;
            params[i->npar++] = (QParam){
                .name = c->name,
                .mut = true,
                .hidden = true,
                .var = v,
            };

            i->nhidden++;
        }
    }
}

/* Have instance i the types targs and the lambdas binds? */
static bool qsem_keyed_as(
    const QFn *const i,
    const QType *const targs,
    QLam *const *const binds,
    const uint32_t ntp,
    const uint32_t npar
)
{
    return !memcmp(i->targs, targs, ntp * sizeof *targs) &&
           !memcmp(i->binds, binds, npar * sizeof *binds);
}

/* Call e of g, a generic or a fn taking fns: its types read from the arguments, its lambdas
   checked at their site, its instance of both found or made, its signature checked; g itself when
   it needs none, null after an error. Each argument read is checked here, once: typed values
   first, then literals, which type only what is still open, as a place types them; then lambdas,
   whose results may type the rest. */
static QFn *qsem_instance(QSem *const S, QExpr *const e, QFn *const g)
{
    QStmt *const loop = g->ytx ? S->genfor : nullptr;
    const bool kept = g->ytx && S->genkept;
    S->genfor = nullptr;
    S->genkept = false;
    QType *const targs = qast_alloc(S->mem, (g->ntp ? g->ntp : 1) * sizeof *targs);
    bool *const bylit = qast_alloc(S->mem, (g->ntp ? g->ntp : 1) * sizeof *bylit);
    QLam **const binds = qast_alloc(S->mem, (g->npar ? g->npar : 1) * sizeof *binds);
    bool bound = false;

    for (uint32_t pass = 0; pass < 3; pass++) {
        uint32_t k = 0;

        for (QArg *a = e->args; a; a = a->next, k++) {
            uint32_t at = k;

            for (uint32_t i = 0; a->name && i < g->npar; i++)
                at = qsem_same(g->params[i].name, a->name) ? i : at;

            if (at >= g->npar || !g->params[at].type || (a->e->kind == QE_LAMBDA) != (pass == 2))
                continue;

            if (pass == 2) {
                bool open;
                QType t = qsem_lamtype(S, g, at, targs, &open);

                if (t == QY_ERROR || !qsem_lambda(S, a->e, &t, open))
                    return nullptr;
                if (open && !qsem_unify(S, g, g->params[at].type, t, targs))
                    return nullptr;

                a->e->done = true;
                binds[at] = a->e->lam;
                bound = true;
                continue;
            }

            if (!pass) {
                const QVar *const pv = g->params[at].var;
                S->passing = g->params[at].type->kind == QX_FN;
                a->e = qsem_expr(S, a->e, !g->ntp && pv ? pv->type : QY_VOID);
                S->passing = false;
                a->e->done = true;
                binds[at] = g->params[at].type->kind == QX_FN ? qsem_bindof(S, a->e) : nullptr;
                bound |= binds[at] != nullptr;

                if (binds[at] && pv && a->e->type != pv->type && a->e->type != QY_ERROR) {
                    qsem_mismatch(S, a->e, pv->type);
                    return nullptr;
                }
            }

            if (a->e->untyped != (pass == 1))
                continue;

            const bool fits = pass ? qsem_literal(S, g, g->params[at].type, a->e, targs, bylit)
                                   : qsem_unify(S, g, g->params[at].type, a->e->type, targs);

            if (!fits) {
                qsem_say(S, a->e->tok, qsem_text(S, g->name), "'s types don't agree here", nullptr);
                return nullptr;
            }
        }
    }

    for (uint32_t j = 0; j < g->ntp; j++)
        if (!targs[j]) {
            qsem_say(
                S,
                e->tok,
                "can't tell ",
                qsem_text(S, g->tparams[j]),
                " from the values given",
                nullptr
            );
            return nullptr;
        }

    if (!g->ntp && !bound && !g->ytx)
        return g;

    for (QFn *i = g->insts; i && !g->ytx; i = i->sibling) /* a generator's: one a site */
        if (qsem_keyed_as(i, targs, binds, g->ntp, g->npar))
            return i->sig ? i : nullptr;

    if (g->ninst == QSEM_INSTS) {
        qsem_say(
            S,
            e->tok,
            qsem_text(S, g->name),
            " has too many instances: does it call itself with ever new types or lambdas?",
            nullptr
        );
        return nullptr;
    }

    QFn *const i = qpar_again(S->prog, S->mem, g);
    i->of = g;
    i->targs = targs;
    i->binds = binds;
    i->sibling = g->insts;
    i->loop = loop;
    i->resumable = kept;
    i->host = S->fn;
    g->insts = i;
    g->ninst++;

    QFn *const fn = S->fn;
    qsem_push(S);
    qsem_typeargs(S, i);
    qsem_signature(S, i);
    S->fn = i;
    qsem_bound(S, i, e);
    qsem_pop(S);
    S->fn = fn;
    return i->sig ? i : nullptr;
}

/* Does f take a fn: a parameter of a fn type, as written? */
static bool qsem_takesfn(const QFn *const f)
{
    for (uint32_t k = 0; k < f->npar; k++)
        if (f->params[k].type && f->params[k].type->kind == QX_FN)
            return true;

    return false;
}

static void qsem_lent(QSem *S, const QExpr *e, const QFn *f);

static QExpr *qsem_fncall(QSem *const S, QExpr *const e, QFn *f)
{
    if (f->ytx && !S->genfor && !S->genkept) {
        qsem_say(
            S,
            e->tok,
            qsem_text(S, f->name),
            " is a generator: loop over it with for, or keep it in a var",
            nullptr
        );
        e->type = QY_ERROR;
        return e;
    }

    if ((qast_generic(f) || (!f->of && qsem_takesfn(f))) && !(f = qsem_instance(S, e, f))) {
        e->type = QY_ERROR;
        return e;
    }

    const uint32_t own = f->npar - f->nhidden;
    e->fn = f;
    e->type = f->result;
    e->nargv = f->npar;
    e->argv = qast_alloc(S->mem, (f->npar ? f->npar : 1) * sizeof *e->argv);
    const QTok **const names = qast_alloc(S->mem, (f->npar ? f->npar : 1) * sizeof *names);

    for (uint32_t k = 0; k < own; k++)
        names[k] = f->params[k].name;

    const QArg **const given = qsem_given(S, e->args, names, own, f->name, " has no parameter ");

    for (uint32_t k = 0; k < own; k++) {
        const QParam *const p = &f->params[k];
        const QArg *const a = given[k];

        if (p->lam && a) { /* checked by qsem_instance: no value, its captures after */
            e->argv[k] = a->e;
            qsem_hiddenargs(S, e, p->capat, p->lam, a->e);
            continue;
        }

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
static QVar *qsem_rawroot(QSem *const S, const QExpr *e)
{
    while (e->kind == QE_FIELD || e->kind == QE_INDEX)
        e = e->a;

    const QBind *const b = qsem_lookup(S, e);
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

    const QVar *orig = v;

    for (const QVar *x = v; x && x->cap; x = x->cap->var) { /* a capture: what it stands for */
        x->cap->mut = true;
        orig = x->cap->var;
    }

    const QLoan *const l = qsem_loan(S, orig);

    if (l && !l->loop->mut)
        qsem_say(
            S,
            at,
            name,
            l->loop->kind == QS_CALL ? " is borrowed by the chain over it"
                                     : " is borrowed by the for over it",
            ": it can't change here",
            nullptr
        );

    return !l; /* a for mut's: qsem_name says so */
}

static uint32_t qsem_at(const QSem *const S, const QTok *const t)
{
    const QMod *const m = &S->prog->mods[qsem_modof(S, t)];
    return m->base + (uint32_t)(t - m->toks);
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
    return qast_compound(t) && !qast_cell(S->prog, t);
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

/* Does e read variable v, but for its fields from field on, or call a fn (v a module's: the fn
   may see it)? */
static bool qsem_reads(const QExpr *const e, const QVar *const v, const uint32_t field)
{
    if (!e)
        return false;
    if (e->kind == QE_FIELD && e->a->kind == QE_NAME && e->a->var == v)
        return e->field < field;
    if (qsem_names(e, v) || (!v->fn && qsem_calls(e, v)))
        return true;
    if (qsem_reads(e->a, v, field) || qsem_reads(e->b, v, field) || qsem_reads(e->c, v, field))
        return true;

    for (uint32_t k = 0; k < e->nargv; k++)
        if (qsem_reads(e->argv[k], v, field))
            return true;

    for (uint32_t k = 0; e->kind != QE_NEW && k < e->n; k++)
        if (qsem_reads(e->items[k], v, field))
            return true;

    return false;
}

/* Can record value e be built field by field straight into variable v though it reads v: does
   each field's value read none of v's fields written before it? An inline operator's arguments
   are read before. */
static bool qsem_fieldwise(QSem *const S, const QVar *const v, const QExpr *const e)
{
    const QExpr *const r = e->kind == QE_APPLY ? e->a : e;

    if (r->kind != QE_NEW || !qsem_isrec(S, r->type) || r->var)
        return false;

    const QTy *const y = qast_ty(S->prog, r->type);

    for (uint32_t k = 0; k < r->nargv; k++) {
        const bool deep = qsem_byplace(S, y->fields[k].type);

        if ((!r->argv[k] && deep) || qsem_reads(r->argv[k], v, k + deep))
            return false;
    }

    return true;
}

/* A record's or list's value e for place t: qsem_into's, but built field by field into a
   variable whose fields it reads only before writing them. */
static QExpr *qsem_intoplace(QSem *const S, const QExpr *const t, QExpr *const e)
{
    if (t->kind == QE_NAME && t->var && qsem_fieldwise(S, t->var, e))
        return e;

    return qsem_into(S, qsem_root(t), e);
}

/* A record or list that is no place, where its address is wanted: built in a temporary. */
static QExpr *qsem_addressed(QSem *const S, QExpr *const e)
{
    if (qsem_byplace(S, e->type) && qast_fresh(e) && !e->var)
        e->var = qsem_var(S, e->tok, e->type, QV_TEMP);

    return e;
}

/* A T? or T! of type t built: its flag and its value, null the rest's zero. */
static QExpr *qsem_wrapped(
    QSem *const S,
    const QTok *const tok,
    const QType t,
    const bool flag,
    QExpr *const val
)
{
    const QTy *const y = qast_ty(S->prog, t);
    QExpr *const e = qast_expr(S->mem, QE_NEW, tok);
    e->type = t;
    e->nargv = y->nfields;
    e->argv = qast_alloc(S->mem, y->nfields * sizeof *e->argv);
    e->argv[QF_FLAG] = qsem_bool(S, tok, flag);
    e->argv[QF_VAL] = val;
    return e;
}

static QExpr *qsem_owned(QSem *S, QExpr *e, bool returned);

/* v, a T, as a T? or a T! of type to: has it, or no error. */
static QExpr *qsem_wrap(QSem *const S, const QType to, QExpr *const v)
{
    const bool opt = qsem_wraps(S, to, QT_OPT);
    return qsem_wrapped(S, v->tok, to, opt, qsem_owned(S, v, false));
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
    if (qsem_ismod(S, e->a)) { /* m.x: x of module m, a name */
        e->kind = QE_NAME;
        return qsem_name(S, e);
    }

    if (e->a->kind == QE_NAME || e->a->kind == QE_FIELD) {
        const QBind *const b = qsem_qual(S, e->a);

        if (b && b->ty == QY_ERROR) {
            e->type = QY_ERROR;
            return e;
        }

        if (b && b->ty)
            return qsem_variant(S, e, b->ty, false);
    }

    const QExpr *const a = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    e->type = QY_ERROR;

    if (a->type == QY_ERROR)
        return e;

    static const char *const errs[] =
        {[QF_CODE] = "code", [QF_MESSAGE] = "message", [QF_LINE] = "line"};

    for (uint32_t k = QF_CODE; qsem_wraps(S, a->type, QT_RES) && k <= QF_LINE; k++)
        if (qsem_is(e->tok, errs[k])) {
            e->field = k;
            e->type = qast_ty(S->prog, a->type)->fields[k].type;
            return e;
        }

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
        e->type = e->b->type == QY_ERROR ? QY_ERROR : QY_UBYTE;
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
/* a.f(..)'s f, not in sight: the public fn f of the module type t is declared in; null none. */
static const QBind *qsem_typefn(QSem *const S, const QType t, const QTok *const name)
{
    const QTy *const y = qast_compound(t) ? qast_ty(S->prog, t) : nullptr;

    if (!y || !y->def)
        return nullptr;

    const QScope *const sc = &S->scopes[qsem_modof(S, y->def->tok)];

    for (uint32_t i = 0; i < sc->n; i++)
        if (qsem_same(sc->at[i].name, name) && sc->at[i].fn && sc->at[i].pub)
            return &sc->at[i];

    return nullptr;
}

static QExpr *qsem_method(QSem *const S, QExpr *const e)
{
    static const struct {
        const char *name;
        QBuiltin op;
    } lists[] = {{"push", QB_PUSH}, {"pop", QB_POP}, {"insert", QB_INSERT}, {"remove", QB_REMOVE}};

    if (qsem_ismod(S, e->a)) { /* m.f(..): f of module m */
        QExpr name = {.kind = QE_NAME, .tok = e->tok, .a = e->a};

        return qsem_called(S, e, qsem_qual(S, &name));
    }

    if (e->a->kind == QE_NAME || e->a->kind == QE_FIELD) {
        const QBind *const b = qsem_qual(S, e->a);

        if (b && b->ty == QY_ERROR) {
            e->type = QY_ERROR;
            return e;
        }

        if (b && b->ty)
            return qsem_variant(S, e, b->ty, true);
        if (b && b->var && qsem_isgen(S, b->var->type) && qsem_is(e->tok, "next"))
            return qsem_next(S, e, b->var);
    }

    uint32_t k;

    if (qsem_method_of(S, e, qsem_ends, QZ_FOR, &k))
        return qsem_iter(S, e, k, nullptr);

    if (qsem_step(S, e, &k)) {
        qsem_say(
            S,
            e->tok,
            "a chain is used up where it is made: by collect(), a reducing method or a for",
            nullptr
        );
        e->type = QY_ERROR;
        return e;
    }

    if (e->a->kind == QE_NAME) {
        const QBind *const b = qsem_find(S, e->a->tok);

        if (b && b->var && qsem_isgen(S, b->var->type))
            return qsem_next(S, e, b->var);
    }

    const QBind *b = qsem_find(S, e->tok);

    if (!b || !b->fn) {
        for (uint32_t i = 0; i < countof(lists); i++)
            if (qsem_is(e->tok, lists[i].name))
                return qsem_listop(S, e, lists[i].op);

        if (qsem_is(e->tok, "len"))
            return qsem_len(S, e, e->a);
        if (qsem_is(e->tok, "keys"))
            return qsem_keys(S, e);
        if (qsem_is(e->tok, "get"))
            return qsem_get(S, e);

        QExpr *const r = qsem_expr(S, e->a, QY_VOID);
        const QTy *const y = qsem_isrec(S, r->type) ? qast_ty(S->prog, r->type) : nullptr;

        for (uint32_t k = 0; y && k < y->nfields; k++) /* a record's fn value, called */
            if (qsem_same(y->fields[k].name, e->tok) && qast_isfn(S->prog, y->fields[k].type)) {
                QExpr *const f = qast_expr(S->mem, QE_FIELD, e->tok);
                f->a = qsem_addressed(S, r);
                f->field = k;
                f->type = y->fields[k].type;
                return qsem_icall(S, e, f);
            }

        b = qsem_typefn(S, r->type, e->tok);

        if (!b) {
            if (r->type != QY_ERROR)
                qsem_say(S, e->tok, "unknown fn ", qsem_text(S, e->tok), nullptr);

            e->type = QY_ERROR;
            return e;
        }

        r->done = true;
        e->a = r;
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

/* ---- calls of fn values and lambdas */

/* A call's n values, plain, each for a parameter of type types[k]: checked, by position. */
static QExpr **qsem_plainargs(
    QSem *const S,
    QExpr *const e,
    const QField *const types,
    const uint32_t n
)
{
    QExpr **const argv = qast_alloc(S->mem, (n ? n : 1) * sizeof *argv);
    uint32_t k = 0;

    for (const QArg *a = e->args; a; a = a->next, k++) {
        if (a->name || a->mut) {
            qsem_say(S, a->name ? a->name : a->mut, "a fn value takes plain values", nullptr);
            return nullptr;
        }

        if (k < n)
            argv[k] = qsem_addressed(S, qsem_value(S, a->e, types[k].type));
    }

    if (k != n) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " takes ", qsem_uint(S, n), " values", nullptr);
        return nullptr;
    }

    return argv;
}

/* Can argument v stand for a lambda's parameter where it is read, unstored: a constant, a
   variable, an element by a variable or a constant, their fields? */
static bool qsem_cheap(const QExpr *const v)
{
    if (v->kind == QE_CONST || v->kind == QE_NAME)
        return v->kind == QE_CONST || v->var;
    if (v->kind == QE_FIELD)
        return qsem_cheap(v->a);

    return v->kind == QE_INDEX && !v->insert && (v->b->kind == QE_CONST || v->b->kind == QE_NAME) &&
           qsem_cheap(v->a);
}

/* f(x), f the fn checked's parameter p, bound to one-line lambda l: l's body checked again here,
   inlined. It sees only its parameters, its captures (the fn's hidden parameters for them) and
   the module's names: x where it is read, when cheap, else stored first. */
static QExpr *qsem_apply(QSem *const S, QExpr *const e, const QLam *const l, const QParam *const p)
{
    const QTy *const y = qast_ty(S->prog, l->type);
    const QType result = y->elem;
    QExpr **const argv = qsem_plainargs(S, e, y->fields, l->npar);
    e->type = QY_ERROR;

    if (!argv)
        return e;

    const uint32_t lo = S->hidelo, hi = S->hidehi;
    QLamAt *const at = S->lamat;
    S->hidelo = qsem_floor(S);
    S->hidehi = S->nbinds;
    S->lamat = nullptr;
    qsem_push(S);
    uint32_t n = 0;

    for (uint32_t j = 0; j < l->ncaps; j++) {
        const QCap *const c = l->caps[j];

        if (c->frame)
            qsem_bind(S, c->name, S->fn->params[p->capat + n++].var, nullptr);
        else {
            S->binds = qast_room(S->mem, S->binds, S->nbinds, &S->capbinds, sizeof *S->binds);
            S->binds[S->nbinds++] = (QBind){
                .name = c->name,
                .var = c->var,
                .fn = c->fn,
                .ty = c->ty,
            };
        }
    }

    e->temps = qast_alloc(S->mem, (l->npar ? l->npar : 1) * sizeof *e->temps);

    for (uint32_t k = 0; k < l->npar; k++) {
        QVar *const v = qsem_var(S, l->params[k].name, argv[k]->type, QV_PARAM);

        if (qsem_cheap(argv[k]))
            v->place = argv[k];
        else
            e->temps[k] = v;

        qsem_bind(S, l->params[k].name, v, nullptr);
    }

    QExpr *const body = qpar_reexpr(S->prog, S->mem, l->at);
    QExpr *const v = body ? qsem_expr(S, body, result) : nullptr;
    qsem_pop(S);
    S->hidelo = lo;
    S->hidehi = hi;
    S->lamat = at;

    if (!v)
        return e;

    e->kind = QE_APPLY;
    e->lam = (QLam *)l;
    e->a = result == QY_VOID ? v : qsem_coerce(S, v, result);
    e->argv = argv;
    e->nargv = l->npar;
    e->type = e->a->type == QY_ERROR ? QY_ERROR : result;
    return e;
}

/* Operator fn f's call e of argv, inlined: f's one return's value checked again here, each
   parameter read where its argument is when cheap, else stored first. Its own module's names in
   sight, not the caller's. */
static QExpr *qsem_inline(QSem *const S, QExpr *const e, QFn *const f, QExpr **const argv)
{
    const uint32_t lo = S->hidelo, hi = S->hidehi;
    QLamAt *const at = S->lamat;
    S->hidelo = qsem_floor(S);
    S->hidehi = S->nbinds;
    S->lamat = nullptr;
    qsem_push(S);
    e->temps = qast_alloc(S->mem, f->npar * sizeof *e->temps);

    for (uint32_t k = 0; k < f->npar; k++) {
        QVar *const v = qsem_var(S, f->params[k].name, argv[k]->type, QV_PARAM);

        if (qsem_cheap(argv[k]))
            v->place = argv[k];
        else
            e->temps[k] = v;

        qsem_bind(S, f->params[k].name, v, nullptr);
    }

    f->inlining = true;
    QExpr *const body = qpar_reexpr(S->prog, S->mem, f->lone);
    QExpr *const v = body ? qsem_expr(S, body, f->result) : nullptr;
    f->inlining = false;
    qsem_pop(S);
    S->hidelo = lo;
    S->hidehi = hi;
    S->lamat = at;
    e->type = QY_ERROR;

    if (!v)
        return e;

    e->kind = QE_APPLY;
    e->a = qsem_coerce(S, v, f->result);
    e->b = e->c = nullptr;
    e->fn = f;
    e->argv = argv;
    e->nargv = f->npar;
    e->type = e->a->type == QY_ERROR ? QY_ERROR : f->result;
    return e;
}

/* Record type t's fn operator op, null none. */
static QFn *qsem_operof(const QSem *const S, const QType t, const QKind op)
{
    for (QFn *f = qsem_isrec(S, t) ? qast_ty(S->prog, t)->opers : nullptr; f; f = f->opnext)
        if (f->op == op)
            return f;

    return nullptr;
}

/* a op b, a a record: its type's fn operator op, inlined when its body is one return, else
   called. */
static QExpr *qsem_operator(QSem *const S, QExpr *const e, const QKind op, QExpr *a, QExpr *b)
{
    QFn *const f = qsem_operof(S, a->type, op);
    e->type = QY_ERROR;

    if (!f) {
        qsem_say(
            S,
            e->tok,
            qsem_aname(S, a->type),
            " has no operator ",
            qast_spelling(op),
            nullptr
        );
        return e;
    }

    if (!f->pub && qsem_modof(S, f->name) != qsem_modof(S, e->tok)) {
        const char *const ty = qsem_text(S, qast_ty(S->prog, a->type)->name);
        qsem_say(S, e->tok, ty, "'s operator ", qast_spelling(op), " is not public", nullptr);
        return e;
    }

    QExpr **const argv = qast_alloc(S->mem, 2 * sizeof *argv);
    argv[0] = qsem_coerce(S, a, f->params[0].var->type);
    argv[1] = qsem_coerce(S, b, f->params[1].var->type);

    if (argv[0]->type == QY_ERROR || argv[1]->type == QY_ERROR)
        return e;
    if (f->lone && !f->inlining) /* a fresh argument built in its parameter's temporary */
        return qsem_inline(S, e, f, argv);

    argv[0] = qsem_addressed(S, argv[0]);
    argv[1] = qsem_addressed(S, argv[1]);

    e->kind = QE_CALL;
    e->fn = f;
    e->argv = argv;
    e->nargv = 2;
    e->a = e->b = nullptr;
    e->done = true;
    e->type = f->result;
    return e;
}

/* Does a op b compare records by a fn operator? */
static bool qsem_hasrel(
    const QSem *const S,
    const QKind op,
    const QExpr *const a,
    const QExpr *const b
)
{
    const bool swap = op == QK_GT || op == QK_LE;
    return qsem_operof(S, (swap ? b : a)->type, op == QK_EQ || op == QK_NE ? QK_EQ : QK_LT);
}

/* a op b, a a record: = and < its operators; <> and >= not = and <; > and <= < the other way. */
static QExpr *qsem_relation(QSem *const S, QExpr *const e, const QKind op, QExpr *a, QExpr *b)
{
    const bool swap = op == QK_GT || op == QK_LE;
    const bool neg = op == QK_NE || op == QK_LE || op == QK_GE;
    QExpr *const x = qsem_operator(
        S,
        qast_expr(S->mem, QE_BINARY, e->tok),
        op == QK_EQ || op == QK_NE ? QK_EQ : QK_LT,
        swap ? b : a,
        swap ? a : b
    );

    if (!neg || x->type == QY_ERROR)
        return x;

    QExpr *const n = qast_expr(S->mem, QE_UNARY, e->tok);
    n->op = QK_NOT;
    n->a = x;
    n->type = QY_BOOL;
    return n;
}

/* f(x), f the fn checked's parameter p, bound to block lambda l: a call of its procedure, its
   captures the fn's hidden parameters for them. */
static QExpr *qsem_blockcall(
    QSem *const S,
    QExpr *const e,
    const QLam *const l,
    const QParam *const p
)
{
    QFn *const f = l->fn;
    const QTy *const y = qast_ty(S->prog, l->type);
    QExpr **const argv = qsem_plainargs(S, e, y->fields, l->npar);
    e->type = QY_ERROR;

    if (!argv)
        return e;

    e->fn = f;
    e->argv = qast_alloc(S->mem, (f->npar ? f->npar : 1) * sizeof *e->argv);
    e->nargv = f->npar;
    memcpy(e->argv, argv, l->npar * sizeof *argv);

    for (uint32_t k = l->npar; k < f->npar; k++)
        e->argv[k] = qsem_nameof(S, S->fn->params[p->capat + k - l->npar].var, e->tok);

    e->type = f->result;
    return e;
}

/* Call e through fn value f, checked. */
static QExpr *qsem_icall(QSem *const S, QExpr *const e, QExpr *const f)
{
    e->type = QY_ERROR;

    if (f->type == QY_ERROR)
        return e;

    const QTy *const y = qast_ty(S->prog, f->type);
    const QType result = y->elem;
    const uint32_t n = y->nfields;
    QExpr **const argv = qsem_plainargs(S, e, y->fields, n);

    if (!argv)
        return e;

    e->kind = QE_CALL;
    e->builtin = QB_ICALL;
    e->a = f;
    e->argv = argv;
    e->nargv = n;
    e->type = result;
    return e;
}

/* f(x), f a variable of a fn type: a lambda bound to the fn checked's parameter, inlined or
   called; else a call through its value. */
static QExpr *qsem_fnvar(QSem *const S, QExpr *const e, QVar *const v)
{
    if (v->lam) {
        const QParam *const p = qsem_paramof(S, v);

        if (v->lam->named)
            return qsem_fncall(S, e, v->lam->named);

        return v->lam->body ? qsem_apply(S, e, v->lam, p) : qsem_blockcall(S, e, v->lam, p);
    }

    return qsem_icall(S, e, qsem_expr(S, qast_expr(S->mem, QE_NAME, e->tok), QY_VOID));
}

/* Fn f named as a value: its procedure's number, typed by its signature. */
static QExpr *qsem_fnref(QSem *const S, QExpr *const e, QFn *const f)
{
    e->type = QY_ERROR;

    if (f->ytx) {
        qsem_say(S, e->tok, qsem_text(S, f->name), " is no value: it is a generator", nullptr);
        return e;
    }

    if (qast_generic(f) || qsem_takesfn(f)) {
        qsem_say(S, e->tok, qsem_text(S, f->name), " is no value: it takes types or fns", nullptr);
        return e;
    }

    QType *const items = qast_alloc(S->mem, (f->npar ? f->npar : 1) * sizeof *items);

    for (uint32_t k = 0; k < f->npar; k++) {
        if (f->params[k].mut) {
            qsem_say(S, e->tok, qsem_text(S, f->name), " is no value: it takes mut", nullptr);
            return e;
        }

        items[k] = f->params[k].var->type;
    }

    e->kind = QE_FNREF;
    e->fn = f;
    e->type = qsem_fnof(S, items, f->npar, f->result);
    return e;
}

static QExpr *qsem_call(QSem *const S, QExpr *const e)
{
    if (e->a)
        return qsem_method(S, e);

    return qsem_called(S, e, qsem_find(S, e->tok));
}

/* Call e of what its name binds, b: a fn, a fn value, a record's type; none: a built-in. */
static QExpr *qsem_called(QSem *const S, QExpr *const e, const QBind *const b)
{
    if (b && b->ty == QY_ERROR) {
        e->type = QY_ERROR;
        return e;
    }

    if (b && b->fn)
        return qsem_fncall(S, e, b->fn);
    if (b && b->var && qast_isfn(S->prog, b->var->type))
        return qsem_fnvar(S, e, b->var);

    if (b && b->ty && qsem_isrec(S, b->ty)) {
        const QTy *const y = qast_ty(S->prog, b->ty);
        return qsem_build(S, e, b->ty, y->fields, y->nfields, y->name, false);
    }

    if (b && b->ty) {
        qsem_say(S, e->tok, "an enum's value is Name.value", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (b || e->a) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is not a fn", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (qsem_is(e->tok, "print"))
        return qsem_print(S, e);

    const QType to = qsem_scalar(e->tok);

    if (to != QY_VOID && to != QY_BOOL)
        return qsem_convert(S, e, to);

    if (qsem_is(e->tok, "len"))
        return qsem_len(S, e, nullptr);
    if (qsem_is(e->tok, "copy"))
        return qsem_copy(S, e);
    if (qsem_is(e->tok, "wrap"))
        return qsem_wrapped_ops(S, e);

    qsem_say(S, e->tok, "unknown fn ", qsem_text(S, e->tok), nullptr);
    e->type = QY_ERROR;
    return e;
}

static QExpr *qsem_name(QSem *const S, QExpr *const e)
{
    const QBind *const b = qsem_lookup(S, e);
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

    if (b->ty == QY_ERROR)
        return e;
    if (b->fn)
        return qsem_fnref(S, e, b->fn);

    if (b->scope) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is a module: name one of its own", nullptr);
        return e;
    }

    if (b->ty) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is a type", nullptr);
        return e;
    }

    QVar *const v = b->var;
    QVar *const named = v->place ? qsem_root(v->place) : v; /* a constant yielded: none */
    QVar *const root = named ? named : v;
    const uint32_t at = qsem_at(S, e->tok);
    root->last = at > root->last ? at : root->last;
    root->shared |= S->fn && !root->fn && root->kind != QV_CONST;
    root->shared |= !root->fn && qsem_modof(S, root->name) != qsem_modof(S, e->tok);

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

    if (qsem_isgen(S, v->type)) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is a generator: call its next()", nullptr);
        return e;
    }

    if (v->lam && !S->passing) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is a lambda: call it or pass it on", nullptr);
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

/* ---- optionals and errors */

/* Field k of T? or T! place a, read where it is: a fresh one is settled by its first read. */
static QExpr *qsem_part(QSem *const S, QExpr *const a, const uint32_t k)
{
    QExpr *const f = qast_expr(S->mem, QE_FIELD, a->tok);
    f->a = a;
    f->field = k;
    f->type = qast_ty(S->prog, a->type)->fields[k].type;
    return f;
}

/* a's place for its later reads: a fresh one's temporary once settled. */
static QExpr *qsem_again(QSem *const S, QExpr *const a)
{
    return qast_fresh(a) && a->var ? qsem_nameof(S, a->var, a->tok) : a;
}

/* Error(code, message) for a T! place hint: its line kept. */
static QExpr *qsem_error(QSem *const S, QExpr *const e, const QType hint)
{
    e->type = QY_ERROR;

    if (!qsem_wraps(S, hint, QT_RES)) {
        qsem_say(S, e->tok, "Error(code, message) needs a T! place, as a fn's result", nullptr);
        return e;
    }

    if (!qsem_plain(S, e, 2)) {
        qsem_say(S, e->tok, "Error takes a code and a message", nullptr);
        return e;
    }

    QExpr *const w = qsem_wrapped(S, e->tok, hint, true, nullptr);
    w->argv[QF_CODE] = qsem_coerce(S, qsem_expr(S, e->args->e, QY_INT), QY_INT);
    w->argv[QF_MESSAGE] = qsem_coerce(S, qsem_expr(S, e->args->next->e, QY_STRING), QY_STRING);
    w->argv[QF_LINE] =
        qsem_const(S, e->tok, QY_INT, (QVal){.i = S->prog->src.lines[e->tok->line].src});

    return w->argv[QF_CODE]->type == QY_ERROR || w->argv[QF_MESSAGE]->type == QY_ERROR ? e : w;
}

/* a is [not] none, a T?; a is [not] Error, a T!. */
static QExpr *qsem_isnone(QSem *const S, QExpr *const e)
{
    QExpr *const a = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    const bool none = e->op == QK_NONE;
    e->type = QY_ERROR;

    if (a->type == QY_ERROR)
        return e;

    if (!qsem_wraps(S, a->type, none ? QT_OPT : QT_RES)) {
        const char *const say = none ? "is none takes a T?, not " : "is Error takes a T!, not ";
        qsem_say(S, e->tok, say, qsem_aname(S, a->type), nullptr);
        return e;
    }

    e->type = QY_BOOL;
    return e;
}

/* a ?? b: a's T, or b when a is none or an Error. */
static QExpr *qsem_coalesce(QSem *const S, QExpr *const e)
{
    QExpr *const a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    e->type = QY_ERROR;

    if (a->type == QY_ERROR) {
        qsem_expr(S, e->b, QY_VOID);
        return e;
    }

    if (!qsem_wraps(S, a->type, QT_REC)) {
        qsem_say(S, e->tok, "?? takes a T? or a T!, not ", qsem_aname(S, a->type), nullptr);
        return e;
    }

    const QType t = qsem_inner(S, a->type);
    QExpr *c = qsem_part(S, a, QF_FLAG); /* read first: settles a fresh a */

    if (qsem_wraps(S, a->type, QT_RES)) {
        QExpr *const n = qast_expr(S->mem, QE_UNARY, e->tok);
        n->op = QK_NOT;
        n->a = c;
        n->type = QY_BOOL;
        c = n;
    }

    e->kind = QE_COND;
    e->c = c;
    e->a = qsem_part(S, qsem_again(S, a), QF_VAL);
    e->b = qsem_value(S, e->b, t);
    e->type = e->b->type == QY_ERROR ? QY_ERROR : t;
    return e;
}

/* x?: x's T, or its error returned from the fn, whose result is a T! too. */
static QExpr *qsem_try(QSem *const S, QExpr *const e)
{
    QExpr *const a = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    e->type = QY_ERROR;

    if (a->type == QY_ERROR)
        return e;

    if (!qsem_wraps(S, a->type, QT_RES)) {
        qsem_say(S, e->tok, "x? takes a T!, not ", qsem_aname(S, a->type), nullptr);
        return e;
    }

    if (!S->fn || !qsem_wraps(S, S->fn->result, QT_RES)) {
        qsem_say(S, e->tok, "x? returns its error: only in a fn whose result is a T!", nullptr);
        return e;
    }

    e->fn = S->fn;
    e->type = qsem_inner(S, a->type);
    return e;
}

/* m.get(k): its value as a V?, none when k is not there; V no record or list. */
static QExpr *qsem_get(QSem *const S, QExpr *const e)
{
    QExpr *const m = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    e->type = QY_ERROR;

    if (m->type == QY_ERROR)
        return e;

    if (!qsem_ismap(S, m->type) || !qsem_plain(S, e, 1)) {
        const bool map = qsem_ismap(S, m->type);
        qsem_say(S, e->tok, map ? "get() takes a key" : "only a map has get()", nullptr);
        return e;
    }

    const QTy *const y = qast_ty(S->prog, m->type);

    if (qsem_byplace(S, y->elem)) {
        qsem_say(S, e->tok, "get() takes a map of numbers, strings and bools: use m[k]", nullptr);
        return e;
    }

    e->builtin = QB_GET;
    e->b = qsem_coerce(S, qsem_expr(S, e->args->e, y->key), y->key);
    e->temps = qast_alloc(S->mem, sizeof *e->temps);
    e->temps[0] = qsem_var(S, e->tok, QY_INT, QV_TEMP);
    e->type = e->b->type == QY_ERROR ? QY_ERROR : qsem_wrapof(S, QT_OPT, y->elem);
    return e;
}

/* e checked: its type set, constants folded; hint the type its place wants, QY_VOID none. */
static QExpr *qsem_expr(QSem *const S, QExpr *const e, const QType hint)
{
    if (!e)
        return qsem_const(S, &S->prog->src.toks[0], QY_ERROR, (QVal){});

    if (e->done)
        return e;

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
        return e->op == QK_QQ ? qsem_coalesce(S, e) : qsem_binary(S, e);

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

        qsem_say(S, e->tok, "none needs a T? place: write var x as T? = none", nullptr);
        e->type = QY_ERROR;
        return e;

    case QE_IS:
        return qsem_isnone(S, e);

    case QE_TRY:
        return qsem_try(S, e);

    case QE_LAMBDA:
        qsem_say(S, e->tok, "a lambda is passed to a fn's parameter", nullptr);
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

/* ---- statements */

/* One declaration of let, var or const. */
static void qsem_decl(QSem *const S, QDecl *const d, const QKind kw)
{
    QFn *const g = d->value && kw != QK_CONST ? qsem_genof(S, d->value) : nullptr;

    if (g) {
        qsem_kept(S, d, kw, g);
        return;
    }

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
    const QBind *const b = qsem_lookup(S, e);

    if (b && b->fn) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is a fn: call it", nullptr);
        e->type = QY_ERROR;
        return e;
    }

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
    if (v->place) { /* an inline generator's capture: its site's place */
        QExpr *const p = qast_alloc(S->mem, sizeof *p);
        *p = *v->place;
        p->tok = tok;
        return p;
    }

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
        s->e = qsem_intoplace(S, t, qsem_value(S, s->e, t->type));
        return;
    }

    if (qsem_byplace(S, t->type) && !qsem_isrec(S, t->type)) {
        qsem_say(S, s->tok, qast_spelling(s->op), " takes numbers or a record's operator", nullptr);
        return;
    }

    qsem_hoist(S, s, t);

    static const QKind ops[] = {[QK_ADDEQ - QK_ADDEQ] = QK_PLUS, QK_MINUS, QK_STAR, QK_SLASH};

    QExpr *const x = qast_expr(S->mem, QE_BINARY, s->tok);
    x->op = ops[s->op - QK_ADDEQ];
    x->a = t;
    x->b = s->e;
    s->e = qsem_coerce(S, qsem_expr(S, x, t->type), t->type);

    if (qsem_byplace(S, t->type))
        s->e = qsem_intoplace(S, t, s->e);
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

    if (a->type == QY_STRING) { /* its bytes, a chain's */
        a->done = true;
        qsem_chainfor(S, s);
        return;
    }

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

static void qsem_genfor(QSem *S, QStmt *s, QFn *g);

/* Is unchecked e a kept generator's name? */
static bool qsem_kepton(QSem *const S, const QExpr *const e)
{
    const QBind *const b = qsem_lookup(S, e);
    return b && b->var && qsem_isgen(S, b->var->type);
}

static void qsem_for(QSem *const S, QStmt *const s)
{
    qsem_loop(S, s);

    QFn *const g = s->each ? qsem_genof(S, s->e) : nullptr;
    uint32_t k;

    if (g) {
        qsem_genfor(S, s, g);
        return;
    }

    if (s->each && (qsem_step(S, s->e, &k) || s->e->kind == QE_RANGE || qsem_kepton(S, s->e))) {
        qsem_chainfor(S, s);
        return;
    }

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

    if (f->ytx) {
        if (s->e)
            qsem_say(S, s->e->tok, "a generator's return takes no value: yield it", nullptr);

        s->e = nullptr;
        return;
    }

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

/* ---- chains */

/* Is e, unchecked, a.name(..) for one of n names, and no fn of that name: *k which. */
static bool qsem_method_of(
    QSem *const S,
    const QExpr *const e,
    const char *const *const names,
    const uint32_t n,
    uint32_t *const k
)
{
    if (e->kind != QE_CALL || !e->a || e->done || qsem_ismod(S, e->a))
        return false;

    const QBind *const b = qsem_find(S, e->tok);

    for (uint32_t i = 0; i < n && !(b && b->fn); i++)
        if (qsem_is(e->tok, names[i])) {
            *k = i;
            return true;
        }

    return false;
}

/* Is e a chain's step, map(f) .. zip(b) or rev(): *k its QI_*. */
static bool qsem_step(QSem *const S, const QExpr *const e, uint32_t *const k)
{
    static const char *const rev[] = {"rev"};

    if (qsem_method_of(S, e, qsem_steps, countof(qsem_steps), k))
        return true;
    if (!qsem_method_of(S, e, rev, 1, k))
        return false;

    *k = QI_REV;
    return true;
}

/* A checked value read again where it is: a copy of its node. */
static QExpr *qsem_dup(QSem *const S, const QExpr *const e)
{
    QExpr *const d = qast_alloc(S->mem, sizeof *d);
    *d = *e;
    d->done = true;
    return d;
}

static QExpr *qsem_intconst(QSem *const S, const QTok *const t, const int64_t v)
{
    return qsem_const(S, t, QY_INT, (QVal){.i = v});
}

/* A string's or a list's length, place a's. */
static QExpr *qsem_lenof(QSem *const S, QExpr *const a)
{
    if (a->type != QY_STRING && qast_ty(S->prog, a->type)->kind == QT_ARRAY)
        return qsem_intconst(S, a->tok, qast_ty(S->prog, a->type)->count);

    QExpr *const n = qast_expr(S->mem, QE_CALL, a->tok);
    n->builtin = QB_LEN;
    n->a = a;
    n->type = QY_INT;
    return n;
}

/* Element at index variable i of a string's or a list's place a. */
static QExpr *qsem_at_index(QSem *const S, QExpr *const a, QVar *const i)
{
    QExpr *const e = qast_expr(S->mem, a->type == QY_STRING ? QE_CALL : QE_INDEX, a->tok);
    e->builtin = a->type == QY_STRING ? QB_BYTE : QB_NONE;
    e->a = a;
    e->b = qsem_nameof(S, i, a->tok);
    e->type = a->type == QY_STRING ? QY_UBYTE : qast_ty(S->prog, a->type)->elem;
    return e;
}

/* Unchecked a, what a chain or its zip runs over: a list's, an array's, a map's keys' or a
   string's place, held first when fresh (*src, in *hold for a string); null when it is none. */
static QExpr *qsem_over(QSem *const S, QExpr *const c, QExpr **const src, QVar **const hold)
{
    QExpr *a = qsem_addressed(S, qsem_expr(S, c, QY_VOID));
    *src = nullptr;
    *hold = nullptr;

    if (a->type == QY_ERROR)
        return nullptr;

    if (a->type == QY_STRING) {
        if (a->kind == QE_NAME && a->var)
            return a;

        *src = a;
        *hold = qsem_var(S, a->tok, QY_STRING, QV_TEMP);
        return qsem_nameof(S, *hold, a->tok);
    }

    if (!qast_array(S->prog, a->type) && !qsem_ismap(S, a->type)) {
        qsem_say(
            S,
            a->tok,
            qsem_aname(S, a->type),
            " has no chain: a list, an array, a map, a string, a range or a generator has",
            nullptr
        );
        return nullptr;
    }

    if (qast_fresh(a)) {
        *src = a;
        a = qsem_nameof(S, a->var, a->tok);
    }

    return qsem_ismap(S, a->type) ? qsem_keysof(S, a) : a;
}

/* Kept generator v as chain it's source: its next() each time, to got. */
static void qsem_fromgen(QSem *const S, QIter *const it, QVar *const v)
{
    const QFn *const r = qast_compound(v->type) ? qast_ty(S->prog, v->type)->gen : nullptr;

    if (!r)
        return;

    QExpr *const call = qast_expr(S->mem, QE_CALL, it->tok);
    call->fn = (QFn *)r;
    call->type = r->result;
    it->from = QO_GEN;
    it->gen = v;
    it->got = qsem_var(S, it->tok, r->result, QV_TEMP);
    it->next = qsem_into(S, it->got, call);
    it->parts[0] = qsem_part(S, qsem_nameof(S, it->got, it->tok), QF_VAL);
    it->nparts = 1;
}

/* Chain it's source c, unchecked: its element its part; false after an error. */
static bool qsem_source(QSem *const S, QIter *const it, QExpr *const c)
{
    it->parts = qast_alloc(S->mem, 4 * sizeof *it->parts);
    QFn *const g = qsem_genof(S, c);
    const QBind *const b = qsem_lookup(S, c);

    if (g) {
        it->start = c;
        qsem_fromgen(S, it, qsem_keptas(S, c, g, it->tok));
        return it->nparts;
    }

    if (b && b->var && qsem_isgen(S, b->var->type)) {
        if (b->var->kind != QV_VAR)
            qsem_say(S, c->tok, "a generator is called in its own fn, not a lambda's", nullptr);
        else
            qsem_fromgen(S, it, b->var);

        return it->nparts;
    }

    it->i = qsem_var(S, it->tok, QY_INT, QV_TEMP);
    it->lim = qsem_var(S, it->tok, QY_INT, QV_TEMP);
    it->stp = qsem_var(S, it->tok, QY_INT, QV_TEMP);

    if (c->kind == QE_RANGE) {
        it->from = QO_RANGE;
        it->lo = qsem_coerce(S, qsem_expr(S, c->a, QY_INT), QY_INT);
        it->hi = qsem_coerce(S, qsem_expr(S, c->b, QY_INT), QY_INT);
        it->parts[it->nparts++] = qsem_nameof(S, it->i, it->tok);
        return it->lo->type != QY_ERROR && it->hi->type != QY_ERROR;
    }

    it->list = qsem_over(S, c, &it->src, &it->hold);

    if (!it->list)
        return false;

    it->from = it->list->type == QY_STRING ? QO_STRING : QO_LIST;
    it->lo = qsem_intconst(S, it->tok, 0);
    it->hi = qsem_lenof(S, it->list);
    it->parts[it->nparts++] = qsem_at_index(S, it->list, it->i);
    return true;
}

/* Lambda or fn a applied to extra (n of them) then chain it's parts, checked for a place of type
   hint: a one-line lambda's body, its parameters naming them; a name's call. */
static QExpr *qsem_applied(
    QSem *const S,
    const QIter *const it,
    QExpr *const a,
    QExpr *const *const extra,
    const uint32_t n,
    const QType hint
)
{
    const uint32_t count = n + it->nparts;

    if (a->kind == QE_NAME) { /* a fn, a fn value, a lambda passed in: called */
        QExpr *const call = qast_expr(S->mem, QE_CALL, a->tok);
        QArg **next = &call->args;

        for (uint32_t k = 0; k < count; k++) {
            QArg *const x = qast_alloc(S->mem, sizeof *x);
            x->e = qsem_dup(S, k < n ? extra[k] : it->parts[k - n]);
            *next = x;
            next = &x->next;
        }

        return qsem_call(S, call);
    }

    if (a->kind != QE_LAMBDA || !a->lam->body) {
        qsem_say(S, a->tok, "a chain takes a one-line lambda or a fn", nullptr);
        return qsem_const(S, a->tok, QY_ERROR, (QVal){});
    }

    QLam *const l = a->lam;

    if (l->npar != count) {
        qsem_say(
            S,
            l->tok,
            "the lambda takes ",
            count == 1 ? "one value" : qsem_join(S, qsem_uint(S, count), " values", ""),
            " here",
            nullptr
        );
        return qsem_const(S, a->tok, QY_ERROR, (QVal){});
    }

    qsem_push(S);

    for (uint32_t k = 0; k < count; k++) {
        const QParam *const p = &l->params[k];
        QExpr *const v = k < n ? extra[k] : it->parts[k - n];
        const QType w = p->type ? qsem_type(S, p->type) : v->type;

        if (w != v->type && w != QY_ERROR && v->type != QY_ERROR)
            qsem_say(
                S,
                p->name,
                qsem_text(S, p->name),
                " is ",
                qsem_aname(S, v->type),
                " here, not ",
                qsem_aname(S, w),
                nullptr
            );

        QVar *const x = qsem_var(S, p->name, v->type, QV_PARAM);
        x->place = v;
        qsem_bind(S, p->name, x, nullptr);
    }

    QExpr *v = qsem_expr(S, l->body, hint);

    if (l->rtype)
        v = qsem_coerce(S, v, qsem_type(S, l->rtype));

    qsem_pop(S);
    return v;
}

/* Call c's one argument, unchecked; null, said, when it has more or none. */
static QExpr *qsem_onearg(QSem *const S, const QExpr *const c)
{
    if (c->args && !c->args->next && !c->args->name)
        return c->args->e;

    qsem_say(S, c->tok, qsem_text(S, c->tok), "() takes one value", nullptr);
    return nullptr;
}

/* zip(b)'s b, unchecked: a range's or a place's element, kept in x at b's index n, past k. */
static void qsem_zip(QSem *const S, QIter *const it, QStage *const st, QExpr *const b)
{
    st->n = qsem_var(S, it->tok, QY_INT, QV_TEMP);
    st->k = qsem_var(S, it->tok, QY_INT, QV_TEMP);
    QExpr *e;

    if (b->kind == QE_RANGE) {
        st->of = qsem_coerce(S, qsem_expr(S, b->a, QY_INT), QY_INT);
        st->to = qsem_coerce(S, qsem_expr(S, b->b, QY_INT), QY_INT);
        e = qsem_nameof(S, st->n, it->tok);
    } else {
        QExpr *src;
        QVar *hold;
        QExpr *const a = qsem_over(S, b, &src, &hold);

        if (!a || src) {
            if (a)
                qsem_say(S, b->tok, "zip() takes a variable's list or string, or a range", nullptr);

            it->nparts = 0;
            return;
        }

        st->of = a;
        st->str = a->type == QY_STRING;
        st->to = qsem_lenof(S, a);
        e = qsem_at_index(S, a, st->n);
    }

    st->x = qsem_var(S, it->tok, e->type, QV_TEMP);
    st->e = qsem_into(S, st->x, qsem_owned(S, e, false));
    it->parts[it->nparts++] = qsem_nameof(S, st->x, it->tok);
}

/* Chain it's step c, its kind k; last: the sink's own input, which a sum or a collect reads
   once. */
static void qsem_stage(
    QSem *const S,
    QIter *const it,
    QExpr *const c,
    const uint32_t k,
    const bool last
)
{
    if (k == QI_REV) {
        for (uint32_t j = 0; j < it->n; j++)
            if (it->stages[j].kind != QI_MAP && it->stages[j].kind != QI_FILTER) {
                qsem_say(S, c->tok, "rev() goes before take, skip and zip", nullptr);
                return;
            }

        if (c->args)
            qsem_say(S, c->tok, "rev() takes no values", nullptr);
        if (it->from == QO_GEN)
            qsem_say(S, c->tok, "a generator can't go backwards", nullptr);

        it->rev = !it->rev;
        return;
    }

    QExpr *const a = qsem_onearg(S, c);
    QStage *const st = &it->stages[it->n++];
    *st = (QStage){.kind = (QStageKind)k};

    if (!a) {
        it->nparts = 0;
        return;
    }

    switch (st->kind) {
    case QI_MAP: {
        QExpr *const v = qsem_settle(S, qsem_applied(S, it, a, nullptr, 0, QY_VOID));
        const bool once = last && (it->sink == QZ_SUM || it->sink == QZ_COLLECT);

        if (v->type == QY_VOID)
            qsem_say(S, a->tok, "map() needs a value", nullptr);

        if (qsem_cheap(v) || once) {
            st->e = v;
            it->parts[0] = v;
        } else {
            st->x = qsem_var(S, v->tok, v->type, QV_TEMP);
            st->e = qsem_into(S, st->x, qsem_owned(S, v, false));
            it->parts[0] = qsem_nameof(S, st->x, v->tok);
        }

        it->nparts = v->type == QY_ERROR ? 0 : 1;
        return;
    }

    case QI_FILTER:
        st->e = qsem_coerce(S, qsem_applied(S, it, a, nullptr, 0, QY_BOOL), QY_BOOL);
        return;

    case QI_TAKE:
    case QI_SKIP:
        st->e = qsem_coerce(S, qsem_expr(S, a, QY_INT), QY_INT);
        st->n = qsem_var(S, c->tok, QY_INT, QV_TEMP);
        st->k = qsem_var(S, c->tok, QY_INT, QV_TEMP);
        return;

    case QI_ZIP:
        qsem_zip(S, it, st, a);
        return;
    }
}

/* T? of chain it's acc, has it. */
static QExpr *qsem_maybe(QSem *const S, const QIter *const it)
{
    const QType t = qsem_wrapof(S, QT_OPT, it->acc->type);
    QExpr *const e = qsem_wrapped(S, it->tok, t, true, qsem_nameof(S, it->acc, it->tok));
    e->argv[QF_FLAG] = qsem_nameof(S, it->has, it->tok);
    return e;
}

/* Chain it used up by c, sink k: what it keeps and gives after its loop. */
static QType qsem_sink(QSem *const S, QIter *const it, QExpr *const c, QExpr *const e)
{
    const QSink k = it->sink;
    const QType t = it->nparts == 1 ? it->parts[0]->type : QY_ERROR;
    const QExpr *const a = c->args ? c->args->e : nullptr;
    const bool pred = k == QZ_ANY || k == QZ_ALL || k == QZ_FIND || (k == QZ_COUNT && a);
    const bool single = k != QZ_COUNT && k != QZ_ANY && k != QZ_ALL && k != QZ_FOLD;

    if (!it->nparts)
        return QY_ERROR;

    if (single && it->nparts != 1) {
        qsem_say(
            S,
            c->tok,
            qsem_text(S, c->tok),
            "() takes one value: map the pair to one",
            nullptr
        );
        return QY_ERROR;
    }

    if (pred) {
        QExpr *const p = qsem_onearg(S, c);
        it->pred = p ? qsem_coerce(S, qsem_applied(S, it, p, nullptr, 0, QY_BOOL), QY_BOOL)
                     : qsem_const(S, c->tok, QY_ERROR, (QVal){});
    } else if (k != QZ_FOLD && c->args) {
        qsem_say(S, c->tok, qsem_text(S, c->tok), "() takes no values", nullptr);
        return QY_ERROR;
    }

    if ((k == QZ_SUM && !qast_numeric(t)) ||
        ((k == QZ_MIN || k == QZ_MAX) && !qast_numeric(t) && t != QY_STRING)) {
        if (t != QY_ERROR)
            qsem_say(
                S,
                c->tok,
                qsem_text(S, c->tok),
                "() takes numbers",
                k == QZ_SUM ? "" : " or strings",
                ", not ",
                qsem_aname(S, t),
                nullptr
            );

        return QY_ERROR;
    }

    switch (k) {
    case QZ_SUM:
    case QZ_COUNT:
    case QZ_ANY:
    case QZ_ALL:
        it->acc = qsem_var(
            S,
            c->tok,
            k == QZ_SUM     ? qsem_sumof(t)
            : k == QZ_COUNT ? QY_INT
                            : QY_BOOL,
            QV_TEMP
        );
        it->result = qsem_nameof(S, it->acc, c->tok);
        return it->acc->type;

    case QZ_MIN:
    case QZ_MAX:
    case QZ_FIND:
        it->acc = qsem_var(S, c->tok, t, QV_TEMP);
        it->has = qsem_var(S, c->tok, QY_BOOL, QV_TEMP);
        it->result = qsem_maybe(S, it);
        return it->result->type;

    case QZ_COLLECT: {
        it->acc = qsem_var(S, c->tok, qsem_listof(S, t, 0), QV_TEMP);
        QExpr *const p = it->push = qast_expr(S->mem, QE_LIST, c->tok);
        p->builtin = QB_PUSH;
        p->a = qsem_nameof(S, it->acc, c->tok);
        p->argv = qast_alloc(S->mem, sizeof *p->argv);
        p->argv[0] = qsem_addressed(S, qsem_owned(S, qsem_dup(S, it->parts[0]), false));
        p->nargv = 1;
        p->type = QY_VOID;
        e->var = it->acc;
        return it->acc->type;
    }

    case QZ_FOLD: {
        if (!a || !c->args->next || c->args->next->next) {
            qsem_say(S, c->tok, "fold() takes a first value and a fn(acc, x)", nullptr);
            return QY_ERROR;
        }

        QExpr *const init = qsem_settle(S, qsem_expr(S, c->args->e, QY_VOID));
        it->acc = qsem_var(S, c->tok, init->type, QV_TEMP);
        it->init = qsem_into(S, it->acc, qsem_owned(S, init, false));
        QExpr *const acc = qsem_nameof(S, it->acc, c->tok);
        QExpr *const v = qsem_applied(S, it, c->args->next->e, &acc, 1, init->type);
        it->pred = qsem_into(S, it->acc, qsem_owned(S, qsem_coerce(S, v, init->type), false));

        if (qsem_byplace(S, init->type))
            e->var = it->acc;
        else
            it->result = qsem_nameof(S, it->acc, c->tok);

        return init->type;
    }

    default:
        return QY_ERROR;
    }
}

/* Chain e, a.step(..)..: its source, its steps and, unless a for's (loop), its end, one loop
   e->iter; loop's a.step(..) e. Its source is borrowed while it runs; its lambdas are checked
   in the loop's scope. */
static QExpr *qsem_iter(QSem *const S, QExpr *e, const uint32_t k, QStmt *const loop)
{
    QIter *const it = qast_alloc(S->mem, sizeof *it);
    it->sink = loop ? QZ_FOR : (QSink)k;
    it->tok = e->tok;
    uint32_t n = 0, step;

    QExpr *c = loop ? e : e->a;

    for (; qsem_step(S, c, &step); c = c->a)
        n++;

    QExpr **const calls = qast_alloc(S->mem, (n ? n : 1) * sizeof *calls);
    uint32_t *const kinds = qast_alloc(S->mem, (n ? n : 1) * sizeof *kinds);
    c = loop ? e : e->a;

    for (uint32_t j = n; j-- > 0; c = c->a) {
        qsem_step(S, c, &kinds[j]);
        calls[j] = c;
    }

    it->stages = qast_alloc(S->mem, (n ? n : 1) * sizeof *it->stages);
    QStmt *const mark = loop ? loop : qast_alloc(S->mem, sizeof *mark);
    const uint32_t loans = S->nloans;
    bool ok = qsem_source(S, it, c);

    if (!loop)
        *mark = (QStmt){.kind = QS_CALL, .tok = e->tok};

    if (ok && it->from != QO_GEN && it->from != QO_RANGE)
        qsem_lend(S, qsem_root(it->list), mark);

    if (!loop) { /* its lambdas run in a loop */
        S->loops = qast_room(S->mem, S->loops, S->nloops, &S->caploops, sizeof *S->loops);
        S->loops[S->nloops++] = qsem_at(S, e->tok);
    }

    for (uint32_t j = 0; ok && j < n; j++) {
        qsem_stage(S, it, calls[j], kinds[j], j + 1 == n);
        ok = it->nparts != 0;
    }

    if (loop && !n) /* for x in a: a the source, kept */
        e = qast_expr(S->mem, QE_ITER, e->tok);

    e->type = ok && !loop ? qsem_sink(S, it, e, e) : ok ? QY_VOID : QY_ERROR;

    if (!loop) {
        S->nloops--;
        S->nloans = loans;
    }

    e->kind = QE_ITER;
    e->iter = it;
    e->a = nullptr;
    e->done = true;
    return e;
}

/* for x in a chain, s->e: its loop's names each a part, read where it is when cheap. */
static void qsem_chainfor(QSem *const S, QStmt *const s)
{
    s->e = qsem_iter(S, s->e, 0, s);
    QIter *const it = s->e->iter;
    uint32_t names = 0;

    for (const QDecl *d = s->decls; d; d = d->next)
        names++;

    if (s->mut)
        qsem_say(S, s->tok, "a chain's elements don't change: for mut takes a list", nullptr);
    if (s->e->type != QY_ERROR && names != it->nparts)
        qsem_say(S, s->tok, "for over a chain takes one name, or one for each of a zip's", nullptr);

    qsem_push(S);
    it->xs = qast_alloc(S->mem, (names ? names : 1) * sizeof *it->xs);
    uint32_t k = 0;

    for (const QDecl *d = s->decls; d; d = d->next, k++) {
        QExpr *const v = k < it->nparts && s->e->type != QY_ERROR ? it->parts[k] : nullptr;
        QVar *const x = it->xs[k] = qsem_var(S, d->name, v ? v->type : QY_ERROR, QV_LOOP);

        if (v && qsem_cheap(v) && !qsem_owns(S, v->type))
            x->place = v;
        else if (v)
            it->parts[k] = qsem_into(S, x, qsem_owned(S, qsem_dup(S, v), false));

        qsem_bind(S, d->name, x, nullptr);
    }

    s->var = it->xs[0];
}

/* ---- generators */

/* Fn f's body checked, its parameters in its scope; S->fn f after. */
static void qsem_fnbody(QSem *const S, QFn *const f)
{
    S->fn = f;
    const uint32_t marks = S->nmarks;
    qsem_push(S);
    qsem_typeargs(S, f);

    for (uint32_t k = 0; k < f->npar; k++)
        if (!f->params[k].hidden)
            qsem_bind(S, f->params[k].name, f->params[k].var, nullptr);

    for (QStmt *s = f->def->next; s != f->end; s = s->next)
        qsem_stmt(S, s);

    qsem_stmt(S, f->end);

    while (S->nmarks > marks)
        qsem_pop(S);
}

/* The generator unchecked call e calls, g(..); null none. */
static QFn *qsem_genof(QSem *const S, const QExpr *const e)
{
    if (e->kind != QE_CALL || (e->a && !qsem_ismod(S, e->a)))
        return nullptr;

    QExpr name = {.kind = QE_NAME, .tok = e->tok, .a = e->a};

    const QBind *const b = qsem_qual(S, &name);
    return b && b->fn && b->fn->ytx ? b->fn : nullptr;
}

/* yield v: numbered in its instance; a resumable's v its next() result. */
static void qsem_yield(QSem *const S, QStmt *const s)
{
    QFn *const f = s->fn;
    f->yields = qast_room(S->mem, f->yields, f->nyields, &f->capyields, sizeof *f->yields);
    f->yields[f->nyields++] = s;
    s->by = (int32_t)f->nyields;
    s->e = qsem_value(S, s->e, f->ytype);

    if (f->resumable && f->result != QY_ERROR)
        s->e = qsem_into(S, f->res, qsem_coerce(S, s->e, f->result));
}

/* Inline instance i's parameters given call e's values, at for s: a constant's its value, a
   record's or list's place named where it is, lent to the loop; the rest stored at the for, the
   host's locals. A lambda's captures are its site's places. */
static void qsem_genargs(QSem *const S, QStmt *const s, QFn *const i, QExpr *const e)
{
    for (uint32_t k = 0; k < i->npar; k++) {
        QVar *const v = i->params[k].var;
        QExpr *const a = e->argv[k];

        if (i->params[k].lam)
            continue;

        if (i->params[k].hidden)
            v->place = a;
        else if (a->kind == QE_CONST) {
            v->constant = true;
            v->val = a->val;
        } else if (qsem_byplace(S, v->type) && !qast_fresh(a)) {
            v->place = a;
            qsem_lend(S, qsem_root(a), s);
        } else
            e->argv[k] = qsem_into(S, v, qsem_owned(S, a, false));

        v->kind = i->params[k].hidden ? v->kind : QV_LET;
    }
}

/* Does the fn checked loop over generator g, inlined in it? */
static bool qsem_inside(const QSem *const S, const QFn *const g)
{
    for (const QFn *f = S->fn; f && qast_inline(f); f = f->host)
        if (f->of == g)
            return true;

    return false;
}

/* for x in g(..): g's instance checked here, its code the for's, its state in this frame. x names
   the value of a sole yield that is a variable or a constant, else holds each yielded. */
static void qsem_genfor(QSem *const S, QStmt *const s, QFn *const g)
{
    const QDecl *const x = s->decls;
    QFn *i = nullptr;

    if (x && (x->next || s->mut))
        qsem_say(S, s->tok, "for over a generator takes one name, not mut", nullptr);

    if (qsem_inside(S, g))
        qsem_say(
            S,
            s->e->tok,
            qsem_text(S, g->name),
            " loops over itself: its for is inlined",
            nullptr
        );
    else {
        S->genfor = s;
        s->e = qsem_fncall(S, s->e, g);
        S->genfor = nullptr;
        i = s->e->type == QY_ERROR ? nullptr : s->e->fn;
    }

    qsem_push(S);

    if (i) {
        qsem_genargs(S, s, i, s->e);
        s->fn = i;

        QFn *const fn = S->fn;
        QLamAt *const lamat = S->lamat;
        const uint32_t lo = S->hidelo, hi = S->hidehi, loops = S->nloops;
        S->hidelo = qsem_floor(S);
        S->hidehi = S->nbinds;
        S->lamat = nullptr;
        S->nloops = 0;
        qsem_fnbody(S, i);
        S->fn = fn;
        S->lamat = lamat;
        S->hidelo = lo;
        S->hidehi = hi;
        S->nloops = loops;
    }

    if (!x)
        return;

    const QType t = i ? i->ytype : QY_ERROR;
    const QExpr *const one = i && i->nyields == 1 ? i->yields[0]->e : nullptr;
    s->var = qsem_var(S, x->name, t, QV_LOOP);

    if (one && !qsem_owns(S, t) && (one->kind == QE_CONST || (one->kind == QE_NAME && one->var)))
        s->var->place = i->yields[0]->e;

    for (uint32_t k = 0; i && !s->var->place && k < i->nyields; k++)
        i->yields[k]->e = qsem_into(S, s->var, i->yields[k]->e);

    if (i && i->nyields > 1)
        i->resume = qsem_var(S, s->tok, QY_INT, QV_TEMP);

    qsem_bind(S, x->name, s->var, nullptr);

    for (const QDecl *d = x->next; d; d = d->next)
        qsem_bind(S, d->name, qsem_var(S, d->name, QY_ERROR, QV_LOOP), nullptr);
}

/* var it = g(..): g's instance a procedure, it its resume index; each of its arguments kept in
   a variable of this frame, which it reaches by reference. */
static void qsem_kept(QSem *const S, QDecl *const d, const QKind kw, QFn *const g)
{
    if (kw != QK_VAR)
        qsem_say(S, d->name, "a generator changes as it runs: keep it in a var", nullptr);
    if (d->type)
        qsem_say(S, d->type->tok, "a generator's type is its fn's: leave out as", nullptr);

    d->var = qsem_keptas(S, d->value, g, d->name);
    qsem_bind(S, d->name, d->var, nullptr);
}

/* Call e of generator g kept in a new variable named name, its resume index: e's arguments
   each kept in a variable of this frame. */
static QVar *qsem_keptas(QSem *const S, QExpr *const call, QFn *const g, const QTok *const name)
{
    S->genkept = true;
    QExpr *const e = qsem_fncall(S, call, g);
    S->genkept = false;
    QFn *const r = e->type == QY_ERROR ? nullptr : e->fn;
    QType t = QY_ERROR;

    if (r) {
        t = qsem_newty(S, QT_GEN);
        qast_ty(S->prog, t)->gen = r;
        r->keep = qast_alloc(S->mem, (r->npar + 1) * sizeof *r->keep);

        for (uint32_t k = 0; k < r->npar; k++) {
            const QParam *const p = &r->params[k];

            if (p->hidden || p->lam) {
                r->keep[k] = e->argv[k];
                continue;
            }

            QVar *const v = qsem_var(S, p->name, p->var->type, QV_VAR);
            e->argv[k] = qsem_into(S, v, qsem_owned(S, e->argv[k], false));
            r->keep[k] = qsem_nameof(S, v, e->tok);
        }
    }

    QVar *const v = qsem_var(S, name, t, QV_VAR);

    if (r)
        r->keep[r->npar] = qsem_nameof(S, v, name);

    return v;
}

/* Resumable r, checked: its resume index and each of its variables kept by its host, each a
   parameter by reference after its own, as r->keep names them. */
static void qsem_keep(QSem *const S, QFn *const r)
{
    uint32_t n = r->npar + 1;

    for (const QVar *v = S->prog->vars; v; v = v->next)
        n += qast_home(v->fn) == r && v->kind != QV_PARAM && v->kind != QV_MUT &&
             v->kind != QV_RESULT && !v->constant && !v->place;

    QParam *const params = qast_alloc(S->mem, n * sizeof *params);
    QExpr **const keep = qast_alloc(S->mem, n * sizeof *keep);
    memcpy(params, r->params, r->npar * sizeof *params);
    memcpy(keep, r->keep, (r->npar + 1) * sizeof *keep);
    r->resume = qsem_var(S, r->name, QY_INT, QV_MUT);
    const uint32_t own = r->npar;
    QVar *const last = S->lastvar;
    params[own] = (QParam){
        .name = r->name,
        .mut = true,
        .hidden = true,
        .var = r->resume,
    };

    r->npar++;

    for (QVar *v = S->prog->vars; v != last->next; v = v->next) {
        if (qast_home(v->fn) != r || v->kind == QV_PARAM || v->kind == QV_MUT ||
            v->kind == QV_RESULT || v->constant || v->place)
            continue;

        QVar *const h = qsem_var(S, v->name, v->type, QV_TEMP);
        h->fn = r->host;
        v->byref = true;
        keep[r->npar] = qsem_nameof(S, h, v->name);
        params[r->npar++] = (QParam){
            .name = v->name,
            .mut = true,
            .hidden = true,
            .var = v,
        };
    }

    r->nhidden += r->npar - own;
    r->params = params;
    r->keep = keep;
}

/* it.next(), it a var generator v: its instance called, its state passed by reference; what
   it yields next, none when done. */
static QExpr *qsem_next(QSem *const S, QExpr *const e, QVar *const v)
{
    const QFn *const r = qast_compound(v->type) ? qast_ty(S->prog, v->type)->gen : nullptr;
    e->type = QY_ERROR;

    if (!qsem_is(e->tok, "next")) {
        qsem_say(S, e->tok, "a generator has next() alone", nullptr);
        return e;
    }

    if (e->args)
        qsem_say(S, e->args->e->tok, "next() takes no values", nullptr);
    if (r && v->kind != QV_VAR) {
        qsem_say(S, e->tok, "a generator is called in its own fn, not a lambda's", nullptr);
        return e;
    }

    if (!r)
        return e;

    e->a = nullptr;
    e->fn = (QFn *)r;
    e->type = r->result;
    return e;
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

    case QS_YIELD:
        qsem_yield(S, s);
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

static bool qsem_leaves(const QStmt *const s)
{
    return s && (s->kind == QS_RETURN || s->kind == QS_BREAK || s->kind == QS_CONTINUE);
}

/* Where test c proves it, x is none or is Error false (proved: c's truth that does so), a let's
   or a parameter's x is its T to the end of the scope: an alias of its value, bound over x. */
static void qsem_narrow(QSem *const S, const QExpr *const c, const bool proved)
{
    if (c->kind != QE_IS || c->neg != proved)
        return;

    const QExpr *const a = c->a;

    if (a->kind != QE_NAME || !a->var || (a->var->kind != QV_LET && a->var->kind != QV_PARAM))
        return;

    QVar *const v = a->var;
    QExpr *const place = qsem_part(S, qsem_nameof(S, v, a->tok), QF_VAL);
    QVar *const n = qsem_var(S, v->name, place->type, QV_EACH);
    n->place = place;
    S->binds = qast_room(S->mem, S->binds, S->nbinds, &S->capbinds, sizeof *S->binds);
    S->binds[S->nbinds++] = (QBind){.name = v->name, .var = n};
}

/* if x is none whose body last leaves: x narrowed after it. */
static void qsem_guard(QSem *const S, const QExpr *const c, const QStmt *const last)
{
    if (qsem_leaves(last))
        qsem_narrow(S, c, false);
}

/* The if an else follows straight, no elseif between; null none. */
static const QStmt *qsem_ifof(const QStmt *const s)
{
    for (const QStmt *p = s->prev; p; p = p->prev)
        if (p->kind == QS_IF && !p->oneline && p->link == s)
            return p;

    return nullptr;
}

static void qsem_stmt(QSem *const S, QStmt *const s)
{
    switch (s->kind) {
    case QS_IF:
        qsem_cond_of(S, s);

        qsem_push(S);
        qsem_narrow(S, s->e, true);

        if (s->oneline) {
            if (s->body) /* null: it failed to parse */
                qsem_simple(S, s->body);
            qsem_pop(S);
            qsem_guard(S, s->e, s->body);
        }

        return;

    case QS_ELSEIF:
        qsem_pop(S);
        qsem_cond_of(S, s);
        qsem_push(S);
        return;

    case QS_ELSE: {
        qsem_pop(S);
        qsem_push(S);
        const QStmt *const i = qsem_ifof(s);

        if (i)
            qsem_narrow(S, i->e, false);

        return;
    }

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
        while (S->nloans && S->loans[S->nloans - 1].loop == s->link)
            S->nloans--;

        [[fallthrough]];

    case QS_ENDWHILE:
    case QS_ENDDO:
        S->nloops -= S->nloops > 0;
        [[fallthrough]];

    case QS_ENDIF:
        qsem_pop(S);

        if (s->kind == QS_ENDIF && s->link->kind == QS_IF && s->link->link == s)
            qsem_guard(S, s->link->e, s->prev);

        return;

    case QS_ENDFN: {
        const QFn *const f = s->fn;

        if (f->resumable && f->result != QY_ERROR) /* done: none */
            s->e = qsem_wrapped(S, s->tok, f->result, false, nullptr);

        if (!f->ytx && f->result != QY_VOID && f->result != QY_ERROR && !qsem_returns(s->prev))
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


/* ---- the passes */

/* Module m's statement after s, past a fn's: null after its last. */
static QStmt *qsem_nextin(const QMod *const m, QStmt *s)
{
    if (s->kind == QS_FN && !s->fn->end)
        return nullptr;
    if (s->kind == QS_FN)
        s = s->fn->end;

    return s == m->tail ? nullptr : s->next;
}

/* Module m's consts, outside blocks and fns, in order. */
static void qsem_consts(QSem *const S, const QMod *const m)
{
    uint32_t depth = 0;

    for (QStmt *s = m->head; s; s = qsem_nextin(m, s)) {
        if (s->kind == QS_FN)
            continue;

        const bool opens = (s->kind == QS_IF && !s->oneline) || s->kind == QS_WHILE ||
                           s->kind == QS_DO || s->kind == QS_FOR || s->kind == QS_MATCH;
        const bool closes = s->kind == QS_ENDIF || s->kind == QS_ENDWHILE || s->kind == QS_ENDDO ||
                            s->kind == QS_ENDFOR || s->kind == QS_ENDMATCH;

        depth += opens;
        depth -= closes && depth;

        if (!depth && s->kind == QS_DECL && s->op == QK_CONST) {
            s->early = S->early = true;
            S->pub = s->pub;
            qsem_decls(S, s);
            S->early = S->pub = false;
        }
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
        S->pub = s->pub;
        qsem_bindty(S, s->decls->name, t);
        S->pub = false;

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

        if (p->mut && f->ytx) {
            qsem_say(S, p->name, "a generator's parameters are not mut", nullptr);
            p->mut = false;
        }

        p->var = qsem_var(S, p->name, t, p->mut ? QV_MUT : QV_PARAM);
    }

    f->result = f->rtype ? qsem_type(S, f->rtype) : QY_VOID;
    f->ytype = f->ytx ? qsem_type(S, f->ytx) : QY_VOID;

    if (f->resumable) /* next()'s: the value yielded, none when done */
        f->result = f->ytype == QY_ERROR ? QY_ERROR : qsem_wrapof(S, QT_OPT, f->ytype);

    f->byarg = qsem_byplace(S, f->result);

    if (f->native)
        qsem_native(S, f);

    if (f->result != QY_VOID)
        f->res = qsem_var(S, f->name, f->result, QV_RESULT);

    f->sig = true;
    S->fn = nullptr;
}

/* Operator fn f checked and kept by its first parameter's type, a record. */
static void qsem_operdef(QSem *const S, QFn *const f)
{
    const char *const op = qast_spelling(f->op);
    const QType t = f->npar ? f->params[0].var->type : QY_ERROR;
    const QType r = f->op == QK_EQ || f->op == QK_LT ? QY_BOOL : f->result;

    if (f->npar != 2 || f->params[0].mut || f->params[1].mut || f->params[1].def)
        qsem_say(S, f->name, "an operator takes two plain values", nullptr);
    else if (t == QY_ERROR || f->result == QY_ERROR)
        return;
    else if (!qsem_isrec(S, t))
        qsem_say(S, f->name, "an operator's first value is a record's", nullptr);
    else if (f->result != r || r == QY_VOID)
        qsem_say(
            S,
            f->name,
            "operator ",
            op,
            r == QY_BOOL ? " gives a bool" : " gives a value",
            nullptr
        );
    else if (qsem_operof(S, t, f->op))
        qsem_say(S, f->name, qsem_aname(S, t), " has an operator ", op, " already", nullptr);
    else {
        QTy *const y = qast_ty(S->prog, t);
        f->opnext = y->opers;
        y->opers = f;
    }
}

/* A number, a bool or a string: a native's argument and result. */
static bool qsem_nativeok(const QType t)
{
    return qast_numeric(t) || t == QY_BOOL || t == QY_STRING || t == QY_ERROR;
}

/* Native fn f's entry, and its types: numbers, bools and strings, by value. */
static void qsem_native(QSem *const S, QFn *const f)
{
    f->nat = qnat_find(f->name->text, f->name->len);

    if (!f->nat)
        qsem_say(S, f->name, "no native entry is named ", qsem_text(S, f->name), nullptr);

    const bool plain = f->result == QY_VOID || f->result == QY_ERROR || qsem_nativeok(f->result);

    for (uint32_t k = 0; k < f->npar; k++) {
        const QParam *const p = &f->params[k];

        if (p->mut || !qsem_nativeok(p->var->type))
            qsem_say(S, p->name, "a native fn takes numbers, bools and strings, not mut", nullptr);
    }

    if (!plain)
        qsem_say(S, f->rtype->tok, "a native fn gives a number, a bool or a string", nullptr);
}

/* May module k's top level have s: the main's anything, another's declarations alone? */
static bool qsem_declares(const uint32_t k, const QStmt *const s)
{
    switch (s->kind) {
    case QS_DECL:
    case QS_TYPE:
    case QS_FIELD:
    case QS_ENDTYPE:
    case QS_ENUM:
    case QS_VARIANT:
    case QS_ENDENUM:
    case QS_IMPORT:
    case QS_FROM:
    case QS_FN:
        return true;

    default:
        return !k;
    }
}

/* Module k's statements, fn bodies and early consts skipped; then its vars are all there. */
static void qsem_module(QSem *const S, const uint32_t k)
{
    const QMod *const m = &S->prog->mods[k];

    for (QStmt *s = m->head; s; s = qsem_nextin(m, s)) {
        if (s->kind == QS_FN || s->early)
            continue;

        if (!qsem_declares(k, s)) {
            qsem_say(
                S,
                s->tok,
                "a module runs nothing: its top level has fn, type, enum, const, var, let and "
                "import",
                nullptr
            );
            continue;
        }

        S->pub = s->pub && !S->nmarks;
        qsem_stmt(S, s);
        S->pub = false;
    }

    S->scopes[k].ready = true;
}

/* A new scope, a package folder's when mod is QMOD_NONE: its number + 1, a binding's scope. */
static uint32_t qsem_newscope(QSem *const S, const uint32_t mod)
{
    S->scopes = qast_room(S->mem, S->scopes, S->nscopes, &S->capscopes, sizeof *S->scopes);
    S->scopes[S->nscopes] = (QScope){.mod = mod};

    return ++S->nscopes;
}

/* In scope k, the package folder name names, made the first time: its scope + 1. */
static uint32_t qsem_package(QSem *const S, const uint32_t k, const QTok *const name)
{
    const QBind *const b = qsem_exported(S, k, name);

    if (b && b->scope && S->scopes[b->scope - 1].mod == QMOD_NONE)
        return b->scope;

    const uint32_t sc = qsem_newscope(S, QMOD_NONE);
    qsem_scoped(S, &S->scopes[k], (QBind){.name = name, .scope = sc});

    return sc;
}

/* import im in module k: its name bound to it, a.b's a a package's folder holding b. */
static void qsem_import(QSem *const S, const uint32_t k, const QImport *const im)
{
    const QBind spoilt = {.name = im->as ? im->as : im->path[0], .ty = QY_ERROR};

    const QBind b = {.name = im->as ? im->as : im->path[im->npath - 1], .scope = im->mod + 1};

    if (im->mod == QMOD_NONE) { /* the loader said */
        qsem_scoped(S, &S->scopes[k], spoilt);
        return;
    }

    if (im->as || im->npath == 1) {
        qsem_scoped(S, &S->scopes[k], b);
        return;
    }

    uint32_t in = k;

    for (uint32_t i = 0; i + 1 < im->npath; i++)
        in = qsem_package(S, in, im->path[i]) - 1;

    if (!qsem_exported(S, in, b.name))
        qsem_scoped(S, &S->scopes[in], b);
}

/* from m import X [as Y], ..: each Y an alias of m's X, found when used; a package folder's
   names its modules. */
static void qsem_from(QSem *const S, const uint32_t k, const QStmt *const s)
{
    for (const QImport *im = s->names; im; im = im->next) {
        const QTok *const name = im->as ? im->as : im->tok;
        QBind b = {.name = name, .ty = QY_ERROR};

        if (s->imps->folder && im->mod != QMOD_NONE)
            b = (QBind){.name = name, .scope = im->mod + 1};
        else if (!s->imps->folder && s->imps->mod != QMOD_NONE)
            b = (QBind){.name = name, .scope = s->imps->mod + 1, .alias = im->tok};

        qsem_scoped(S, &S->scopes[k], b);
    }
}

/* Each module's scope, its imports bound. */
static void qsem_scopes(QSem *const S)
{
    for (uint32_t k = 0; k < S->prog->nmods; k++)
        qsem_newscope(S, k);

    for (uint32_t k = 0; k < S->prog->nmods; k++) {
        const QMod *const m = &S->prog->mods[k];

        for (QStmt *s = m->head; s; s = qsem_nextin(m, s)) {
            for (const QImport *im = s->kind == QS_IMPORT ? s->imps : nullptr; im; im = im->next)
                qsem_import(S, k, im);

            if (s->kind == QS_FROM)
                qsem_from(S, k, s);
        }
    }
}

/* Each from's names: an error where one its module does not export is. */
static void qsem_exports(QSem *const S)
{
    for (uint32_t k = 0; k < S->prog->nmods; k++) {
        const QMod *const m = &S->prog->mods[k];

        for (QStmt *s = m->head; s; s = qsem_nextin(m, s)) {
            const QImport *const from = s->kind == QS_FROM ? s->imps : nullptr;

            if (!from || from->folder || from->mod == QMOD_NONE)
                continue;

            const QBind mod = {.name = from->path[from->npath - 1], .scope = from->mod + 1};

            for (const QImport *im = s->names; im; im = im->next)
                qsem_member(S, &mod, im->tok);
        }
    }
}

/* Fn f's body, its parameters in its scope. */
static void qsem_body(QSem *const S, QFn *const f)
{
    if (!f->end || !f->sig || f->native || qast_generic(f) || f->lamof || qast_inline(f))
        return;

    qsem_fnbody(S, f);

    if (f->resumable)
        qsem_keep(S, f);

    S->fn = nullptr;
}

void qsem_program(QProg *const p, QMem *const m)
{
    QSem S = {.prog = p, .mem = m};

    qsem_scopes(&S);

    for (QFn *f = p->fns; f; f = f->next)
        if (!f->lamof && !f->op) {
            S.pub = f->pub;
            qsem_bind(&S, f->name, nullptr, f);
            S.pub = false;
        }

    qsem_types(&S);

    for (uint32_t i = 0; i < p->nmods; i++)
        qsem_consts(&S, &p->mods[p->order[i]]);

    for (uint32_t i = 0; i < p->tys.n; i++)
        qsem_typed(&S, QY_FIRST + i);

    for (QFn *f = p->fns; f; f = f->next) /* a generator's too, its instances' model */
        if (!f->ntp && !f->lamof)
            qsem_signature(&S, f);

    for (QFn *f = p->fns; f; f = f->next)
        if (f->op && (f->ntp || f->ytx))
            qsem_say(&S, f->name, "an operator is not generic and yields nothing", nullptr);
        else if (f->op)
            qsem_operdef(&S, f);

    for (uint32_t i = 0; i < p->nmods; i++) {
        qsem_module(&S, p->order[i]);

        while (S.nmarks)
            qsem_pop(&S);
    }

    for (QFn *f = p->fns; f; f = f->next)
        qsem_body(&S, f);

    qsem_exports(&S);
    qsem_moved(&S);
}
