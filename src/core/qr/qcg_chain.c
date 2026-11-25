/* Quickr's code generator: chains: one loop for a sequence's steps and end.
 *
 * # How it works
 * A chain is its source's loop, each step on the element in turn, its end gathering the
 * result; no list between.
 */
#include "qr/qcg_int.h"

/* ---- chains */

/* A chain's loop's exits: to its next element and past its end. A for's are jumps to its end
   for's start and end; an expression's, hops within the line, landed after. */
typedef struct {
    const QStmt *end; /* the for's next; null an expression's */
    QCgHop *hops;
    bool *done; /* each hop's: past the end, else to the next element */
    uint32_t n, cap;
} QCgExits;

static void qcg_leave(QCg *const g, QCgExits *const x, const QrtWhen when, const bool done)
{
    static const uint8_t ops[] = {[QJ_ALWAYS] = O_JMP, [QJ_FALSE] = O_I_JF, [QJ_TRUE] = O_I_JT};

    if (x->end) {
        qcg_to(g, ops[when], x->end, done ? BOFF : 0, TO_STMT);
        return;
    }

    uint32_t cap = x->cap;
    x->hops = qast_room(g->mem, x->hops, x->n, &x->cap, sizeof *x->hops);
    x->done = qast_room(g->mem, x->done, x->n, &cap, sizeof *x->done);
    x->done[x->n] = done;
    x->hops[x->n++] = qcg_hop(g, when);
}

/* x's exits to the next element (done false) or past the end land here. */
static void qcg_exits(QCg *const g, const QCgExits *const x, const bool done)
{
    for (uint32_t k = 0; k < x->n; k++)
        if (x->done[k] == done)
            qcg_land(g, x->hops[k]);
}

/* Chain it's i .. lim, half-open, to its first and last, counted by stp; on to its end through x
   when empty. Tested before the end less one, which overflows only when empty. */
static void qcg_bounds(QCg *const g, const QIter *const it, QCgExits *const x)
{
    Code *const c = g->c;
    qcg_load(c, it->i);
    uint8_t *const right = c->out;
    qcg_load(c, it->lim);
    qcg_compare(c, QY_INT, QK_GE, right);
    qcg_leave(g, x, QJ_TRUE, true);
    qcg_load(c, it->lim);
    uint8_t *const one = c->out;
    code_int(c, 1);
    qcg_arith(c, one, QY_INT, QA_SUB);

    if (it->rev) { /* from the last down to i */
        qcg_load(c, it->i);
        qcg_store(c, it->lim);
    }

    qcg_store(c, it->rev ? it->i : it->lim);
}

/* v - w to the stack, wrapped: a span past an int's range is right as a uint. */
static void qcg_span(Code *const c, QVar *const v, QVar *const w)
{
    qcg_load(c, v);
    qcg_load(c, w);
    code_xop(c, X_IW_SUB);
}

/* Window count t, the stack's top, at most lim - i, both as uints: what a step keeps of the
   window. floor: first 0 if below it. */
static void qcg_clamp(QCg *const g, const QIter *const it, QVar *const t, const bool floor)
{
    Code *const c = g->c;
    qcg_store(c, t);

    for (uint32_t k = !floor; k < 2; k++) {
        qcg_load(c, t);
        uint8_t *const right = c->out;

        if (k)
            qcg_span(c, it->lim, it->i);
        else
            code_int(c, 0);

        qcg_compare(c, k ? QY_UINT : QY_INT, k ? QK_GT : QK_LT, right);
        const QCgHop in = qcg_hop(g, QJ_FALSE);

        if (k)
            qcg_span(c, it->lim, it->i);
        else
            code_int(c, 0);

        qcg_store(c, t);
        qcg_land(g, in);
    }
}

/* v to w when w is below it: an empty range's span 0. */
static void qcg_atleast(QCg *const g, QVar *const v, QVar *const w)
{
    Code *const c = g->c;
    qcg_load(c, v);
    uint8_t *const right = c->out;
    qcg_load(c, w);
    qcg_compare(c, QY_INT, QK_LT, right);
    const QCgHop in = qcg_hop(g, QJ_FALSE);
    qcg_load(c, w);
    qcg_store(c, v);
    qcg_land(g, in);
}

