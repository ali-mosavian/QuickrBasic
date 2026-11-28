/* Quickr's semantic pass: generics and lambdas: a call's instance.
 *
 * # How it works
 * A call of a generic, or of a fn taking fns, reads its types from its arguments and checks its
 * lambdas at their site; its instance of both is found or made, once each, and checked again.
 */
#include "qr/qsem_int.h"
#include "qr/qpar.h"

/* A call of fn f: its arguments by parameter, defaults where left out. */

/* ---- generics */

/* Generic record instance y's type parameters bound to its types, in the scope open. */
void qsem_bindtypes(QSem *const S, const QTy *const y)
{
    for (uint32_t j = 0; y->of && j < y->ntp; j++)
        qsem_bindty(S, y->tparams[j], y->targs[j]);
}

/* Instance f's type parameters bound to its types, in the scope open; a generic record's
   method's, its type's. */
void qsem_typeargs(QSem *const S, const QFn *const f)
{
    if (f->in && f->in->kind == QS_TYPE)
        qsem_bindtypes(S, qast_ty(S->prog, f->in->ty));

    for (uint32_t j = 0; f->of && j < f->ntp; j++)
        qsem_bindty(S, f->tparams[j], f->targs[j]);
}

/* Type as written x matched to type t: each of the ntp type parameters tp it names found, into
   targs; false when one is found twice, two ways. */
bool qsem_unify(
    QSem *const S,
    const QTok *const *const tp,
    const uint32_t ntp,
    const QTypeEx *const x,
    const QType t,
    QType *const targs
)
{
    if (t == QY_ERROR)
        return true;

    if (x->kind == QX_NAME) {
        for (uint32_t j = 0; j < ntp; j++)
            if (qsem_same(tp[j], x->tok)) {
                const bool fits = !targs[j] || targs[j] == t;
                targs[j] = targs[j] ? targs[j] : t;
                return fits;
            }

        return true;
    }

    const QTy *const y = qast_compound(t) ? qast_ty(S->prog, t) : nullptr;

    switch (x->kind) {
    case QX_DYN:
    case QX_LIST:
    case QX_ARRAY:
        return !y || (y->kind != QT_DYN && y->kind != QT_ARRAY) ||
               qsem_unify(S, tp, ntp, x->elem, y->elem, targs);

    case QX_OPT:
    case QX_RES: /* a T for a T? too */
        return qsem_unify(
            S,
            tp,
            ntp,
            x->elem,
            qsem_wraps(S, t, QT_REC) ? qsem_inner(S, t) : t,
            targs
        );

    case QX_GEN: { /* Name[T] for an instance of Name */
        bool fits = true;
        const bool same = y && y->of && qsem_same(qast_ty(S->prog, y->of)->name, x->elem->tok);

        for (uint32_t k = 0; same && k < x->n && k < y->ntp; k++)
            fits &= qsem_unify(S, tp, ntp, x->items[k], y->targs[k], targs);

        return fits;
    }

    case QX_TUPLE:
    case QX_FN: {
        const QTyKind kind = x->kind == QX_TUPLE ? QT_TUPLE : QT_FN;
        bool fits = true;

        for (uint32_t k = 0; y && y->kind == kind && k < x->n && k < y->nfields; k++)
            fits &= qsem_unify(S, tp, ntp, x->items[k], y->fields[k].type, targs);

        if (y && y->kind == QT_FN && x->elem)
            fits &= qsem_unify(S, tp, ntp, x->elem, y->elem, targs);

        return fits;
    }

    default:
        return true;
    }
}

/* Literal a given for type as written x: each of the ntp type parameters tp x names that is still
   open, or was typed by literals only, takes a's type (1 and 2.5 meet in double); false when one a
   typed value bound doesn't take a. */
