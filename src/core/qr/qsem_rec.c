/* Quickr's semantic pass: records, enums and tuples: built, read by field, indexed, sliced.
 *
 * # How it works
 * A record's private fields are its methods' alone; a generic record built without its types
 * has them read from its fields' values (qsem_call.c).
 */
#include "qr/qsem_int.h"

/* ---- records, enums, tuples */

/* e, a QE_CALL with args or a name alone, made a QE_NEW of type t: its n fields' values by
   position or by name, a default or zero where left out; all: each needed. */
QExpr *qsem_build(
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
QExpr *qsem_variant(QSem *const S, QExpr *const e, const QType t, const bool call)
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

bool qsem_isrec(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->kind == QT_REC;
}

/* Is type t's member name, private when priv, out of sight at at, outside t's module: said. */
bool qsem_private(
    QSem *const S,
    const QType t,
    const QTok *const name,
    const bool priv,
    const QTok *const at
)
{
    const QTy *const y = qast_ty(S->prog, t);

    if (!priv || qsem_modof(S, y->def->tok) == qsem_modof(S, at))
        return false;

    qsem_say(
        S,
        at,
        qsem_text(S, name),
        " is private to ",
        qsem_text(S, y->name),
        "'s module",
        nullptr
    );
    return true;
}

/* a.f: a record's field; Name.v, an enum's value. */
QExpr *qsem_field(QSem *const S, QExpr *const e)
{
    if (qsem_ismod(S, e->a)) { /* m.x: x of module m, a name */
        e->kind = QE_NAME;
        return qsem_name(S, e);
    }

    const QType at = qsem_typeat(S, e->a);
    const QBind tb = {.name = e->a->tok, .ty = at};

    if (at || e->a->kind == QE_NAME || e->a->kind == QE_FIELD) {
        const QBind *const b = at ? &tb : qsem_qual(S, e->a);

        if (b && b->ty == QY_ERROR) {
            e->type = QY_ERROR;
            return e;
        }

        QFn *const f = b && b->ty ? qsem_methodof(S, b->ty, e->tok) : nullptr;

        if (f && (!qsem_named(S, f, e->tok) || qsem_private(S, b->ty, e->tok, f->priv, e->tok))) {
            e->type = QY_ERROR;
            return e;
        }

        if (f) /* Type.f: a function value */
            return qsem_fnref(S, e, f);
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
            if (qsem_private(S, a->type, e->tok, y->fields[k].priv, e->tok))
                return e;

            e->field = k;
            e->type = y->fields[k].type;
            return e;
        }

    if (qsem_methodof(S, a->type, e->tok))
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is a method: call it, with (..)", nullptr);
    else
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
QExpr *qsem_slice(QSem *const S, QExpr *const e)
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
        (!qast_compound(a->type) || qast_ty(S->prog, a->type)->kind != QT_DYN)) {
        qsem_say(
            S,
            e->tok,
            "a slice takes a string, a list or a T[], not ",
            qsem_aname(S, a->type),
            nullptr
        );
        return e;
    }

    e->type = a->type; /* a new string, a new list or array */
    return e;
}

/* String constant s's byte i, its escapes read; -1 past its end. */
static int32_t qsem_byteof(const QPiece *s, int64_t i)
{
    for (; s; s = s->next) {
        const QTok *const t = s->lit;

        for (uint32_t k = 0; k < t->len; k++, i--) {
            char c = t->text[k];

            if (c == '\\') {
                c = t->text[++k];
                c = c == 'n' ? '\n' : c == 't' ? '\t' : c;
            }

            if (!i)
                return (uint8_t)c;
        }
    }

    return -1;
}

/* T[n]: a new array of n elements of elem, each empty. */
static QExpr *qsem_newarray(QSem *const S, QExpr *const e, const QType elem)
{
    QExpr *const n = qsem_coerce(S, qsem_expr(S, e->b, QY_INT), QY_INT);
    e->kind = QE_CALL;
    e->builtin = QB_ARRAY;
    e->argv = qast_alloc(S->mem, sizeof *e->argv);
    e->argv[0] = n;
    e->nargv = 1;
    e->type = n->type == QY_ERROR ? QY_ERROR : qsem_arrayof(S, elem, 0, false);

    if (qsem_isconst(n) && n->val.i < 0)
        qsem_say(S, n->tok, "an array's size is 0 or more", nullptr);

    return e;
}

QExpr *qsem_index(QSem *const S, QExpr *const e)
{
    const QType elem = e->b ? qsem_typevalue(S, e->a) : QY_VOID;

    if (elem != QY_VOID && elem != QY_ERROR)
        return qsem_newarray(S, e, elem);

    const QBind *const b = e->a->kind == QE_NAME ? qsem_find(S, e->a->tok) : nullptr;

    if ((e->tx && !e->b) || (b && qsem_model(S, b->ty))) { /* Name[T] alone: a type */
        qsem_say(S, e->tok, "a type is no value: build one with (..)", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    const QExpr *const a = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));

    if (a->type == QY_STRING) { /* a byte, read only */
        e->b = qsem_coerce(S, qsem_expr(S, e->b, QY_INT), QY_INT);

        if (a->kind == QE_CONST && e->b->kind == QE_CONST) { /* a character constant */
            const int32_t c = e->b->val.i < 0 ? -1 : qsem_byteof(a->val.s, e->b->val.i);

            if (c < 0)
                qsem_say(S, e->b->tok, "the index is past the elements", nullptr);

            return qsem_const(S, e->tok, c < 0 ? QY_ERROR : QY_UBYTE, (QVal){.i = c});
        }

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
QExpr *qsem_tuple(QSem *const S, QExpr *const e, const QType hint)
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