/* Chain it's window, i to lim past its last, narrowed by each take, skip and zip before its
   rev(). Spans and offsets wrap: right as uints, as a zip's b's count past an int's range is. */
static void qcg_windows(QCg *const g, const QIter *const it)
{
    Code *const c = g->c;
    qcg_expr(g, it->lo);
    qcg_store(c, it->i);
    qcg_expr(g, it->hi);
    qcg_store(c, it->lim);
    qcg_atleast(g, it->lim, it->i);

    for (uint32_t k = 0; k < it->n; k++) {
        const QStage *const st = &it->stages[k];

        if (!st->win)
            continue;

        if (st->kind != QI_ZIP)
            qcg_expr(g, st->e);
        else if (st->of->type == QY_INT) { /* b's count: its end less its first, 0 if below */
            qcg_expr(g, st->to);
            qcg_store(c, st->k);
            qcg_expr(g, st->of);
            qcg_store(c, st->n);
            qcg_atleast(g, st->k, st->n);
            qcg_span(c, st->k, st->n);
        } else
            qcg_expr(g, st->to);

        qcg_clamp(g, it, st->n, st->kind != QI_ZIP);

        if (st->kind == QI_ZIP) { /* b's first less i */
            if (st->of->type == QY_INT)
                qcg_expr(g, st->of);
            else
                code_int(c, 0);

            qcg_load(c, it->i);
            code_xop(c, X_IW_SUB);
            qcg_store(c, st->k);
        }

        qcg_load(c, it->i);
        qcg_load(c, st->n);
        code_xop(c, X_IW_ADD);
        qcg_store(c, st->kind == QI_SKIP ? it->i : it->lim);
    }

    for (uint32_t k = 0; k < it->n; k++) { /* each zip's b's index past the window's last */
        const QStage *const st = &it->stages[k];

        if (st->win && st->kind == QI_ZIP) {
            qcg_load(c, st->k);
            qcg_load(c, it->lim);
            code_xop(c, X_IW_ADD);
            qcg_store(c, st->n);
        }
    }
}

/* Does chain it count down over a window (QStage.win)? */
static bool qcg_windowed(const QIter *const it)
{
    for (uint32_t k = 0; k < it->n; k++)
        if (it->stages[k].win)
            return true;

    return false;
}

/* Chain it's start: its source held, its counter from its first, each step's and its end's
   state; on to its end when its source is empty. */
static void qcg_chainstart(QCg *const g, const QIter *const it, QCgExits *const x)
{
    Code *const c = g->c;

    if (it->hold) {
        const QExpr h = qcg_named(it->hold);
        qcg_set(g, &h, it->src);
    } else if (it->src)
        qcg_settle(g, it->src);

    if (it->start) {
        const QExpr state = qcg_named(it->gen);
        qcg_set(g, &state, it->start);
    }

    if (qcg_windowed(it))
        qcg_windows(g, it);
    else if (it->from != QO_GEN) { /* i from its first, lim its end */
        qcg_expr(g, it->lo);
        qcg_store(c, it->i);
        qcg_expr(g, it->hi);
        qcg_store(c, it->lim);
    }

    if (it->from != QO_GEN) {
        code_int(c, it->rev ? (uint32_t)-1 : 1);
        qcg_store(c, it->stp);
    }

    for (uint32_t k = 0; k < it->n; k++) {
        const QStage *const st = &it->stages[k];

        if (st->win)
            continue;

        if (st->kind == QI_TAKE || st->kind == QI_SKIP) {
            qcg_expr(g, st->e);
            qcg_store(c, st->n);
            code_int(c, 0);
            qcg_store(c, st->k);
        } else if (st->kind == QI_ZIP) { /* n from b's first, k its end */
            if (st->of->type == QY_INT)
                qcg_expr(g, st->of);
            else
                code_int(c, 0);

            qcg_store(c, st->n);
            qcg_expr(g, st->to);
            qcg_store(c, st->k);
        }
    }

    switch (it->sink) {
    case QZ_SUM:
    case QZ_COUNT:
    case QZ_ANY:
    case QZ_ALL:
        if (it->sink == QZ_ALL)
            code_int(c, 1);
        else
            qcg_zero(g, it->acc->type);

        qcg_store(c, it->acc);
        break;

    case QZ_MIN:
    case QZ_MAX:
    case QZ_FIND:
        code_int(c, 0);
        qcg_store(c, it->has);
        break;

    case QZ_FOLD: {
        const QExpr a = qcg_named(it->acc);
        qcg_set(g, &a, it->init);
        break;
    }

    case QZ_COLLECT:
        qcg_fresh(g, it->acc);
        break;

    default:
        break;
    }

    if (it->from != QO_GEN)
        qcg_bounds(g, it, x);
}