bool qsem_literal(
    QSem *const S,
    const QTok *const *const tp,
    const uint32_t ntp,
    const QTypeEx *const x,
    const QExpr *const a,
    QType *const targs,
    bool *const bylit
)
{
    QType *const lit = qast_alloc(S->mem, ntp * sizeof *lit);
    qsem_unify(S, tp, ntp, x, a->type, lit);

    for (uint32_t j = 0; j < ntp; j++) {
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
bool qsem_lambda(QSem *const S, QExpr *const e, QType *const t, const bool open)
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
    return qsem_nameof(S, c->var, tok);
}

/* The parameter of the fn checked whose variable v is; null none. */
const QParam *qsem_paramof(const QSem *const S, const QVar *const v)
{
    for (uint32_t k = 0; S->fn && k < S->fn->npar; k++)
        if (S->fn->params[k].var == v)
            return &S->fn->params[k];

    return nullptr;
}

/* Call e's hidden arguments from at, for fn parameter p bound to lambda l given as a: a literal's
   captures as this site sees them; one passed on, the fn checked's own hidden parameters for it. */
void qsem_hiddenargs(
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
        qsem_say(S, g->params[at].name, "a lambda goes to a function's parameter", nullptr);
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

    if (g->in && g->in->kind == QS_TYPE)
        qsem_bindtypes(S, qast_ty(S->prog, g->in->ty));

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
QFn *qsem_instance(QSem *const S, QExpr *const e, QFn *const g)
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
                if (open && !qsem_unify(S, g->tparams, g->ntp, g->params[at].type, t, targs))
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

            const QTypeEx *const x = g->params[at].type;
            const bool fits = pass ? qsem_literal(S, g->tparams, g->ntp, x, a->e, targs, bylit)
                                   : qsem_unify(S, g->tparams, g->ntp, x, a->e->type, targs);

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

    for (QFn *i = g->insts; i && (!g->ytx || kept);

         i = i->sibling) /* an inline generator's: one a for */
        if (i->resumable == kept && qsem_keyed_as(i, targs, binds, g->ntp, g->npar))
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

    if (kept) {
        i->gentype = qsem_newty(S, QT_REC);
        qast_ty(S->prog, i->gentype)->name = g->name;
        qast_ty(S->prog, i->gentype)->gen = i;
    }

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
    i->nown = i->npar;
    return i->sig ? i : nullptr;
}

/* Does f take a fn: a parameter of a fn type, as written? */
bool qsem_takesfn(const QFn *const f)
{
    for (uint32_t k = 0; k < f->npar; k++)
        if (f->params[k].type && f->params[k].type->kind == QX_FN)
            return true;

    return false;
}

/* Is a call of f inlined: a method whose body is one return, its parameters plain values? */
static bool qsem_inlines(const QFn *const f)
{
    bool plain = f->in && f->lone && !f->inlining && !f->native && !f->ytx && !f->nhidden;

    for (uint32_t k = 0; plain && k < f->npar; k++)
        plain = !f->params[k].mut && !f->params[k].lam;

    return plain;
}

/* Is unchecked e a var's place: a var's, a mut parameter's, a for mut's element's? */
static bool qsem_varplace(QSem *const S, const QExpr *const e)
{
    const QVar *const v = qsem_rawroot(S, e);
    return v && (v->kind == QV_VAR || v->kind == QV_MUT || (v->kind == QV_EACH && v->mut));
}

/* Var place v for QBasic procedure f's by-reference parameter p: of its type, so the var sees the
   change; a T[]'s any array of its elements, by its descriptor. */
static QExpr *qsem_refarg(QSem *const S, QExpr *const v, const QParam *const p)
{
    if (v->type != p->var->type && v->type != QY_ERROR &&
        !qsem_lendable(S, v->type, p->var->type)) {
        qsem_say(
            S,
            v->tok,
            qsem_text(S, p->name),
            " is by reference: pass a var of its type, ",
            qsem_aname(S, p->var->type),
            nullptr
        );
        v->type = QY_ERROR;
    }

    return v;
}

/* Checked argument v for a QBasic procedure, no var's by reference: a copy the call may change, a
   place's record its own temporary's, a number or a string by value. */
static QExpr *qsem_copyarg(QSem *const S, QExpr *const v)
{
    if (v->type == QY_ERROR || qast_fresh(v))
        return v;

    if (!qsem_byplace(S, v->type)) {
        v->byval = true;
        return v;
    }

    QExpr *const c = qast_expr(S->mem, QE_CALL, v->tok);
    c->builtin = QB_COPY;
    c->a = v;
    c->type = v->type;
    return qsem_held(S, c);
}

QExpr *qsem_fncall(QSem *const S, QExpr *const e, QFn *f)
{
    f->idle = false;

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

        const bool ref = p->ref && qsem_varplace(S, a->e);
        const bool lend = (p->mut || ref) && qsem_changes(S, a->e, a->e->tok);
        QExpr *const v = qsem_expr(S, a->e, p->var->type);

        if ((p->mut || ref) && !lend)
            v->type = QY_ERROR;

        const bool lent = !qast_fresh(v) && qsem_lendable(S, v->type, p->var->type);

        e->argv[k] = ref      ? qsem_refarg(S, v, p)
                     : p->mut ? qsem_mutarg(S, v, p)
                     : lent   ? qsem_addressed(S, v)
                              : qsem_addressed(S, qsem_coerce(S, v, p->var->type));

        if (f->foreign && !ref)
            e->argv[k] = qsem_copyarg(S, e->argv[k]);
    }

    if (qsem_inlines(f)) {
        bool ok = true;

        for (uint32_t k = 0; k < f->npar; k++) /* an array lent by its descriptor: called */
            ok &= e->argv[k]->type != QY_ERROR &&
                  !qsem_lendable(S, e->argv[k]->type, f->params[k].var->type);

        if (ok)
            return qsem_inline(S, e, f, e->argv);
    }

    qsem_lent(S, e, f);
    qsem_callee(S, e);
    return e;
}
