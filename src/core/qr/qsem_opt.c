/* Quickr's semantic pass: optionals and errors: none, T?, T!, ?? and ?.
 *
 * # How it works
 * A test that proves x is not none narrows it (qsem_stmt.c); an enum's name and a value's text
 * are its own methods.
 */
#include "qr/qsem_int.h"

/* ---- optionals and errors */

/* Field k of T? or T! place a, read where it is: a fresh one is settled by its first read. */
QExpr *qsem_part(QSem *const S, QExpr *const a, const uint32_t k)
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
QExpr *qsem_error(QSem *const S, QExpr *const e, const QType hint)
{
    e->type = QY_ERROR;

    if (!qsem_wraps(S, hint, QT_RES)) {
        qsem_say(
            S,
            e->tok,
            "Error(code, message) needs a T or Error place, as a function's result",
            nullptr
        );
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
QExpr *qsem_isnone(QSem *const S, QExpr *const e)
{
    QExpr *const a = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    const bool none = e->op == QK_NONE;
    e->type = QY_ERROR;

    if (a->type == QY_ERROR)
        return e;

    if (!qsem_wraps(S, a->type, none ? QT_OPT : QT_RES)) {
        const char *const say =
            none ? "is none takes a T or none, not " : "is Error takes a T or Error, not ";
        qsem_say(S, e->tok, say, qsem_aname(S, a->type), nullptr);
        return e;
    }

    e->type = QY_BOOL;
    return e;
}

/* a or else b: a's T, or b when a is none or an Error. */
QExpr *qsem_coalesce(QSem *const S, QExpr *const e)
{
    QExpr *const a = qsem_held(S, qsem_expr(S, e->a, QY_VOID));
    e->type = QY_ERROR;

    if (a->type == QY_ERROR) {
        qsem_expr(S, e->b, QY_VOID);
        return e;
    }

    if (!qsem_wraps(S, a->type, QT_REC)) {
        qsem_say(
            S,
            e->tok,
            "or else takes a T or none or a T or Error, not ",
            qsem_aname(S, a->type),
            nullptr
        );
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

/* Is f a method text(self) as string? */
bool qsem_istext(const QFn *const f)
{
    return f->in && qsem_is(f->name, "text") && f->self && f->npar == 1 && !f->params[0].mut &&
           f->result == QY_STRING && !f->ntp && !f->ytx;
}

/* Plain enum value v's name: each value's compared in turn, v held first unless a name. */
static QExpr *qsem_enumname(QSem *const S, QExpr *const v)
{
    const QTy *const y = qast_ty(S->prog, v->type);
    QExpr *apply = nullptr, *at = v;

    if (qsem_isconst(v)) {
        const uint32_t k = v->val.i >= 0 && v->val.i < y->nvars ? (uint32_t)v->val.i : 0;
        QPiece *const s = qast_alloc(S->mem, sizeof *s);
        s->lit = y->vars[k].name;
        return qsem_const(S, v->tok, QY_STRING, (QVal){.s = s});
    }

    if (v->kind != QE_NAME) {
        apply = qast_expr(S->mem, QE_APPLY, v->tok);
        apply->argv = qast_alloc(S->mem, sizeof *apply->argv);
        apply->temps = qast_alloc(S->mem, sizeof *apply->temps);
        apply->argv[0] = v;
        apply->temps[0] = qsem_var(S, v->tok, v->type, QV_TEMP);
        apply->nargv = 1;
        at = qsem_nameof(S, apply->temps[0], v->tok);
    }

    QExpr *r = nullptr;

    for (uint32_t k = y->nvars; k-- > 0;) {
        QPiece *const s = qast_alloc(S->mem, sizeof *s);
        s->lit = y->vars[k].name;
        QExpr *const name = qsem_const(S, v->tok, QY_STRING, (QVal){.s = s});

        if (!r) {
            r = name;
            continue;
        }

        QExpr *const eq = qast_expr(S->mem, QE_CHAIN, v->tok);
        eq->n = 2;
        eq->items = qast_alloc(S->mem, 2 * sizeof *eq->items);
        eq->ops = qast_alloc(S->mem, sizeof *eq->ops);
        eq->items[0] = qsem_dup(S, at);
        eq->items[1] = qsem_const(S, v->tok, v->type, (QVal){.i = k});

        eq->ops[0] = QK_EQ;

        QExpr *const c = qast_expr(S->mem, QE_COND, v->tok);
        c->c = qsem_expr(S, eq, QY_VOID);
        c->a = name;
        c->b = r;
        c->type = QY_STRING;
        r = c;
    }

    if (!apply)
        return r;

    apply->a = r;
    apply->type = QY_STRING;
    return apply;
}

/* Checked v as print and an f-string spell it: a number, a string or a bool itself; a value of a
   type with text() that call; a plain enum's name; a T or none's T so spelled, or none. Null when
   it has no spelling. */
static QExpr *qsem_spelled(QSem *const S, QExpr *const v)
{
    if (!qast_compound(v->type))
        return v;

    const QTy *const y = qast_ty(S->prog, v->type);

    for (uint32_t k = 0; k < y->nmeths; k++)
        if (qsem_istext(y->meths[k])) {
            QExpr *const c = qast_expr(S->mem, QE_CALL, v->tok);
            QArg *const self = qast_alloc(S->mem, sizeof *self);
            v->done = true;
            self->e = v;
            c->args = self;
            return qsem_fncall(S, c, y->meths[k]);
        }

    if (qast_tag(S->prog, v->type))
        return qsem_enumname(S, v);
    if (!qsem_wraps(S, v->type, QT_OPT))
        return nullptr;

    QExpr *const a = qsem_held(S, v);
    QExpr *const flag = qsem_part(S, a, QF_FLAG); /* read first: settles a fresh a */
    QExpr *in = qsem_spelled(S, qsem_part(S, qsem_again(S, a), QF_VAL));

    if (!in)
        return nullptr;

    if (in->type != QY_STRING) {
        QExpr *const s = qast_expr(S->mem, QE_CONV, in->tok);
        s->a = in;
        s->type = QY_STRING;
        in = s;
    }

    QExpr *const none = qast_expr(S->mem, QE_CALL, v->tok);
    none->builtin = QB_NONETEXT;
    none->type = QY_STRING;

    QExpr *const c = qast_expr(S->mem, QE_COND, v->tok);
    c->c = flag;
    c->a = in;
    c->b = none;
    c->type = QY_STRING;
    return c;
}

/* v spelled for who, print or an f-string; an error when it has no spelling. */
QExpr *qsem_spell(QSem *const S, QExpr *const v, const char *const who)
{
    if (v->type == QY_ERROR)
        return v;

    QExpr *const s = qsem_spelled(S, v);

    if (s)
        return s;

    if (qsem_owntype(S, v->type))
        qsem_say(
            S,
            v->tok,
            qsem_aname(S, v->type),
            " has no text(): ",
            who,
            " can't spell it",
            nullptr
        );
    else
        qsem_say(S, v->tok, who, " can't spell ", qsem_aname(S, v->type), nullptr);

    v->type = QY_ERROR;
    return v;
}

QExpr *qsem_try(QSem *const S, QExpr *const e)
{
    QExpr *const a = e->a = qsem_held(S, qsem_expr(S, e->a, QY_VOID));
    e->type = QY_ERROR;

    if (a->type == QY_ERROR)
        return e;

    if (!qsem_wraps(S, a->type, QT_RES)) {
        qsem_say(S, e->tok, "x? takes a T or Error, not ", qsem_aname(S, a->type), nullptr);
        return e;
    }

    if (!S->fn || !qsem_wraps(S, S->fn->result, QT_RES)) {
        qsem_say(
            S,
            e->tok,
            "x? returns its error: only in a function whose result is a T or Error",
            nullptr
        );
        return e;
    }

    e->fn = S->fn;
    e->type = qsem_inner(S, a->type);
    return e;
}

/* m.get(k): its value as a V?, none when k is not there; V no record or list. */
QExpr *qsem_get(QSem *const S, QExpr *const e)
{
    QExpr *const m = e->a = qsem_addressed(S, qsem_expr(S, e->a, QY_VOID));
    e->type = QY_ERROR;

    if (m->type == QY_ERROR)
        return e;

    if (!qsem_ismap(S, m->type) || !qsem_plain(S, e, 1)) {
        const bool map = qsem_ismap(S, m->type);
        qsem_say(S, e->tok, map ? "get() takes a key" : "only a dict has get()", nullptr);
        return e;
    }

    const QTy *const y = qast_ty(S->prog, m->type);

    if (qsem_wraps(S, y->elem, QT_RES)) {
        qsem_say(S, e->tok, "get() of a dict of T or Error: test k in d, then read d[k]", nullptr);
        return e;
    }

    e->builtin = QB_GET;
    e->b = qsem_coerce(S, qsem_expr(S, e->args->e, y->key), y->key);
    e->temps = qast_alloc(S->mem, sizeof *e->temps);
    e->temps[0] = qsem_var(S, e->tok, QY_INT, QV_TEMP);
    e->type = e->b->type == QY_ERROR           ? QY_ERROR
              : qsem_wraps(S, y->elem, QT_OPT) ? y->elem /* none too when its value is */
                                               : qsem_wrapof(S, QT_OPT, y->elem);
    return e;
}
