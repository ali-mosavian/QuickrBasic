/* Quickr's code generator: a checked statement to the VM's stack code, as QBasic's makes it.
 *
 * # How it works
 * Every operand is pushed, then its op: I.ADD, F.ADD, D.ADD by the operands' type, which sema
 * made one. A right operand that is one load folds into the operator (I.ADD.V, I.CMP.U8), and the
 * line's code is fused (comp/fuse.c), as QBasic's is. A bool is an int, -1 true, as QBasic's
 * compares give it. A variable is its id, a parameter's through its ref (V.LD, V.ST);
 * link_resolve (comp/select.c) places each.
 *
 * Control flow is the VM's jumps, each a line and a body offset: 0 the statement, BOFF the line's
 * end, which threads to the next line. A jump within the line (and, or, a if c else b, chains)
 * goes to a label: an entry of the line's statement map, body 1, 2 ... A jump drops the string
 * temporaries, so with a string on the stack it is CALL QJ by an offset instead (qr/qrt.c). Fusion
 * runs a segment at a time, ended where a jump lands or a QJ starts, so neither moves.
 *
 * A for loop is QBasic's FOR, its limit made inclusive once the loop is entered. Its next steps
 * only while the limit is a step or more away, so the counter stops at an int's last value:
 * X.I.COUNT, fused to I.COUNT.UP or .DN when the step's sign is known. A step whose sign is known
 * only when run enters by I.PAST.
 *
 * A string literal loads from its line's text in the program store; an escape is CHR$ of its
 * byte, joined on.
 *
 * A record is QBasic's TYPE: a variable's a box R.ADDR finds, a field FL.LD and FL.ST at its
 * offset, a nested one's offsets added at compile time. A list is a dynamic array: an element
 * I.LD.A1 and its kin, a record's RA.ADDR, a field's list's FE.ADDR, a fixed array field's
 * FX.IDX; its methods and an f-string's specs are CALL QX (qr/qrt.c). A record or list result is
 * written through a last parameter, the caller's place. A call, a fn value's, a native's, or a
 * list's or a map's that may grow or shrink, is held as QBasic's is (comp/code.h code_calling). A
 * select case is QBasic's: its value held, each case's line its tests, past the case before's JMP
 * to end select.
 *
 * This file makes a statement's code; each part of the making is a file of its own, behind
 * qcg_int.h: ops, jumps and loops (qcg_emit), values (qcg_expr), records and lists (qcg_place),
 * drops (qcg_drop), chains (qcg_chain) and generators (qcg_generator).
 *
 * # Example
 *    while i < n          I.LD.V i, I.JFC.V n < (wend's line, BOFF)
 *    wend                 JMP (while's line, 0)
 */
#include "qr/qcg_int.h"
#include "comp/codegen.h"

/* ---- statements */

/* A clause's target from the one before it: past an elseif's or else's JMP, or the end if. */
static void qcg_clause(QCg *const g, const uint8_t op, const QStmt *const next)
{
    const JumpMode mode = next->kind == QS_ENDIF || next->kind == QS_ENDSELECT ? TO_STMT : PAST_JMP;
    qcg_to(g, op, next, 0, mode);
}

/* The end if of the if whose clause s is. */
static const QStmt *qcg_endif(const QStmt *s)
{
    while (s->kind != QS_ENDIF)
        s = s->link;

    return s;
}

/* For s's counter: a range's, a list's index. */
static QVar *qcg_counter(const QStmt *const s) { return s->each ? s->index : s->var; }

/* The flag of for s's counter past its limit, its step's sign known (by) or not (I.PAST).
   entered: the limit inclusive; else the range's end, past when reached. */
static void qcg_past(QCg *const g, const QStmt *const s, const bool entered)
{
    Code *const c = g->c;
    qcg_load(c, qcg_counter(s));

    if (s->by) {
        uint8_t *const right = c->out;
        qcg_load(c, s->lim);
        qcg_compare(
            c,
            QY_INT,
            s->by > 0 ? (entered ? QK_GT : QK_GE) : (entered ? QK_LT : QK_LE),
            right
        );
        return;
    }

    qcg_load(c, s->lim);
    qcg_load(c, s->stp);
    code_xop(c, X_I_PAST);

    if (entered)
        return;

    qcg_load(c, qcg_counter(s));
    uint8_t *const right = c->out;
    qcg_load(c, s->lim);
    qcg_compare(c, QY_INT, QK_EQ, right);
    code_op(c, O_I_OR);
}

