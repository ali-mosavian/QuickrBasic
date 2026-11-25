/* Quickr's semantic pass: ownership: places, moves and copies, and borrows.
 *
 * # How it works
 * A record, tuple or list lives in a place; a value of one moves into its new place, or is
 * copied where its old one is read again. A fn that writes module vars may not be called while
 * one of them is lent (borrow rule 9).
 */
#include "qr/qsem_int.h"

/* ---- module vars a fn writes (borrow rule 9) */

static bool qsem_modvar(const QVar *const v) { return v && v->kind == QV_VAR && !v->fn; }

/* The fn checked calls g. */
static void qsem_callto(QSem *const S, QFn *const g)
{
    QFn *const f = S->fn;

    for (uint32_t i = 0; f && i < f->ncallees; i++)
        if (f->callees[i] == g)
            return;

    if (f) {
        f->callees = qast_room(S->mem, f->callees, f->ncallees, &f->capcallees, sizeof *f->callees);
        f->callees[f->ncallees++] = g;
    }
}

/* The fn checked may drop a value of type t: each drop in it a callee. */
void qsem_dropcalls(QSem *const S, const QType t)
{
    if (!S->fn || !qast_drops(S->prog, t))
        return;

    QTy *const y = qast_ty(S->prog, t);

    if (y->seen)
        return;
    if (y->drop)
        qsem_callto(S, y->drop);

    y->seen = true;

    for (uint32_t k = 0; k < qast_parts(y); k++)
        qsem_dropcalls(S, qast_part(y, k));

    y->seen = false;
}

/* Variable v written in the fn checked: a module var noted as its write, a value it holds
   maybe dropped. */
static void qsem_wrote(QSem *const S, const QVar *const v)
{
    QFn *const f = S->fn;

    if (!f)
        return;

    qsem_dropcalls(S, v->type);

    for (uint32_t i = 0; i < f->nwrites; i++)
        if (f->writes[i] == v)
            return;

    if (qsem_modvar(v)) {
        f->writes = qast_room(S->mem, f->writes, f->nwrites, &f->capwrites, sizeof *f->writes);
        f->writes[f->nwrites++] = (QVar *)v;
    }
}

/* Call e checked: its callee noted, and e kept for qsem_lends when it lends a module var. */
void qsem_callee(QSem *const S, const QExpr *const e)
{
    if (e->type == QY_ERROR)
        return;
    if (e->fn)
        qsem_callto(S, e->fn);
    if (S->fn && e->builtin == QB_ICALL)
        S->fn->icalls = true;

    qsem_dropcalls(S, e->type);

    for (uint32_t k = 0; k < e->nargv; k++)
        if (e->argv[k]->type != QY_ERROR && qsem_modvar(qsem_root(e->argv[k]))) {
            S->lends = qast_room(S->mem, S->lends, S->nlends, &S->caplends, sizeof *S->lends);
            S->lends[S->nlends++] = (QLend){e, S->fn};

            return;
        }
}

/* Does f, or a fn it reaches, write module var v? The fn that does; null none. */
static const QFn *qsem_writer(QSem *const S, QFn *const f, const QVar *const v)
{
    if (f->walk == S->walks)
        return nullptr;

    f->walk = S->walks;

    for (uint32_t i = 0; i < f->nwrites; i++)
        if (f->writes[i] == v)
            return f;

    const QFn *w = nullptr;

    for (uint32_t i = 0; !w && i < f->ncallees; i++)
        w = qsem_writer(S, f->callees[i], v);
    for (uint32_t i = 0; !w && f->icalls && i < S->nvalues; i++)
        w = qsem_writer(S, S->values[i], v);

    return w;
}

/* The fn call l reaches that writes module var v; null none. */
static const QFn *qsem_lendee(QSem *const S, const QLend *const l, const QVar *const v)
{
    S->walks++;

    if (l->call->fn)
        return qsem_writer(S, l->call->fn, v);

    const QFn *w = nullptr;

    for (uint32_t i = 0; !w && i < S->nvalues; i++)
        w = qsem_writer(S, S->values[i], v);

    return w;
}

