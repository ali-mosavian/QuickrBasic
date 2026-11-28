/* Quickr's semantic pass: calls of fns, fn values and lambdas, and records' operators.
 *
 * # How it works
 * A name called is what it binds: a fn, a fn value, a record's type, else a built-in. A call
 * of a parameter bound to a one-line lambda, or of an operator or method of one return, is
 * inlined.
 */
#include "qr/qsem_int.h"
#include "qr/qpar.h"

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
        if (a->name) {
            qsem_say(S, a->name, "a function value takes plain values", nullptr);
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
bool qsem_cheap(const QExpr *const v)
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

/* Operator or method fn f's call e of argv, inlined: f's one return's value checked again here,
   each parameter read where its argument is when cheap, else stored first. Its own module's names
   in sight, not the caller's; its uses and moves at e. */
QExpr *qsem_inline(QSem *const S, QExpr *const e, QFn *const f, QExpr **const argv)
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

    qsem_typeargs(S, f);
    const QFn *const outer = S->body;
    const uint32_t site = S->site;
    S->site = outer ? site : qsem_at(S, e->tok); /* a call in an inlined body: the outer call's */
    S->body = f;
    f->inlining = true;
    QExpr *const body = qpar_reexpr(S->prog, S->mem, f->lone);
    QExpr *const v = body ? qsem_expr(S, body, f->result) : nullptr;
    f->inlining = false;
    S->body = outer;
    S->site = site;
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

/* Can checked e be a value of type t: of it, widening to it, or an untyped constant fitting it? */
static bool qsem_takes(const QExpr *const e, const QType t)
{
    return e->type == t || qsem_widens(e->type, t) || (e->untyped && qsem_fits(e, t));
}

/* Operator op of a, or of a and b, from their records' operators: the one taking their types,
   else the one they widen to; null none. *two: two of them widen. */
QFn *qsem_operfor(
    const QSem *const S,
    const QKind op,
    const QExpr *const a,
    const QExpr *const b,
    bool *const two
)
{
    const QExpr *const sides[] = {a, b};

    QFn *wide = nullptr;
    *two = false;

    for (uint32_t i = 0; i < 2 && sides[i]; i++) {
        const QTy *const y =
            qsem_isrec(S, sides[i]->type) ? qast_ty(S->prog, sides[i]->type) : nullptr;

        for (uint32_t k = 0; y && k < y->nopers; k++) {
            QFn *const f = y->opers[k];

            if (f->op != op || f->npar != (b ? 2u : 1u))
                continue;

            const QType p = f->params[0].var->type, q = b ? f->params[1].var->type : QY_VOID;

            if (a->type == p && (!b || b->type == q))
                return f;

            if (qsem_takes(a, p) && (!b || qsem_takes(b, q))) {
                *two |= wide && wide != f;
                wide = f;
            }
        }
    }

    return wide;
}

/* a op b, or op a when b is null, of a record: its operator taking them, inlined when its body is
   one return, else called. */
QExpr *qsem_operator(QSem *const S, QExpr *const e, const QKind op, QExpr *a, QExpr *b)
{
    bool two;
    QFn *const f = qsem_operfor(S, op, a, b, &two);
    const char *const sym = qast_spelling(op);
    const QExpr *const rec = qsem_isrec(S, a->type) ? a : b;
    e->type = QY_ERROR;

    if (!f || two) {
        if (b)
            qsem_say(
                S,
                e->tok,
                two ? "two operators " : "no operator ",
                sym,
                two ? " take " : " takes ",
                qsem_aname(S, a->type),
                " and ",
                qsem_aname(S, b->type),
                nullptr
            );
        else
            qsem_say(S, e->tok, qsem_aname(S, a->type), " has no operator ", sym, nullptr);

        return e;
    }

    if (f->priv && qsem_modof(S, f->name) != qsem_modof(S, e->tok)) {
        const char *const ty = qsem_text(S, qast_ty(S->prog, rec->type)->name);
        qsem_say(S, e->tok, ty, "'s operator ", sym, " is private", nullptr);
        return e;
    }

    const uint32_t n = b ? 2 : 1;
    QExpr **const argv = qast_alloc(S->mem, n * sizeof *argv);
    argv[0] = qsem_coerce(S, a, f->params[0].var->type);

    if (b)
        argv[1] = qsem_coerce(S, b, f->params[1].var->type);
    if (argv[0]->type == QY_ERROR || (b && argv[1]->type == QY_ERROR))
        return e;
    if (f->lone && !f->inlining) /* a fresh argument built in its parameter's temporary */
        return qsem_inline(S, e, f, argv);

    for (uint32_t k = 0; k < n; k++)
        argv[k] = qsem_addressed(S, argv[k]);

    e->kind = QE_CALL;
    e->fn = f;
    e->argv = argv;
    e->nargv = n;
    e->a = e->b = nullptr;
    e->done = true;
    e->type = f->result;
    qsem_callee(S, e);
    return e;
}

/* Does a op b compare records by a fn operator? */
bool qsem_hasrel(const QSem *const S, const QKind op, const QExpr *const a, const QExpr *const b)
{
    const bool swap = op == QK_GT || op == QK_LE;
    const QKind by = op == QK_EQ || op == QK_NE ? QK_EQ : QK_LT;
    bool two;
    return qsem_operfor(S, by, swap ? b : a, swap ? a : b, &two);
}

/* a op b, a a record: = and < its operators; <> and >= not = and <; > and <= < the other way. */
QExpr *qsem_relation(QSem *const S, QExpr *const e, const QKind op, QExpr *a, QExpr *b)
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
    qsem_callee(S, e);
    return e;
}