/* For s's limit made inclusive, the loop entered: the range's end less the step's sign. */
static void qcg_inclusive(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;
    qcg_load(c, s->lim);

    if (s->by) {
        uint8_t *const right = c->out;
        code_int(c, 1);
        qcg_arith(c, right, QY_INT, s->by > 0 ? QA_SUB : QA_ADD);
    } else { /* compares give -1 or 0: lim + (stp > 0) - (stp < 0) */
        for (uint32_t k = 0; k < 2; k++) {
            qcg_load(c, s->stp);
            uint8_t *const right = c->out;
            code_int(c, 0);
            qcg_compare(c, QY_INT, k ? QK_LT : QK_GT, right);
            code_op(c, k ? O_I_SUB : O_I_ADD);
        }
    }

    qcg_store(c, s->lim);
}

/* Copy x's E.NEXT size operand: 0 a number of 4 bytes or 8 in its own cell, else its bytes. */
static uint16_t qcg_each(const QCg *const g, const QVar *const x)
{
    const uint32_t size = qcg_size(g, x->type);
    return (uint16_t)(x->byref || qcg_rec(g, x->type) || size < 4 ? size : 0);
}

/* For s's copy of x, which its E.NEXT loads; null none. */
static const QVar *qcg_copy(const QCg *const g, const QStmt *const s)
{
    return s->each && s->var && s->var->place ? qcg_copyat(g, s->var->place) : nullptr;
}

/* let q, r = v: v's fields to the names. */
static void qcg_destructure(QCg *const g, const QStmt *const s)
{
    const QDecl *last = s->decls;

    while (last->next)
        last = last->next;

    const QExpr *const v = last->value;
    const QExpr t = qast_fresh(v) ? qcg_settle(g, v) : *v;
    uint32_t k = 0;

    for (const QDecl *d = s->decls; d; d = d->next, k++) {
        const QExpr to = qcg_named(d->var);
        const QExpr f = qcg_fieldof(g, &t, k);

        if (d->var->live)
            qcg_dropvar(g, d->var);

        qcg_set(g, &to, &f);

        if (d->var->live)
            qcg_live(g, d->var, true);
    }
}

/* a, b = x, y: the values on the stack, stored in turn from the last; or each in its temporary
   first. a, b = f(): its fields in turn. */
static void qcg_parallel(QCg *const g, const QStmt *const s)
{
    const QExpr *const to = s->target;
    const QExpr *const v = s->e;

    if (v->kind != QE_TUPLE) {
        const QExpr t = qast_fresh(v) ? qcg_settle(g, v) : *v;

        for (uint32_t k = 0; k < to->n; k++) {
            const QExpr f = qcg_fieldof(g, &t, k);
            qcg_set(g, to->items[k], &f);
        }

        return;
    }

    if (!s->temps) {
        for (uint32_t k = 0; k < to->n; k++) {
            qcg_as(g, v->items[k], to->items[k]->type);
            qcg_hold(g, to->items[k]->type, 1);
        }

        for (uint32_t k = to->n; k-- > 0;) {
            qcg_hold(g, to->items[k]->type, -1);
            qcg_store(g->c, to->items[k]->var);
        }

        return;
    }

    for (uint32_t k = 0; k < to->n; k++) {
        const QExpr t = qcg_named(s->temps[k]);
        qcg_set(g, &t, v->items[k]);
    }

    for (uint32_t k = 0; k < to->n; k++) {
        const QExpr t = qcg_named(s->temps[k]);
        qcg_set(g, to->items[k], &t);
    }
}

/* Place p, holding a value with drop, given v: v made first, p's old value dropped, then v moved
   in. */
static void qcg_replace(QCg *const g, const QExpr *const p, const QExpr *const v)
{
    const QExpr t = qast_fresh(v) ? qcg_settle(g, v) : *v;
    qcg_dropold(g, p);
    qcg_into(g, p, &t);

    if (p->kind == QE_NAME && p->var->live)
        qcg_live(g, p->var, true);
}

/* ---- --test */

/* X op x's LineRef to statement s's line: ON LOCAL ERROR's handler, RESUME's line. */
static void qcg_lineref(QCg *const g, const uint8_t x, const QStmt *const s)
{
    code_xop(g->c, x);

    if (x == X_RESUME)
        code_byte(g->c, RESUME_LINE);

    code_word(g->c, NONUM);
    code_dword(g->c, g->env->lines[s->line].at);
}

