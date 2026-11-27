/* Quickr's semantic pass: lists and maps, and the methods of values.
 *
 * # How it works
 * A list's and a map's methods are built in; a record's are its fns whose first parameter is
 * self, a method call a call of one.
 */
#include "qr/qsem_int.h"

/* ---- lists */

/* A list's method: a.push(x), a.pop(), a.insert(i, x), a.remove(i); a T[]'s a.resize(n); of a
   mutable one. */
static QExpr *qsem_listop(QSem *const S, QExpr *const e, const QBuiltin op)
{
    static const uint32_t counts[] =
        {[QB_PUSH] = 1, [QB_POP] = 0, [QB_INSERT] = 2, [QB_REMOVE] = 1, [QB_RESIZE] = 1};

    static const char *const say[] = {
        [QB_PUSH] = "push() takes a value",
        [QB_POP] = "pop() takes no values",
        [QB_INSERT] = "insert() takes an index and a value",
        [QB_REMOVE] = "remove() takes an index",
        [QB_RESIZE] = "resize() takes a size for each dimension",
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

    const QTy *const y = qast_array(S->prog, a->type) ? qast_ty(S->prog, a->type) : nullptr;
    const bool list = op != QB_RESIZE;

    if (!y || y->kind != QT_DYN || y->grows != list) { /* what has it said */
        const char *const has = !y || y->kind == QT_ARRAY
                                    ? (list ? "a list" : "an array T[]")
                                    : qsem_aname(S, qsem_arrayof(S, y->elem, 0, list));
        qsem_say(S, e->tok, "only ", has, " has ", qsem_text(S, e->tok), "()", nullptr);
        return e;
    }

    const bool fits = op == QB_RESIZE ? n && n <= MAXDIMS : n == counts[op];

    if (!fits || !qsem_plain(S, e, ~0u)) {
        qsem_say(S, e->tok, say[op], nullptr);
        return e;
    }

    const QType elem = y->elem;

    if (op == QB_RESIZE && qast_drops(S->prog, elem)) {
        qsem_say(S, e->tok, "resize() can't cut ", qsem_aname(S, elem), ": it has drop", nullptr);
        return e;
    }

    e->argv = qast_alloc(S->mem, (n > 2 ? n : 2) * sizeof *e->argv);
    e->nargv = n;
    const QArg *x = e->args;

    if (op == QB_RESIZE) {
        for (uint32_t k = 0; k < n; k++, x = x->next)
            e->argv[k] = qsem_coerce(S, qsem_expr(S, x->e, QY_INT), QY_INT);

        e->type = QY_VOID;
        return e;
    }

    if (op == QB_INSERT || op == QB_REMOVE) {
        e->argv[0] = qsem_coerce(S, qsem_expr(S, x->e, QY_INT), QY_INT);
        x = x->next;
    }

    if (op == QB_PUSH || op == QB_INSERT)
        e->argv[n - 1] = qsem_held(S, qsem_value(S, x->e, elem));
    else if (op != QB_RESIZE)
        qsem_dropix(S, elem); /* remove's, a pop's dropped when not kept */

    e->type = op == QB_POP ? elem : QY_VOID;
    return e;
}

/* a.shape(): a new int[], each of a's dimensions' size: a T[]'s its descriptor's. */
static QExpr *qsem_shape(QSem *const S, QExpr *const e)
{
    QExpr *const a = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    e->kind = QE_CALL;
    e->builtin = QB_SHAPE;
    e->type = QY_ERROR;

    if (a->type == QY_ERROR)
        return e;

    if (!qast_array(S->prog, a->type))
        qsem_say(S, e->tok, "only an array has shape()", nullptr);
    else if (e->args)
        qsem_say(S, e->tok, "shape() takes no values", nullptr);
    else
        e->type = qsem_arrayof(S, QY_INT, 0, false);

    return e;
}

/* a.swap(i, j): two elements of a mutable list or array exchanged, each index checked. */
static QExpr *qsem_swap(QSem *const S, QExpr *const e)
{
    const bool ok = qsem_changes(S, e->a, e->tok);
    QExpr *const a = e->a = qsem_expr(S, e->a, QY_VOID);
    e->kind = QE_LIST;
    e->builtin = QB_SWAP;
    e->type = QY_ERROR;
    uint32_t n = 0;

    for (const QArg *x = e->args; x; x = x->next)
        n++;

    if (!ok || a->type == QY_ERROR)
        return e;

    if (!qast_array(S->prog, a->type)) {
        qsem_say(S, e->tok, "only a list or an array has swap()", nullptr);
        return e;
    }

    if (n != 2 || !qsem_plain(S, e, ~0u)) {
        qsem_say(S, e->tok, "swap() takes two indexes", nullptr);
        return e;
    }

    e->argv = qast_alloc(S->mem, 2 * sizeof *e->argv);
    e->nargv = 2;
    e->argv[0] = qsem_coerce(S, qsem_expr(S, e->args->e, QY_INT), QY_INT);
    e->argv[1] = qsem_coerce(S, qsem_expr(S, e->args->next->e, QY_INT), QY_INT);
    e->type = QY_VOID;
    return e;
}

/* a.sort(less): a mutable list's or array's elements in order, less(x, y) a bool on two of them;
   with none, numbers' or strings' by value. */
static QExpr *qsem_sort(QSem *const S, QExpr *const e)
{
    const bool ok = qsem_changes(S, e->a, e->tok);
    QExpr *const a = e->a = qsem_expr(S, e->a, QY_VOID);
    e->kind = QE_LIST;
    e->builtin = QB_SORT;
    e->type = QY_ERROR;

    if (!ok || a->type == QY_ERROR)
        return e;

    if (!qast_array(S->prog, a->type)) {
        qsem_say(S, e->tok, "only a list or an array has sort()", nullptr);
        return e;
    }

    if (!qsem_plain(S, e, ~0u))
        return e;

    if (e->args && e->args->next) {
        qsem_say(S, e->tok, "sort() takes one lambda, less than", nullptr);
        return e;
    }

    const QType elem = qast_ty(S->prog, a->type)->elem;

    if (!e->args) {
        if (!qast_numeric(elem) && elem != QY_STRING) {
            qsem_say(S, e->tok, "sort() takes a lambda here: fn(x, y) x < y", nullptr);
            return e;
        }

        e->type = QY_VOID;
        return e;
    }

    e->temps = qast_alloc(S->mem, 2 * sizeof *e->temps);
    QExpr *at[2];

    for (uint32_t k = 0; k < 2; k++) {
        e->temps[k] = qsem_var(S, e->tok, QY_INT, QV_TEMP);
        at[k] = qsem_at_index(S, a, e->temps[k]);
    }

    QExpr *const less = qsem_applied(S, &(QIter){}, e->args->e, at, 2, QY_BOOL);

    if (less->type != QY_BOOL) {
        if (less->type != QY_ERROR)
            qsem_say(S, e->tok, "sort()'s lambda gives a bool: x less than y", nullptr);

        return e;
    }

    e->argv = qast_alloc(S->mem, sizeof *e->argv);
    e->argv[0] = less;
    e->nargv = 1;
    e->type = QY_VOID;
    return e;
}

/* Map m's keys, the list its record's first field. */
QExpr *qsem_keysof(QSem *const S, QExpr *const m) { return qsem_part(S, m, 0); }

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
            qsem_ismap(S, m->type) ? "keys() takes no values" : "only a dict has keys()",
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
QExpr *qsem_len(QSem *const S, QExpr *const e)
{
    e->type = QY_ERROR;

    if (!qsem_plain(S, e, 1))
        return e;

    QExpr *a = qsem_addressed(S, qsem_expr(S, e->args->e, QY_VOID));

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
            "len() takes a list, a dict, a string or an array, not ",
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

/* copy(x): gone, assignment copies. */
QExpr *qsem_copy(QSem *const S, QExpr *const e)
{
    qsem_say(S, e->tok, "there is no copy(): an assignment copies, var b = a", nullptr);
    e->type = QY_ERROR;
    return e;
}

/* Type t's method name, null none. */
QFn *qsem_methodof(const QSem *const S, const QType t, const QTok *const name)
{
    const QTy *const y = qast_compound(t) ? qast_ty(S->prog, t) : nullptr;

    for (uint32_t k = 0; y && k < y->nmeths; k++)
        if (qsem_same(y->meths[k]->name, name))
            return y->meths[k];

    return nullptr;
}

/* Is t a type or an enum a program declares: one with methods? */
bool qsem_owntype(const QSem *const S, const QType t)
{
    const QTy *const y = qast_compound(t) ? qast_ty(S->prog, t) : nullptr;
    return y && y->def && (y->kind == QT_REC || y->kind == QT_ENUM);
}

/* Method f named at t, as a call or a value: an error if it is drop. */
bool qsem_named(QSem *const S, const QFn *const f, const QTok *const t)
{
    if (!qsem_is(f->name, "drop"))
        return true;

    qsem_say(S, t, "drop runs when its owner ends: it is never called by name", nullptr);
    return false;
}

/* Unchecked e's type as its names tell it, nothing checked: a variable's, its fields' and
   elements', a call's result, an enum's value; QY_VOID when they don't. */
QType qsem_peek(QSem *const S, const QExpr *const e)
{
    if (e->done)
        return e->type;

    switch (e->kind) {
    case QE_NAME: {
        const QBind *const b = qsem_qual(S, e);
        return b && b->var ? b->var->type : QY_VOID;
    }

    case QE_FIELD: {
        const QBind *const b =
            e->a->kind == QE_NAME || e->a->kind == QE_FIELD ? qsem_qual(S, e->a) : nullptr;

        if (b && b->ty) /* Enum.value */
            return qsem_methodof(S, b->ty, e->tok) ? QY_VOID : b->ty;
        if (b && b->scope) {
            const QBind *const x = qsem_qual(S, e);
            return x && x->var ? x->var->type : QY_VOID;
        }

        const QType t = qsem_peek(S, e->a);
        const QTy *const y = qsem_isrec(S, t) ? qast_ty(S->prog, t) : nullptr;

        for (uint32_t k = 0; y && k < y->nfields; k++)
            if (qsem_same(y->fields[k].name, e->tok))
                return y->fields[k].type;

        return QY_VOID;
    }

    case QE_INDEX: {
        const QType t = qsem_peek(S, e->a);
        const QTy *const y = qast_compound(t) ? qast_ty(S->prog, t) : nullptr;
        const bool elems = y && (y->kind == QT_DYN || y->kind == QT_ARRAY || y->kind == QT_MAP);
        return elems ? y->elem : QY_VOID;
    }

    case QE_CALL: {
        const QBind *b = nullptr;
        const QFn *f = nullptr;

        if (!e->a)
            b = qsem_find(S, e->tok);
        else if (e->a->kind == QE_NAME || e->a->kind == QE_FIELD) {
            const QBind *const m = qsem_qual(S, e->a);

            if (m && m->ty) {
                f = qsem_methodof(S, m->ty, e->tok);

                if (!f)
                    return m->ty; /* an enum's value with data */
            } else if (m && m->scope && !m->alias) {
                QExpr name = {.kind = QE_NAME, .tok = e->tok, .a = e->a};

                b = qsem_qual(S, &name);
            }
        }

        if (e->a && !b && !f)
            f = qsem_methodof(S, qsem_peek(S, e->a), e->tok);
        if (b && b->ty)
            return b->ty;
        if (b && b->fn)
            f = b->fn;

        return f && f->sig && !qast_generic(f) ? f->result : QY_VOID;
    }

    default:
        return QY_VOID;
    }
}

/* Unchecked e as a generic record's instance, Name[T] before a '.': its type; QY_VOID when it
   is none, QY_ERROR after an error. */
QType qsem_typeat(QSem *const S, const QExpr *const e)
{
    if (e->kind != QE_INDEX || !e->tx || e->a->kind != QE_NAME)
        return QY_VOID;

    const QBind *const b = qsem_find(S, e->a->tok);

    if (!b || !qsem_model(S, b->ty))
        return QY_VOID;

    return qsem_given_types(S, b->ty, e->tx->items, e->tx->n, e->a->tok);
}

/* T.f(..): T's method f without self, else an enum's value with data. */
static QExpr *qsem_typecall(QSem *const S, QExpr *const e, const QType t)
{
    QFn *const f = qsem_methodof(S, t, e->tok);
    const QTy *const y = qast_ty(S->prog, t);
    e->type = QY_ERROR;

    if (!f && y->kind == QT_REC) {
        qsem_say(S, e->tok, qsem_aname(S, t), " has no method ", qsem_text(S, e->tok), nullptr);
        return e;
    }

    if (!f)
        return qsem_variant(S, e, t, true);
    if (!qsem_named(S, f, e->tok) || qsem_private(S, t, e->tok, f->priv, e->tok))
        return e;

    if (f->self) {
        qsem_say(
            S,
            e->tok,
            qsem_text(S, e->tok),
            " takes self: call it on ",
            qsem_aname(S, t),
            nullptr
        );
        return e;
    }

    return qsem_fncall(S, e, f);
}

/* a.f(..), a checked as r: a method of r's type with self, else a record's fn value called. */
static QExpr *qsem_valuecall(QSem *const S, QExpr *const e, QExpr *const r)
{
    e->type = QY_ERROR;

    if (r->type == QY_ERROR)
        return e;

    QFn *const f = qsem_methodof(S, r->type, e->tok);

    if (f && (!qsem_named(S, f, e->tok) || qsem_private(S, r->type, e->tok, f->priv, e->tok)))
        return e;

    if (f && !f->self) {
        const char *const ty = qsem_text(S, qast_ty(S->prog, r->type)->name);
        qsem_say(
            S,
            e->tok,
            qsem_text(S, e->tok),
            " is called on its type: ",
            ty,
            ".",
            qsem_text(S, e->tok),
            "(..)",
            nullptr
        );
        return e;
    }

    if (f) {
        QArg *const self = qast_alloc(S->mem, sizeof *self);
        r->done = true;
        self->e = r;
        self->next = e->args;
        e->args = self;
        e->a = nullptr;
        return qsem_fncall(S, e, f);
    }

    if (qast_tag(S->prog, r->type) && qsem_is(e->tok, "text")) { /* a plain enum's name */
        if (!e->args)
            return qsem_spell(S, r, "text()");

        qsem_say(S, e->tok, "text() takes no values", nullptr);
        return e;
    }

    const QTy *const y = qsem_isrec(S, r->type) ? qast_ty(S->prog, r->type) : nullptr;

    for (uint32_t k = 0; y && k < y->nfields; k++) /* a record's fn value, called */
        if (qsem_same(y->fields[k].name, e->tok) && qast_isfn(S->prog, y->fields[k].type)) {
            if (qsem_private(S, r->type, e->tok, y->fields[k].priv, e->tok))
                return e;

            QExpr *const fv = qast_expr(S->mem, QE_FIELD, e->tok);
            fv->a = qsem_addressed(S, r);
            fv->field = k;
            fv->type = y->fields[k].type;
            return qsem_icall(S, e, fv);
        }

    qsem_say(S, e->tok, qsem_aname(S, r->type), " has no method ", qsem_text(S, e->tok), nullptr);
    return e;
}

/* A built-in method: its name, what has it, its check; none: qsem_listop's of op. */
static const struct {
    const char *name;
    uint8_t on;
    QBuiltin op;
    QExpr *(*check)(QSem *S, QExpr *e);
} qsem_builtins[] = {
    {.name = "push", .on = QM_LIST, .op = QB_PUSH},
    {.name = "pop", .on = QM_LIST, .op = QB_POP},
    {.name = "insert", .on = QM_LIST, .op = QB_INSERT},
    {.name = "remove", .on = QM_LIST | QM_DICT, .op = QB_REMOVE},
    {.name = "resize", .on = QM_DYN, .op = QB_RESIZE},
    {.name = "shape", .on = QM_LIST | QM_ARRAY | QM_DYN, .check = qsem_shape},
    {.name = "swap", .on = QM_LIST | QM_ARRAY | QM_DYN, .check = qsem_swap},
    {.name = "sort", .on = QM_LIST | QM_ARRAY | QM_DYN, .check = qsem_sort},
    {.name = "keys", .on = QM_DICT, .check = qsem_keys},
    {.name = "get", .on = QM_DICT, .check = qsem_get},
};

void qsem_methods(const uint8_t on, QsemMethodFn *const fn, void *const ctx)
{
    for (uint32_t i = 0; i < countof(qsem_builtins); i++)
        if (qsem_builtins[i].on & on)
            fn(ctx, qsem_builtins[i].name);

    qsem_chain_methods(fn, ctx);
}

/* a.f(b): f of module a; a method of a's type; an enum's value with data; a list's, a dict's
   or a chain's method. */
QExpr *qsem_method(QSem *const S, QExpr *const e)
{
    if (qsem_ismod(S, e->a)) { /* m.f(..): f of module m */
        QExpr name = {.kind = QE_NAME, .tok = e->tok, .a = e->a};

        return qsem_called(S, e, qsem_qual(S, &name));
    }

    const QType at = qsem_typeat(S, e->a);

    if (at) { /* Name[T].f(..) */
        e->type = QY_ERROR;
        return at == QY_ERROR ? e : qsem_typecall(S, e, at);
    }

    if (e->a->kind == QE_NAME || e->a->kind == QE_FIELD) {
        const QBind *const b = qsem_qual(S, e->a);

        if (b && b->ty == QY_ERROR) {
            e->type = QY_ERROR;
            return e;
        }

        if (b && b->ty)
            return qsem_typecall(S, e, b->ty);
        if (b && b->var && qsem_isgen(S, b->var->type) && qsem_is(e->tok, "next"))
            return qsem_next(S, e, b->var);
    }

    if (qsem_owntype(S, qsem_peek(S, e->a)))
        return qsem_valuecall(S, e, qsem_expr(S, e->a, QY_VOID));

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

    for (uint32_t i = 0; i < countof(qsem_builtins); i++)
        if (qsem_is(e->tok, qsem_builtins[i].name))
            return qsem_builtins[i].check ? qsem_builtins[i].check(S, e)
                                          : qsem_listop(S, e, qsem_builtins[i].op);

    if (qsem_is(e->tok, "len")) {
        qsem_say(S, e->tok, "the length is len(x), not x.len()", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    return qsem_valuecall(S, e, qsem_expr(S, e->a, QY_VOID));
}

/* x in a, a list: a's element type. */
QExpr *qsem_has(QSem *const S, QExpr *const e)
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
            "in takes a range a..b, a list or a dict, not ",
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