/* Call e through fn value f, checked. */
QExpr *qsem_icall(QSem *const S, QExpr *const e, QExpr *const f)
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
    qsem_callee(S, e);
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
QExpr *qsem_fnref(QSem *const S, QExpr *const e, QFn *const f)
{
    e->type = QY_ERROR;
    f->idle = false;

    if (f->ytx) {
        qsem_say(S, e->tok, qsem_text(S, f->name), " is no value: it is a generator", nullptr);
        return e;
    }

    if (f->foreign) {
        qsem_say(S, e->tok, qsem_text(S, f->name), " is no value: it is QBasic's", nullptr);
        return e;
    }

    if (qast_generic(f) || qsem_takesfn(f)) {
        qsem_say(
            S,
            e->tok,
            qsem_text(S, f->name),
            " is no value: it takes types or functions",
            nullptr
        );
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

    S->values = qast_room(S->mem, S->values, S->nvalues, &S->capvalues, sizeof *S->values);
    S->values[S->nvalues++] = f;
    e->kind = QE_FNREF;
    e->fn = f;
    e->type = qsem_fnof(S, items, f->npar, f->result);
    return e;
}

QExpr *qsem_call(QSem *const S, QExpr *const e)
{
    if (e->a)
        return qsem_method(S, e);

    return qsem_called(S, e, qsem_find(S, e->tok));
}

/* Generic record g built by call e without its types: each read from the value given for a
   field of it, typed values first, then literals; its instance, QY_ERROR after an error. */
static QType qsem_inferred(QSem *const S, QExpr *const e, const QType g)
{
    const QTy m = *qast_ty(S->prog, g); /* a copy: a value checked may add types */
    QType *const targs = qast_alloc(S->mem, m.ntp * sizeof *targs);
    bool *const bylit = qast_alloc(S->mem, m.ntp * sizeof *bylit);

    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t k = 0;

        for (QArg *a = e->args; a; a = a->next, k++) {
            const QTypeEx *x = nullptr;
            uint32_t at = 0;

            for (const QStmt *f = m.def->next; f && f->kind != QS_ENDTYPE; f = f->next) {
                for (const QDecl *d = f->kind == QS_FIELD ? f->decls : nullptr; d;

                     d = d->next, at++)
                    if (a->name ? qsem_same(d->name, a->name) : at == k)
                        x = d->type;

                f = f->kind == QS_FN && f->fn->end ? f->fn->end : f;
            }

            if (!pass) {
                a->e = qsem_expr(S, a->e, QY_VOID);
                a->e->done = true;
            }

            if (!x || a->e->untyped != (pass == 1) || a->e->type == QY_ERROR)
                continue;

            const bool fits = pass ? qsem_literal(S, m.tparams, m.ntp, x, a->e, targs, bylit)
                                   : qsem_unify(S, m.tparams, m.ntp, x, a->e->type, targs);

            if (!fits) {
                qsem_say(S, a->e->tok, qsem_text(S, m.name), "'s types don't agree here", nullptr);
                return QY_ERROR;
            }
        }
    }

    for (uint32_t j = 0; j < m.ntp; j++)
        if (!targs[j]) {
            const char *types = "";

            for (uint32_t i = 0; i < m.ntp; i++)
                types = qsem_join(S, types, i ? ", " : "", qsem_text(S, m.tparams[i]));

            qsem_say(
                S,
                e->tok,
                "can't tell ",
                qsem_text(S, m.tparams[j]),
                " from the values given: write ",
                qsem_join(S, qsem_text(S, m.name), "[", types),
                "](..)",
                nullptr
            );
            return QY_ERROR;
        }

    return qsem_typeinst(S, g, targs, e->tok);
}