/* Test s->fn run, its name said when it returns unfailed. */
static void qcg_testrun(QCg *const g, const QStmt *const s)
{
    const QProg *const p = g->env->prog;
    const QPiece name = {.lit = s->fn->name};

    Code *const c = g->c;
    code_int(c, 0);
    qcg_store(c, p->failed);
    qcg_pcall(g, s->fn);
    qcg_load(c, p->failed);
    const QCgHop out = qcg_hop(g, QJ_TRUE);
    qcg_bump(c, p->passes, 1);
    qcg_string(g, &name);
    qcg_x(c, QX_PASSED);
    qcg_land(g, out);
}

/* Test s->fn's error handler, in its frame: its failure said, then on at its end. */
static void qcg_testfail(QCg *const g, const QStmt *const s)
{
    const QProg *const p = g->env->prog;
    const QPiece name = {.lit = s->fn->name};

    code_int(g->c, 1);
    qcg_store(g->c, p->failed);
    qcg_bump(g->c, p->fails, 1);
    qcg_string(g, &name);
    qcg_x(g->c, QX_FAILED);
    qcg_lineref(g, X_RESUME, s->fn->end);
}

/* A statement a one-line if may take, or one that is no block's. */
static void qcg_simple(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;

    switch (s->kind) {
    case QS_DECL:
        if (s->op == QK_CONST)
            return;

        if (s->decls->next && !s->decls->value && !s->decls->type) {
            qcg_destructure(g, s);
            return;
        }

        for (const QDecl *d = s->decls; d; d = d->next) {
            const QExpr to = qcg_named(d->var);

            if (d->var->live) /* a loop's declaration run again */
                qcg_dropvar(g, d->var);

            if (d->value)
                qcg_set(g, &to, d->value);
            else
                qcg_fresh(g, d->var);

            if (d->var->live)
                qcg_live(g, d->var, true);
        }

        return;

    case QS_ASSIGN:
        for (const QStmt *p = s->pre; p; p = p->next)
            qcg_set(g, p->target, p->e);

        if (s->target->kind == QE_TUPLE)
            qcg_parallel(g, s);
        else if (qcg_drops(g, s->target->type))
            qcg_replace(g, s->target, s->e);
        else
            qcg_set(g, s->target, s->e);

        return;

    case QS_CALL:
        if (qcg_byplace(g, s->e->type) && s->e->var) {
            const QExpr t = qcg_settle(g, s->e);

            if (qcg_drops(g, t.type))
                qcg_drop(g, &t, 0);

            return;
        }

        if (s->e->kind == QE_LIST && s->e->builtin == QB_POP && qcg_byplace(g, s->e->type)) {
            qcg_listop(g, s->e, nullptr);
            return;
        }

        qcg_expr(g, s->e);

        if (s->e->type != QY_VOID)
            code_op(c, O_DROP);

        return;

    case QS_BREAK:
        qcg_to(g, O_JMP, s->link->link, BOFF, TO_STMT);
        return;

    case QS_YIELD:
        qcg_yield(g, s);
        return;

    case QS_ASSERT: {
        qcg_expr(g, s->e);
        const QCgHop ok = qcg_hop(g, QJ_TRUE);
        qcg_x(c, QX_ASSERT);
        qcg_land(g, ok);
        return;
    }

    case QS_CONTINUE: {
        const QStmt *const loop = s->link;

        if (loop->kind == QS_WHILE)
            qcg_to(g, O_JMP, loop, 0, TO_STMT);
        else if (loop->kind == QS_FOR || loop->link->e) /* to its next, its loop's test */
            qcg_to(g, O_JMP, loop->link, 0, TO_STMT);
        else
            qcg_to(g, O_JMP, loop, BOFF, TO_STMT);

        return;
    }

    case QS_RETURN:
        if (s->fn->ytx && s->e) /* a last yield: resumed, it ends */
            qcg_yield(g, s);
        else if (s->fn->ytx)
            qcg_done(g, s->fn);

        if (s->fn->ytx)
            return;

        if (s->e) {
            const QExpr res = qcg_named(s->fn->res);
            qcg_set(g, &res, s->e);
        }

        qcg_dropall(g, &s->fn->own);
        code_op(c, O_PRET);
        code_dword(c, s->fn->proc);
        code_dword(c, 0);
        return;

    default:
        return;
    }
}

