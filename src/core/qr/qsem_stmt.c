/* Quickr's semantic pass: statements.
 *
 * # How it works
 * qsem_stmt checks a statement by its kind: a declaration, an assignment, a loop, a return, an
 * if, whose test may narrow an optional to the end of its scope.
 */
#include "qr/qsem_int.h"

static void qsem_replaced(QSem *S, QExpr *e);
static void qsem_compound(QSem *S, QStmt *s, QExpr *t);

/* ---- statements */

/* The frame checked's owners: its fn's, or the modules'. */
static QOwners *qsem_frame(QSem *const S) { return S->fn ? &S->fn->own : &S->prog->own; }

/* Room in the frame checked for the loops dropping a value of type t. */
void qsem_dropix(QSem *const S, const QType t)
{
    QOwners *const o = qsem_frame(S);
    const uint32_t n = qast_dropdepth(S->prog, t);

    if (n <= o->nix)
        return;

    QVar **const ix = qast_alloc(S->mem, n * sizeof *ix);

    for (uint32_t k = 0; k < n; k++)
        ix[k] = k < o->nix ? o->ix[k] : qsem_var(S, &S->prog->src.toks[0], QY_INT, QV_TEMP);

    o->ix = ix;
    o->nix = n;
}

/* Let or var v of a type with drop: an owner of its frame, its live flag set when it gets a
   value. */
static void qsem_owner(QSem *const S, QVar *const v)
{
    qsem_dropin(S, v->type, v->name);

    if (!qast_drops(S->prog, v->type))
        return;

    if (S->fn && (S->fn->ytx || qast_inline(S->fn))) {
        qsem_say(S, v->name, "a generator can't own ", qsem_aname(S, v->type), nullptr);
        return;
    }

    qsem_dropcalls(S, v->type);
    QOwners *const o = qsem_frame(S);
    v->live = qsem_var(S, v->name, QY_INT, QV_TEMP);
    o->owned = qast_room(S->mem, o->owned, o->nowned, &o->capowned, sizeof *o->owned);
    o->owned[o->nowned++] = v;
    qsem_dropix(S, v->type);
}

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
    } else {
        d->var = qsem_var(S, d->name, type, kw == QK_LET ? QV_LET : QV_VAR);
        qsem_owner(S, d->var);
    }

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
        qsem_held(S, v);

    uint32_t k = 0;

    for (QDecl *d = s->decls; d; d = d->next, k++) {
        const QType t = fits ? qast_ty(S->prog, v->type)->fields[k].type : QY_ERROR;
        d->var = qsem_var(S, d->name, t, s->op == QK_LET ? QV_LET : QV_VAR);
        qsem_owner(S, d->var);
        qsem_bind(S, d->name, d->var, nullptr);
    }
}

void qsem_decls(QSem *const S, QStmt *const s)
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
        qsem_say(S, e->tok, qsem_text(S, e->tok), " is a function: call it", nullptr);
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

        if (qast_drops(S->prog, to->items[k]->type))
            qsem_say(S, to->items[k]->tok, "a, b = of values with drop: assign each", nullptr);
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
            qsem_held(S, v);

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

QExpr *qsem_nameof(QSem *const S, QVar *const v, const QTok *const tok)
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

    if (s->op == QK_EQ)
        s->e = qsem_intoplace(S, t, qsem_value(S, s->e, t->type));
    else
        qsem_compound(S, s, t);

    if (qast_drops(S->prog, t->type)) /* the new value first, then the old one dropped */
        qsem_replaced(S, s->e);
}

/* Value e replacing a value with drop: in a temporary, unless an owner's moved; room to drop the
   old. */
static void qsem_replaced(QSem *const S, QExpr *const e)
{
    if (qast_fresh(e) && !e->var)
        e->var = qsem_var(S, e->tok, e->type, QV_TEMP);

    qsem_dropix(S, e->type);
}