/* Does call e pass its argument k by value: a number, a bool, a plain enum, a string or a fn
   value, to a parameter without mut? */
static bool qsem_byvalue(const QSem *const S, const QExpr *const e, const uint32_t k)
{
    const QFn *const f = e->fn;

    if (!f && e->builtin != QB_ICALL)
        return false;
    if (f && (k >= f->npar - f->nhidden || f->params[k].mut || f->params[k].lam))
        return false;

    return !qsem_byplace(S, e->argv[k]->type);
}

/* Each call lending a module var: no fn it reaches writes it, else it passes a copy of its
   value. */
void qsem_lends(QSem *const S)
{
    for (uint32_t i = 0; i < S->nlends; i++) {
        const QLend *const l = &S->lends[i];
        S->fn = l->in;

        for (uint32_t k = 0; k < l->call->nargv; k++) {
            QExpr *const a = l->call->argv[k];
            const QVar *const v = a->type == QY_ERROR ? nullptr : qsem_root(a);
            const QFn *const w = qsem_modvar(v) ? qsem_lendee(S, l, v) : nullptr;

            if (w && qsem_byvalue(S, l->call, k))
                a->byval = true;
            else if (w)
                qsem_say(
                    S,
                    a->tok,
                    qsem_text(S, v->name),
                    " is written by ",
                    w->lamof ? "a lambda" : qsem_text(S, w->name),
                    ": this call can't borrow it",
                    nullptr
                );
        }
    }

    S->fn = nullptr;
}

/* Call e's mut arguments each its only way to its var: no other argument the same var, but one
   passed by value, a copy then. A lambda's capture is lent mut when the lambda changes it. */