/* A statement a debugger stops at: one that does something, not a block's end or else. */
static bool qcg_stops(const QStmt *const s)
{
    switch (s->kind) {
    case QS_DECL:
        return s->op != QK_CONST;

    case QS_ASSIGN:
    case QS_CALL:
    case QS_IF:
    case QS_WHILE:
    case QS_FOR:
    case QS_SELECT:
    case QS_CASE:
    case QS_ENDFOR:
    case QS_BREAK:
    case QS_CONTINUE:
    case QS_RETURN:
    case QS_YIELD:
        return true;

    case QS_ENDDO:
        return s->e != nullptr;

    default:
        return false;
    }
}

/* Match m's value, as its cases compare it: its variable's, its temporary's, its tag. */
static void qcg_subject(QCg *const g, const QStmt *const m)
{
    if (m->temp)
        qcg_load(g->c, m->temp);
    else
        qcg_expr(g, m->e);
}

/* select case e: its value held, then to its first case's tests, past that line's JMP. */
static void qcg_match(QCg *const g, const QStmt *const s)
{
    if (s->place) {
        if (qast_fresh(s->e))
            qcg_settle(g, s->e);

        const QExpr tag = qcg_fieldof(g, s->place, 0);
        qcg_loadat(g, &tag);
        qcg_store(g->c, s->temp);
    } else if (s->temp) {
        qcg_expr(g, s->e);
        qcg_store(g->c, s->temp);
    }

    qcg_to(g, O_JMP, s->link, 0, PAST_JMP);
}

/* One pattern's flag: the value equal, in a to b, is op v; an enum's tag the value's. */
static void qcg_pattern(QCg *const g, const QStmt *const m, const QPat *const pt)
{
    Code *const c = g->c;
    const QType w = m->place ? QY_INT : m->ty;

    if (pt->kind == QP_VARIANT) {
        qcg_subject(g, m);
        uint8_t *const right = c->out;
        code_int(c, pt->variant);
        qcg_compare(c, QY_INT, QK_EQ, right);
        return;
    }

    for (uint32_t side = 0; side < (pt->kind == QP_RANGE ? 2u : 1u); side++) {
        qcg_subject(g, m);
        qcg_hold(g, w, 1);
        uint8_t *const right = c->out;
        qcg_as(g, side ? pt->b : pt->a, w);
        qcg_hold(g, w, -1);
        const QKind op = pt->kind == QP_IS      ? pt->op
                         : pt->kind == QP_RANGE ? (side ? QK_LE : QK_GE)
                                                : QK_EQ;
        qcg_compare(c, w, op, right);
    }

    if (pt->kind == QP_RANGE)
        code_op(c, O_I_AND);
}

/* case: the case before it done, to end select; then its patterns, any of them, else to the
   next case. */
static void qcg_case(QCg *const g, const QStmt *const s)
{
    const QStmt *end = s;

    while (end->kind != QS_ENDSELECT)
        end = end->link;

    qcg_to(g, O_JMP, end, BOFF, TO_STMT);

    if (!s->pats)
        return;

    for (const QPat *pt = s->pats; pt; pt = pt->next) {
        qcg_pattern(g, s->body, pt);

        if (pt != s->pats)
            code_op(g->c, O_I_OR);
    }

    qcg_clause(g, O_I_JF, s->link);
}

