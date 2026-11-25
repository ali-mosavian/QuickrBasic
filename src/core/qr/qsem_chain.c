/* Quickr's semantic pass: chains: a sequence's steps and its end.
 *
 * # How it works
 * xs.map(..).filter(..).sum() is one loop: its source, each step on the element, then its end
 * (sum, min, collect, ...), no list between.
 */
#include "qr/qsem_int.h"

enum { QI_REV = QI_ZIP + 1 }; /* rev(): the source counted down */

/* A chain's steps' and ends' names, by QI_* and QZ_*. */
static const char *const qsem_steps[] = {
    [QI_MAP] = "map",
    [QI_FILTER] = "filter",
    [QI_TAKE] = "take",
    [QI_SKIP] = "skip",
    [QI_ZIP] = "zip",
    [QI_REV] = "rev",
};

const char *const qsem_ends[] = {
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

void qsem_chain_methods(QsemMethodFn *const fn, void *const ctx)
{
    for (uint32_t i = 0; i < countof(qsem_steps); i++)
        fn(ctx, qsem_steps[i]);

    for (uint32_t i = 0; i < QZ_FOR; i++)
        fn(ctx, qsem_ends[i]);
}

/* ---- chains */

/* Is e, unchecked, a.name(..) for one of n names, a not a record or an enum: *k which. */
bool qsem_method_of(
    QSem *const S,
    const QExpr *const e,
    const char *const *const names,
    const uint32_t n,
    uint32_t *const k
)
{
    if (e->kind != QE_CALL || !e->a || e->done || qsem_ismod(S, e->a))
        return false;
    if (qsem_owntype(S, qsem_peek(S, e->a)))
        return false;

    for (uint32_t i = 0; i < n; i++)
        if (qsem_is(e->tok, names[i])) {
            *k = i;
            return true;
        }

    return false;
}

/* Is e a chain's step, map(f) .. zip(b) or rev(): *k its QI_*. */
bool qsem_step(QSem *const S, const QExpr *const e, uint32_t *const k)
{
    return qsem_method_of(S, e, qsem_steps, countof(qsem_steps), k);
}

/* A checked value read again where it is: a copy of its node. */
QExpr *qsem_dup(QSem *const S, const QExpr *const e)
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
QExpr *qsem_at_index(QSem *const S, QExpr *const a, QVar *const i)
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
            " has no chain: a list, an array, a dict, a string, a range or a generator has",
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
    call->c = qsem_nameof(S, v, it->tok);
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
        it->start = qsem_state(S, c, g);
        qsem_fromgen(S, it, qsem_var(S, it->tok, it->start->type, QV_VAR));
        return it->nparts;
    }

    if (b && b->var && qsem_isgen(S, b->var->type)) {
        if (qsem_changes(S, c, c->tok))
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

/* Block lambda a applied to extra (n of them) then chain it's parts, for a place of type hint (a
   filter's bool, else its as type): its procedure called, its captures as this site sees them. */
static QExpr *qsem_chainblock(
    QSem *const S,
    const QIter *const it,
    QExpr *const a,
    QExpr *const *const extra,
    const uint32_t n,
    const QType hint
)
{
    const uint32_t count = n + it->nparts;
    QType *const items = qast_alloc(S->mem, (count ? count : 1) * sizeof *items);

    for (uint32_t k = 0; k < count; k++)
        items[k] = (k < n ? extra[k] : it->parts[k - n])->type;

    const bool open = hint != QY_BOOL;
    QType t = qsem_fnof(S, items, count, open ? QY_VOID : hint);

    if (open && !a->lam->rtype) {
        qsem_say(S, a->lam->tok, "can't tell the lambda's result: write fn(..) as T", nullptr);
        return qsem_const(S, a->tok, QY_ERROR, (QVal){});
    }

    if (!qsem_lambda(S, a, &t, open))
        return qsem_const(S, a->tok, QY_ERROR, (QVal){});

    QFn *const f = a->lam->fn;
    QExpr *const call = qast_expr(S->mem, QE_CALL, a->tok);
    call->fn = f;
    call->nargv = f->npar;
    call->argv = qast_alloc(S->mem, (f->npar ? f->npar : 1) * sizeof *call->argv);

    for (uint32_t k = 0; k < count; k++)
        call->argv[k] = qsem_dup(S, k < n ? extra[k] : it->parts[k - n]);

    qsem_hiddenargs(S, call, count, a->lam, a);
    call->type = f->result;
    qsem_callee(S, call);
    return call;
}

/* Lambda or fn a applied to extra (n of them) then chain it's parts, checked for a place of type
   hint: a one-line lambda's body, its parameters naming them; a name's call. */
QExpr *qsem_applied(
    QSem *const S,
    const QIter *const it,
    QExpr *const a,
    QExpr *const *const extra,
    const uint32_t n,
    const QType hint
)
{
    const uint32_t count = n + it->nparts;
    const bool qual = a->kind == QE_FIELD && (a->a->kind == QE_NAME || a->a->kind == QE_FIELD);
    const QBind *const b = qual && !qsem_ismod(S, a->a) ? qsem_qual(S, a->a) : nullptr;
    QFn *const f = b && b->ty ? qsem_methodof(S, b->ty, a->tok) : nullptr;

    if (f && f->self && f->params[0].mut) {
        qsem_say(S, a->tok, qsem_text(S, a->tok), " is no value: it takes mut", nullptr);
        return qsem_const(S, a->tok, QY_ERROR, (QVal){});
    }

    if (a->kind == QE_NAME || (qual && (!b || b->ty))) { /* f, m.f, T.f, a lambda passed in */
        QExpr *const call = qast_expr(S->mem, QE_CALL, a->tok);
        const bool self = f && f->self && count;
        QArg **next = &call->args;
        call->a = qual ? a->a : nullptr;

        if (self) /* T.f(x, ..) is x.f(..) */
            call->a = qsem_dup(S, n ? extra[0] : it->parts[0]);

        for (uint32_t k = self; k < count; k++) {
            QArg *const x = qast_alloc(S->mem, sizeof *x);
            x->e = qsem_dup(S, k < n ? extra[k] : it->parts[k - n]);
            *next = x;
            next = &x->next;
        }

        return qsem_call(S, call);
    }

    if (a->kind != QE_LAMBDA) {
        qsem_say(S, a->tok, "a chain takes a lambda or a function", nullptr);
        return qsem_const(S, a->tok, QY_ERROR, (QVal){});
    }

    if (!a->lam->body)
        return qsem_chainblock(S, it, a, extra, n, hint);

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
    if (k == QI_REV) { /* a take, skip or zip before it: a window on the source, counted down */
        bool filtered = false;

        for (uint32_t j = 0; j < it->n; j++) {
            QStage *const p = &it->stages[j];
            filtered |= p->kind == QI_FILTER;

            if (p->kind == QI_MAP || p->kind == QI_FILTER || p->win)
                continue;

            if (filtered || it->rev) {
                qsem_say(
                    S,
                    c->tok,
                    filtered ? "rev() can't follow a filter's take, skip or zip: collect() first"
                             : "rev() after take, skip or zip counts down once",
                    nullptr
                );
                return;
            }

            p->win = true;
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
            qsem_say(S, c->tok, "fold() takes a first value and a lambda, fn(acc, x)", nullptr);
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
QExpr *qsem_iter(QSem *const S, QExpr *e, const uint32_t k, QStmt *const loop)
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
void qsem_chainfor(QSem *const S, QStmt *const s)
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