/* Chain it's element, each time: its source's, through its steps. */
static void qcg_element(QCg *const g, const QIter *const it, QCgExits *const x)
{
    Code *const c = g->c;

    if (it->from == QO_GEN) {
        const QExpr got = qcg_named(it->got);
        qcg_set(g, &got, it->next);
        const QExpr has = qcg_fieldof(g, &got, QF_FLAG);
        qcg_expr(g, &has);
        qcg_leave(g, x, QJ_FALSE, true);
    }

    for (uint32_t k = 0; k < it->n; k++) {
        const QStage *const st = &it->stages[k];

        switch (st->kind) {
        case QI_MAP:
            if (st->x) {
                const QExpr v = qcg_named(st->x);
                qcg_set(g, &v, st->e);
            }

            break;

        case QI_FILTER:
            qcg_expr(g, st->e);
            qcg_leave(g, x, QJ_FALSE, false);
            break;

        case QI_TAKE: {
            if (st->win)
                break;

            qcg_load(c, st->k);
            uint8_t *const right = c->out;
            qcg_load(c, st->n);
            qcg_compare(c, QY_INT, QK_GE, right);
            qcg_leave(g, x, QJ_TRUE, true);
            qcg_bump(c, st->k, 1);
            break;
        }

        case QI_SKIP: {
            if (st->win)
                break;

            qcg_load(c, st->k);
            uint8_t *const right = c->out;
            qcg_load(c, st->n);
            qcg_compare(c, QY_INT, QK_LT, right);
            const QCgHop past = qcg_hop(g, QJ_FALSE);
            qcg_bump(c, st->k, 1);
            qcg_leave(g, x, QJ_ALWAYS, false);
            qcg_land(g, past);
            break;
        }

        case QI_ZIP: {
            if (st->win) { /* b's index, counted down with i's */
                qcg_load(c, st->n);
                uint8_t *const right = c->out;
                code_int(c, 1);
                qcg_arith(c, right, QY_INT, QA_SUB);
                qcg_store(c, st->n);
                const QExpr v = qcg_named(st->x);
                qcg_set(g, &v, st->e);
                break;
            }

            qcg_load(c, st->n);
            uint8_t *const right = c->out;
            qcg_load(c, st->k);
            qcg_compare(c, QY_INT, QK_GE, right);
            qcg_leave(g, x, QJ_TRUE, true);
            const QExpr v = qcg_named(st->x);
            qcg_set(g, &v, st->e);
            qcg_bump(c, st->n, 1);
            break;
        }
        }
    }
}