void qsem_lent(QSem *const S, const QExpr *const e, const QFn *const f)
{
    for (uint32_t k = 0; k < f->npar; k++) {
        const QParam *const p = &f->params[k];
        const QExpr *const m = e->argv[k];
        const bool excl = p->hidden ? p->var->kind == QV_MUT : p->mut;
        const QVar *const v = excl && m->type != QY_ERROR ? qsem_root(m) : nullptr;

        for (uint32_t j = 0; v && j < f->npar; j++)
            if (j != k && qsem_root(e->argv[j]) == v && qsem_byvalue(S, e, j))
                e->argv[j]->byval = true;
            else if (j != k && qsem_root(e->argv[j]) == v)
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
const QArg **qsem_given(
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

/* ---- places, moves */

/* An unchecked place's variable as bound: a for's element itself, not the list it names. */
QVar *qsem_rawroot(QSem *const S, const QExpr *e)
{
    while (e->kind == QE_FIELD || e->kind == QE_INDEX)
        e = e->a;

    const QBind *const b = qsem_lookup(S, e);
    return b ? b->var : nullptr;
}

/* The for's loan of variable v, the innermost; null none. */
const QLoan *qsem_loan(const QSem *const S, const QVar *const v)
{
    for (uint32_t i = S->nloans; i-- > 0;)
        if (S->loans[i].root == v)
            return &S->loans[i];

    return nullptr;
}

/* Unchecked place e changed at at: its variable a var, a mut parameter or a for mut's element,
   and no list a for borrows. False: an error said, a for mut's loan by qsem_name. */
bool qsem_changes(QSem *const S, const QExpr *const e, const QTok *const at)
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

    qsem_wrote(S, orig);
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

/* Owned place e copied: listed for --copies. */
static void qsem_copyof(QSem *const S, QExpr *const e)
{
    const char *const name = qsem_aname(S, e->type);
    e->copy = true;
    qast_error(&S->prog->copies, e->tok, "{} copied", name, (uint32_t)strlen(name));
}

/* Value e of a type that owns a list, taken whole: a local's name moves when qsem_moved finds it
   is not used again, leaving its fn when it returns; any other place is copied. A value with drop
   only moves. */
QExpr *qsem_owned(QSem *const S, QExpr *const e, const bool returned)
{
    if (e->kind == QE_COND) { /* the arm taken moves */
        e->a = qsem_owned(S, e->a, returned);
        e->b = qsem_owned(S, e->b, returned);
        return e;
    }

    if (!qsem_owns(S, e->type) || !qast_place(e))
        return e;

    QVar *const v = e->kind == QE_NAME ? e->var : nullptr;

    if (!v || (v->kind != QV_LET && v->kind != QV_VAR)) {
        if (qast_drops(S->prog, e->type)) {
            qsem_say(
                S,
                e->tok,
                qsem_aname(S, e->type),
                " has drop: only a whole let or var moves",
                nullptr
            );
            e->type = QY_ERROR;
        } else {
            qsem_copyof(S, e);
        }

        return e;
    }

    e->move = v->live != nullptr;

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

/* Each move: its variable not used after it, nor moved in a loop it is declared outside, else a
   copy; a value with drop's an error. */
void qsem_moved(QSem *const S)
{
    for (uint32_t i = 0; i < S->nmoves; i++) {
        const QMove *const m = &S->moves[i];
        const char *const name = qsem_text(S, m->var->name);
        const char *const why = m->var->last > m->at ? " is used after it moves here"
                                : m->var->shared     ? " is seen by a function: it can't move"
                                : m->loop != UINT32_MAX && qsem_at(S, m->var->name) < m->loop
                                    ? " moves in a loop it is declared outside"
                                    : nullptr;

        if (!why)
            continue;

        if (qast_drops(S->prog, m->e->type))
            qsem_say(S, m->e->tok, name, why, "; it has drop, so it only moves", nullptr);
        else
            qsem_copyof(S, m->e);
    }
}

/* Does e, or any part of it, hit(part, v)? */
bool qsem_any(
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

bool qsem_calls(const QExpr *const e, const QVar *const)
{
    return e->kind == QE_CALL || e->kind == QE_LIST;
}

/* Is variable v read in e? */
static bool qsem_mentions(const QExpr *const e, const QVar *const v)
{
    return qsem_any(e, qsem_names, v);
}

bool qsem_byplace(const QSem *const S, const QType t)
{
    return qast_compound(t) && !qast_cell(S->prog, t);
}

/* A record's or list's value e built straight into variable v's place, unless it reads v or calls
   a fn that may see v (a module's): then in a temporary first. */
QExpr *qsem_into(QSem *const S, const QVar *const v, QExpr *const e)
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
QExpr *qsem_intoplace(QSem *const S, const QExpr *const t, QExpr *const e)
{
    if (t->kind == QE_NAME && t->var && qsem_fieldwise(S, t->var, e))
        return e;

    return qsem_into(S, qsem_root(t), e);
}

/* A record or list that is no place, where its address is wanted: built in a temporary, which
   its user empties or drops. */
QExpr *qsem_held(QSem *const S, QExpr *const e)
{
    if (qsem_byplace(S, e->type) && qast_fresh(e) && !e->var)
        e->var = qsem_var(S, e->tok, e->type, QV_TEMP);

    return e;
}

/* qsem_held's, read in place: a value with drop must have an owner. */
QExpr *qsem_addressed(QSem *const S, QExpr *const e)
{
    if (qast_fresh(e) && e->type != QY_ERROR && qast_drops(S->prog, e->type)) {
        qsem_say(S, e->tok, qsem_aname(S, e->type), " has drop: keep it in a let first", nullptr);
        e->type = QY_ERROR;
        return e;
    }

    return qsem_held(S, e);
}

/* A T? or T! of type t built: its flag and its value, null the rest's zero. */
QExpr *qsem_wrapped(
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

/* v, a T, as a T? or a T! of type to: has it, or no error. */
QExpr *qsem_wrap(QSem *const S, const QType to, QExpr *const v)
{
    const bool opt = qsem_wraps(S, to, QT_OPT);
    return qsem_wrapped(S, v->tok, to, opt, qsem_owned(S, v, false));
}

/* A value for a place of type to: converted, a move noted. */
QExpr *qsem_value(QSem *const S, QExpr *const e, const QType to)
{
    return qsem_owned(S, qsem_coerce(S, qsem_expr(S, e, to), to), false);
}
