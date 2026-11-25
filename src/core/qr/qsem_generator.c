/* Quickr's semantic pass: fn bodies and generators.
 *
 * # How it works
 * A generator's body yields: its state kept between calls in a record of its own, each next
 * running it on to its next yield.
 */
#include "qr/qsem_int.h"

/* ---- generators */

/* Fn f's body checked, its parameters in its scope; S->fn f after. */
void qsem_fnbody(QSem *const S, QFn *const f)
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
QFn *qsem_genof(QSem *const S, const QExpr *const e)
{
    if (e->kind != QE_CALL || (e->a && !qsem_ismod(S, e->a)))
        return nullptr;

    QExpr name = {.kind = QE_NAME, .tok = e->tok, .a = e->a};

    const QBind *const b = qsem_qual(S, &name);
    return b && b->fn && b->fn->ytx ? b->fn : nullptr;
}

/* yield v: numbered in its instance; a resumable's v its next() result. */
void qsem_yield(QSem *const S, QStmt *const s)
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

        if (i->params[k].hidden || i->params[k].mut) /* a mut's borrowed for the loop */
            v->place = a;
        else if (a->kind == QE_CONST) {
            v->constant = true;
            v->val = a->val;
        } else if (qsem_byplace(S, v->type) && !qast_fresh(a)) {
            v->place = a;
            qsem_lend(S, qsem_root(a), s);
        } else
            e->argv[k] = qsem_into(S, v, qsem_owned(S, a, false));

        v->kind = i->params[k].hidden || i->params[k].mut ? v->kind : QV_LET;
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
void qsem_genfor(QSem *const S, QStmt *const s, QFn *const g)
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
void qsem_kept(QSem *const S, QDecl *const d, const QKind kw, QFn *const g)
{
    if (kw != QK_VAR)
        qsem_say(S, d->name, "a generator changes as it runs: keep it in a var", nullptr);
    if (d->type)
        qsem_say(S, d->type->tok, "a generator's type is its function's: leave out as", nullptr);

    d->value = qsem_state(S, d->value, g);
    d->var = qsem_var(S, d->name, d->value->type, QV_VAR);
    qsem_bind(S, d->name, d->var, nullptr);
}

/* Call e of generator g, kept: its state made, a record of g's resumable instance, each argument
   moved or copied in. */
QExpr *qsem_state(QSem *const S, QExpr *const call, QFn *const g)
{
    S->genkept = true;
    QExpr *const e = qsem_fncall(S, call, g);
    S->genkept = false;
    QFn *const r = e->type == QY_ERROR ? nullptr : e->fn;

    if (!r)
        return e;

    QExpr *const made = qast_expr(S->mem, QE_NEW, e->tok);
    made->type = r->gentype;
    made->nargv = r->nown;
    made->argv = qast_alloc(S->mem, (r->nown ? r->nown : 1) * sizeof *made->argv);

    for (uint32_t k = 0; k < r->nown; k++) {
        const QParam *const p = &r->params[k];

        if (p->mut && !p->hidden && !p->lam)
            qsem_say(S, e->argv[k]->tok, "a kept generator owns its arguments: no mut", nullptr);
        if (!p->hidden && !p->lam)
            made->argv[k] = qsem_owned(S, e->argv[k], false);
    }

    r->keep = e->argv; /* a lambda's captures: its site's places */
    r->news = qast_room(S->mem, r->news, r->nnews, &r->capnews, sizeof *r->news);
    r->news[r->nnews++] = made;
    return made;
}

/* Resumable r, checked: its resume index and each of its variables a parameter by reference
   after its own; its state's fields each parameter's, the states made so far filled. */
void qsem_keep(QSem *const S, QFn *const r)
{
    uint32_t n = r->npar + 1;

    for (const QVar *v = S->prog->vars; v; v = v->next)
        n += qast_home(v->fn) == r && v->kind != QV_PARAM && v->kind != QV_MUT &&
             v->kind != QV_RESULT && !v->constant && !v->place;

    QParam *const params = qast_alloc(S->mem, n * sizeof *params);
    memcpy(params, r->params, r->npar * sizeof *params);
    r->resume = qsem_var(S, r->name, QY_INT, QV_MUT);
    const QVar *const last = S->lastvar;
    params[r->npar++] = (QParam){
        .name = r->name,
        .mut = true,
        .hidden = true,
        .var = r->resume,
    };

    for (QVar *v = S->prog->vars; v != last->next; v = v->next) {
        if (qast_home(v->fn) != r || v->kind == QV_PARAM || v->kind == QV_MUT ||
            v->kind == QV_RESULT || v->constant || v->place)
            continue;

        v->byref = true;
        params[r->npar++] = (QParam){
            .name = v->name,
            .mut = true,
            .hidden = true,
            .var = v,
        };
    }

    r->nhidden += r->npar - r->nown;
    r->params = params;
    QTy *const y = qast_ty(S->prog, r->gentype);
    y->fields = qast_alloc(S->mem, r->npar * sizeof *y->fields);
    y->nfields = y->capfields = r->npar;

    for (uint32_t k = 0; k < r->npar; k++) {
        const QParam *const p = &params[k];
        const bool cell = p->lam || (k < r->nown && p->hidden); /* none of its own: an int */
        y->fields[k] = (QField){.name = p->name, .type = cell ? QY_INT : p->var->type};

        y->owns |= !cell && qsem_owns(S, p->var->type);
    }

    for (uint32_t i = 0; i < r->nnews; i++) {
        QExpr *const made = r->news[i];
        QExpr **const argv = qast_alloc(S->mem, r->npar * sizeof *argv);
        memcpy(argv, made->argv, made->nargv * sizeof *argv);
        made->argv = argv;
        made->nargv = r->npar;
    }
}

/* it.next(), it e->a a generator's state in variable v: its instance called, the state's fields
   by reference (e->c the state); what it yields next, none when done. */
QExpr *qsem_next(QSem *const S, QExpr *const e, QVar *const v)
{
    QFn *const r = qast_compound(v->type) ? qast_ty(S->prog, v->type)->gen : nullptr;
    e->type = QY_ERROR;

    if (!qsem_is(e->tok, "next")) {
        qsem_say(S, e->tok, "a generator has next() alone", nullptr);
        return e;
    }

    if (e->args)
        qsem_say(S, e->args->e->tok, "next() takes no values", nullptr);
    if (!r || !qsem_changes(S, e->a, e->tok))
        return e;
    if (r->nhidden > r->npar - r->nown && S->fn != r->host) {
        qsem_say(
            S,
            e->tok,
            "its lambda borrows from where it was made: call next() there",
            nullptr
        );
        return e;
    }

    e->c = qsem_expr(S, e->a, QY_VOID);
    e->a = nullptr;
    e->fn = r;
    e->type = r->result;
    return e;
}