/* Chain it's element used up by its end: kept, counted, tested. */
static void qcg_used(QCg *const g, const QIter *const it, QCgExits *const x)
{
    Code *const c = g->c;
    const QExpr acc = it->acc ? qcg_named(it->acc) : (QExpr){};

    const QExpr *const v = it->parts[0];

    switch (it->sink) {
    case QZ_SUM: {
        const QType t = it->acc->type;
        qcg_load(c, it->acc);
        uint8_t *const right = c->out;
        qcg_as(g, v, t);

        if (qast_integer(t) && t != QY_INT)
            qcg_iarith(g, right, t, QK_PLUS, false);
        else
            qcg_arith(c, right, t, QA_ADD);

        qcg_store(c, it->acc);
        return;
    }

    case QZ_COUNT:
        if (it->pred) {
            qcg_expr(g, it->pred);
            qcg_leave(g, x, QJ_FALSE, false);
        }

        qcg_bump(c, it->acc, 1);
        return;

    case QZ_ANY:
    case QZ_ALL:
        qcg_expr(g, it->pred);
        qcg_leave(g, x, it->sink == QZ_ANY ? QJ_FALSE : QJ_TRUE, false);
        code_int(c, it->sink == QZ_ANY);
        qcg_store(c, it->acc);
        qcg_leave(g, x, QJ_ALWAYS, true);
        return;

    case QZ_FIND:
        qcg_expr(g, it->pred);
        qcg_leave(g, x, QJ_FALSE, false);
        qcg_set(g, &acc, v);
        code_int(c, 1);
        qcg_store(c, it->has);
        qcg_leave(g, x, QJ_ALWAYS, true);
        return;

    case QZ_MIN:
    case QZ_MAX: { /* the first, or one past acc */
        qcg_load(c, it->has);
        const QCgHop first = qcg_hop(g, QJ_FALSE);
        qcg_expr(g, v);
        const uint32_t held = v->type == QY_STRING;
        g->strings += held;
        uint8_t *const right = c->out;
        qcg_load(c, it->acc);
        g->strings -= held;
        qcg_compare(c, v->type, it->sink == QZ_MIN ? QK_LT : QK_GT, right);
        qcg_leave(g, x, QJ_FALSE, false);
        qcg_land(g, first);
        qcg_set(g, &acc, v);
        code_int(c, 1);
        qcg_store(c, it->has);
        return;
    }

    case QZ_FOLD:
        qcg_set(g, &acc, it->pred);
        return;

    case QZ_COLLECT:
        qcg_expr(g, it->push);
        return;

    case QZ_FOR: /* each name's part, unless it names it where it is */
        for (uint32_t k = 0; k < it->nparts; k++)
            if (!it->xs[k]->place) {
                const QExpr p = qcg_named(it->xs[k]);
                qcg_set(g, &p, it->parts[k]);
            }

        return;
    }
}

/* Chain it's loop's end: on to the next element, its counter stepped, back to its start. */
void qcg_again(QCg *const g, const QIter *const it, const uint32_t line)
{
    Code *const c = g->c;

    if (it->from != QO_GEN)
        qcg_count(c, it->i, it->lim, it->stp, it->rev ? -1 : 1);

    const QrtWhen when = it->from == QO_GEN ? QJ_ALWAYS : QJ_TRUE;

    if (line == g->line) {
        qcg_back(g, when, it->top);
        return;
    }

    static const uint8_t ops[] = {[QJ_ALWAYS] = O_JMP, [QJ_TRUE] = O_I_JT};

    code_op(c, ops[when]);
    code_dword(c, line);
    code_dword(c, it->top);
    code_dword(c, TO_STMT);
}

/* Chain expression e's loop, its value in its acc. */
void qcg_iter(QCg *const g, const QExpr *const e)
{
    QIter *const it = e->iter;
    QCgExits x = {};

    qcg_chainstart(g, it, &x);
    it->top = qcg_top(g);
    qcg_element(g, it, &x);
    qcg_used(g, it, &x);
    qcg_exits(g, &x, false);
    qcg_again(g, it, g->line);
    qcg_exits(g, &x, true);
}

/* for x in a chain, s: its loop's start and element on the for's line, its body after. */
void qcg_chainfor(QCg *const g, const QStmt *const s)
{
    QIter *const it = s->e->iter;
    QCgExits x = {.end = s->link};

    qcg_chainstart(g, it, &x);
    it->top = qcg_top(g);
    qcg_element(g, it, &x);
    qcg_used(g, it, &x);
}