/* Statement s's code, unfused. */
static void qcg_body(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;

    switch (s->kind) {
    case QS_IF:
        qcg_expr(g, s->e);

        if (s->oneline && s->other) {
            const QCgHop no = qcg_hop(g, QJ_FALSE);
            qcg_simple(g, s->body);
            const QCgHop over = qcg_hop(g, QJ_ALWAYS);
            qcg_land(g, no);
            qcg_simple(g, s->other);
            qcg_land(g, over);
        } else if (s->oneline) {
            if (!cg_cmpif(c)) /* its line's end, as QBasic's one-line IF */
                qcg_to(g, O_I_JF, s, BOFF, TO_STMT);

            qcg_simple(g, s->body);
        } else
            qcg_clause(g, O_I_JF, s->link);

        return;

    case QS_ELSEIF:
        qcg_to(g, O_JMP, qcg_endif(s), BOFF, TO_STMT);
        qcg_expr(g, s->e);
        qcg_clause(g, O_I_JF, s->link);
        return;

    case QS_ELSE:
        qcg_to(g, O_JMP, qcg_endif(s), BOFF, TO_STMT);
        return;

    case QS_WHILE:
        qcg_expr(g, s->e);
        qcg_to(g, O_I_JF, s->link, BOFF, TO_STMT);
        return;

    case QS_ENDWHILE:
        qcg_to(g, O_JMP, s->link, 0, TO_STMT);
        return;

    case QS_ENDDO:
        if (s->e) {
            qcg_expr(g, s->e);
            qcg_to(g, s->op == QK_UNTIL ? O_I_JF : O_I_JT, s->link, BOFF, TO_STMT);
        } else
            qcg_to(g, O_JMP, s->link, BOFF, TO_STMT);

        return;

    case QS_FOR:
        if (s->fn) {
            qcg_genfor(g, s);
            return;
        }

        if (s->e->kind == QE_ITER) {
            qcg_chainfor(g, s);
            return;
        }

        if (s->each) { /* the index from 0, the limit the list's length */
            if (qast_fresh(s->e))
                qcg_settle(g, s->e);

            code_int(c, 0);
            qcg_store(c, s->index);

            if (qcg_ty(g, s->e->type)->kind == QT_ARRAY)
                code_int(c, qcg_ty(g, s->e->type)->count);
            else {
                qcg_cell(g, s->var->place->a);
                qcg_x(c, QX_LEN);
            }
        } else {
            qcg_expr(g, s->e);
            qcg_store(c, s->var);
            qcg_expr(g, s->to);
        }

        qcg_store(c, s->lim);

        if (s->by)
            code_int(c, (uint32_t)s->by);
        else
            qcg_expr(g, s->step);

        qcg_store(c, s->stp);
        qcg_past(g, s, s->inclusive);
        qcg_to(g, O_I_JT, s->link, BOFF, TO_STMT);

        if (!s->inclusive)
            qcg_inclusive(g, s);

        if (qcg_copy(g, s)) { /* x's first copy; E.NEXT the rest */
            QExpr at = *s->var->place;
            at.var = nullptr;
            const QExpr x = qcg_named((QVar *)qcg_copy(g, s));

            if (qcg_rec(g, x.type))
                qcg_copyrec(g, &x, &at);
            else {
                qcg_loadat(g, &at);
                qcg_store(c, x.var);
            }
        }

        return;

    case QS_ENDFOR: { /* NEXT's six ops (comp/fuse.c): I.COUNT.UP or I.COUNT.DN */
        const QStmt *const f = s->link;

        if (f->fn) {
            qcg_resume(g, f->fn);
            return;
        }

        if (f->e->kind == QE_ITER) {
            qcg_again(g, f->e->iter, g->env->lines[f->line].at);
            return;
        }

        if (qcg_copy(g, f)) {
            code_op(c, O_E_NEXT);
            code_dword(c, f->index->id);
            code_dword(c, f->e->var->id);
            code_dword(c, qcg_copy(g, f)->id);
            code_word(c, qcg_each(g, qcg_copy(g, f)));
            qcg_target(g, f, BOFF, TO_STMT);
            return;
        }

        qcg_count(c, qcg_counter(f), f->lim, f->stp, f->by);
        qcg_to(g, O_I_JT, f, BOFF, TO_STMT);
        return;
    }

    case QS_FN:
        if (s->fn->native)
            return;

        qcg_to(g, O_JMP, s->fn->end, BOFF, TO_STMT);

        if (s->fn->resumable) /* a call's entry, past the JMP */
            qcg_resume(g, s->fn);
        if (s->fn->onfail)
            qcg_lineref(g, X_ONLERR, s->fn->onfail);

        return;


    case QS_SELECT:
        qcg_match(g, s);
        return;

    case QS_CASE:
        qcg_case(g, s);
        return;

    case QS_ENDFN:
        if (s->fn->ytx) {
            qcg_ended(g, s);
            return;
        }

        qcg_dropall(g, &s->fn->own);
        code_op(c, O_PRET);
        code_dword(c, s->fn->proc);
        code_dword(c, 0);
        return;

    case QS_ATEND:
        qcg_dropall(g, s->own);
        return;

    case QS_TESTRUN:
        qcg_testrun(g, s);
        return;

    case QS_TESTFAIL:
        qcg_testfail(g, s);
        return;

    case QS_TESTEND:
        qcg_load(c, g->env->prog->passes);
        qcg_load(c, g->env->prog->fails);
        qcg_x(c, QX_TALLY);
        return;

    case QS_IMPORT:
    case QS_FROM:
        qcg_imports(g, s);
        return;

    default:
        qcg_simple(g, s);
    }
}

void qcg_stmt(QCg *const g, const QStmt *const s)
{
    g->seg = g->c->out;
    g->nlabels = g->hops = g->strings = 0;
    g->own = s->own;
    g->c->held = false;

    if (g->env->dbg && qcg_stops(s))
        code_op(g->c, O_DBG);

    qcg_body(g, s);
    qcg_flush(g);
}