/* t op= v: t = t op v. */
static void qsem_compound(QSem *const S, QStmt *const s, QExpr *const t)
{
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

/* for i = a to b [step s], for i in a..b: ints; a constant step not 0. Its limit and step are variables, as
   I.COUNT's (qr/qcg.c); by its step if constant, else 0. */
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

void qsem_lend(QSem *const S, const QVar *const root, const QStmt *const s)
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
        qsem_say(S, s->tok, "a dict's keys don't change: assign d[k] for its values", nullptr);

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

/* Is a for's element of type t read from a copy its loop keeps: a number, an int cell, a record
   of them (qr/qcg.c qcg_copyat)? */
static bool qsem_copied(const QSem *const S, const QType t)
{
    if (!qast_compound(t))
        return t != QY_STRING && t != QY_VOID && t != QY_ERROR;

    const QTy *const y = qast_ty(S->prog, t);

    if (qast_cell(S->prog, t))
        return true;
    if (y->kind == QT_ARRAY)
        return qsem_copied(S, y->elem);
    if (!qast_record(S->prog, t) || y->kind == QT_MAP || y->kind == QT_RES)
        return false;

    for (uint32_t k = 0; k < y->nfields; k++)
        if (!qsem_copied(S, y->fields[k].type))
            return false;

    return true;
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

    if (!s->mut && a->kind == QE_NAME && !qast_array(S->prog, elem) && qsem_copied(S, elem))
        at->var = qsem_var(S, x->name, elem, QV_TEMP);
}

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

    if (f->ytx) { /* return x: x the last yielded */
        if (s->e)
            qsem_yield(S, s);

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
            f->test ? "a test" : qsem_text(S, f->name),
            f->test ? " returns nothing" : " returns nothing: it has no as type",
            nullptr
        );
        qsem_expr(S, s->e, QY_VOID);
        return;
    }

    s->e = qsem_owned(S, qsem_coerce(S, qsem_expr(S, s->e, f->result), f->result), true);
    s->e = qsem_into(S, f->res, s->e);
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

    case QS_CALL: /* a value with drop dropped */
        s->e = qsem_held(S, qsem_expr(S, s->e, QY_VOID));
        qsem_dropix(S, s->e->type);
        return;

    case QS_RETURN:
        qsem_return(S, s);
        return;

    case QS_YIELD:
        qsem_yield(S, s);
        return;

    case QS_ASSERT:
        s->e = qsem_coerce(S, qsem_expr(S, s->e, QY_BOOL), QY_BOOL);
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
        return !last->e && !last->link->broken;
    if (last->kind == QS_IF && last->oneline)
        return last->other && qsem_returns(last->body) && qsem_returns(last->other);

    if (last->kind == QS_ENDSELECT) {
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

    if (a->kind != QE_NAME || !a->var || (a->var->kind != QV_LET && a->var->kind != QV_PARAM) ||
        a->type == QY_ERROR)
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

/* The bindings in sight now, the last statement's kept when they are the same. */
static QScopeAt *qsem_sight(QSem *const S)
{
    const uint32_t lo = S->hidelo < S->hidehi ? S->hidelo : S->nbinds;
    const uint32_t hi = S->hidelo < S->hidehi ? S->hidehi : S->nbinds;
    const uint32_t n = S->nbinds - (hi - lo);
    QScopeAt *const was = S->at;

    if (was && was->fn == S->fn && was->n == n &&
        !memcmp(was->binds, S->binds, lo * sizeof *S->binds) &&
        !memcmp(was->binds + lo, S->binds + hi, (n - lo) * sizeof *S->binds))
        return was;

    QScopeAt *const at = qast_alloc(S->mem, sizeof *at);
    *at = (QScopeAt){.fn = S->fn,
                     .n = n,
                     .binds = qast_alloc(S->mem, (n ? n : 1) * sizeof *S->binds)};

    memcpy(at->binds, S->binds, lo * sizeof *S->binds);
    memcpy(at->binds + lo, S->binds + hi, (n - lo) * sizeof *S->binds);
    return S->at = at;
}

void qsem_stmt(QSem *const S, QStmt *const s)
{
    s->own = qsem_frame(S);

    if (S->prog->keep)
        s->at = qsem_sight(S);

    switch (s->kind) {
    case QS_IF:
        qsem_cond_of(S, s);

        qsem_push(S);
        qsem_narrow(S, s->e, true);

        if (s->oneline) {
            if (s->body) /* null: it failed to parse */
                qsem_simple(S, s->body);

            qsem_pop(S);

            if (s->other) {
                qsem_push(S);
                qsem_narrow(S, s->e, false);
                qsem_simple(S, s->other);
                qsem_pop(S);
            }

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

    case QS_SELECT:
        qsem_match(S, s);
        return;

    case QS_CASE:
        qsem_case(S, s);
        return;

    case QS_ENDSELECT:
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

        if (s->kind == QS_ENDDO && s->e) /* the body's names ended */
            qsem_cond_of(S, s);
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