/* Call e of what its name binds, b: a fn, a fn value, a record's type; none: a built-in. */
QExpr *qsem_called(QSem *const S, QExpr *const e, const QBind *const b)
{
    if (b && b->ty == QY_ERROR) {
        e->type = QY_ERROR;
        return e;
    }

    if (b && qsem_model(S, b->ty)) { /* Name[T](..), Name(..): an instance's */
        QTypeEx *const x = e->tx;
        e->tx = nullptr;
        const QType t =
            x ? qsem_given_types(S, b->ty, x->items, x->n, e->tok) : qsem_inferred(S, e, b->ty);

        if (t == QY_ERROR) {
            e->type = QY_ERROR;
            return e;
        }

        return qsem_called(S, e, &(QBind){.name = b->name, .ty = t});
    }

    if (e->tx) {
        qsem_say(
            S,
            e->tok,
            qsem_text(S, e->tok),
            " is not a generic type: it takes no types",
            nullptr
        );
        e->type = QY_ERROR;
        return e;
    }

    if (b && b->fn)
        return qsem_fncall(S, e, b->fn);
    if (b && b->var && qast_isfn(S->prog, b->var->type))
        return qsem_fnvar(S, e, b->var);

    if (b && b->ty && qsem_isrec(S, b->ty)) {
        const QTy *const y = qast_ty(S->prog, b->ty);
        bool priv = false;

        for (uint32_t k = 0; k < y->nfields; k++)
            priv |= y->fields[k].priv;

        if (priv && qsem_modof(S, y->def->tok) != qsem_modof(S, e->tok)) {
            const char *const ty = qsem_text(S, y->name);
            qsem_say(S, e->tok, ty, " has a private field: only its module builds one", nullptr);
            e->type = QY_ERROR;
            return e;
        }

        return qsem_build(S, e, b->ty, y->fields, y->nfields, y->name, false);
    }

    if (b && b->ty) {
        qsem_say(S, e->tok, "an enum's value is Name.value", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (b || e->a) {
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is not a function", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    if (e->tok->kind == QK_PRINT)
        return qsem_print(S, e);

    const QType to = qsem_scalar(e->tok);

    if (to != QY_VOID && to != QY_BOOL)
        return qsem_convert(S, e, to);

    if (qsem_is(e->tok, "len"))
        return qsem_len(S, e);
    if (qsem_is(e->tok, "copy"))
        return qsem_copy(S, e);
    if (qsem_is(e->tok, "wrap"))
        return qsem_wrapped_ops(S, e);

    qsem_say(S, e->tok, "unknown function ", qsem_text(S, e->tok), nullptr);
    e->type = QY_ERROR;
    return e;
}

QExpr *qsem_name(QSem *const S, QExpr *const e)
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
    const uint32_t at = qsem_useat(S, e->tok);
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
