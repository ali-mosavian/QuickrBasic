/* Quickr's semantic pass: select case and its patterns.
 *
 * # How it works
 * A case's pattern is a value of the select's type, a range, is op v, or a data enum's value
 * with its data bound; end select checks each value has its case.
 */
#include "qr/qsem_int.h"

/* ---- match */

static bool qsem_isdata(const QSem *const S, const QType t)
{
    return qast_compound(t) && qast_ty(S->prog, t)->kind == QT_ENUM && qast_ty(S->prog, t)->data;
}

/* select case e: a number, a string, a bool or an enum's; a variable's read where it is, else held in
   s->temp; an enum with data's tag held, its data read at its place. */
void qsem_match(QSem *const S, QStmt *const s)
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
            "select case takes a number, a string, a bool or an enum, not ",
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
        if (x->name || x->e->kind != QE_NAME || n >= v->n) {
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

/* A case's value e of plain enum t's: one of its values by its name alone, else e checked. */
static QExpr *qsem_bare(QSem *const S, QExpr *const e, const QType t)
{
    if (!qast_tag(S->prog, t) || e->kind != QE_NAME)
        return qsem_expr(S, e, t);

    const QTy *const y = qast_ty(S->prog, t);

    for (uint32_t k = 0; k < y->nvars; k++)
        if (qsem_same(y->vars[k].name, e->tok))
            return qsem_variant(S, e, t, false);

    return qsem_expr(S, e, t);
}

/* A case's pattern for select m: a value of its type, a to b, is op v. */
static void qsem_pat(QSem *const S, QOpenMatch *const m, QPat *const pt, const bool alone)
{
    const QType t = m->s->ty;

    if (t == QY_ERROR)
        return;

    if (qsem_isdata(S, t)) {
        qsem_varpat(S, m, pt, alone);
        return;
    }

    pt->a = qsem_coerce(S, qsem_bare(S, pt->a, t), t);

    if (pt->b)
        pt->b = qsem_coerce(S, qsem_bare(S, pt->b, t), t);

    const bool ordered = qast_numeric(t) || t == QY_STRING;

    if (pt->kind == QP_RANGE && !ordered)
        qsem_say(S, pt->tok, "to takes numbers or strings", nullptr);
    if (pt->kind == QP_IS && !ordered && pt->op != QK_EQ && pt->op != QK_NE)
        qsem_say(S, pt->tok, qast_spelling(pt->op), " takes numbers or strings", nullptr);

    if (pt->kind == QP_VALUE && m->nseen && qsem_isconst(pt->a) && pt->a->val.i >= 0 &&
        (uint64_t)pt->a->val.i < m->nseen)
        m->seen[pt->a->val.i] = 1;
}

void qsem_case(QSem *const S, QStmt *const s)
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

/* end select: each value has its case, or there is a case else. */
void qsem_endmatch(QSem *const S, const QStmt *const s)
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
            "the select misses ",
            what,
            ": give it a case, or add case else",
            nullptr
        );
    else
        qsem_say(S, s->tok, "the select misses values: add case else", nullptr);
}
