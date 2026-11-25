/* Quickr's code generator: generators.
 *
 * # How it works
 * A generator's state is a record of its own; next runs its body from where it yielded.
 */
#include "qr/qcg_int.h"

/* ---- generators */

static void qcg_pret(Code *const c, const QFn *const f)
{
    code_op(c, O_PRET);
    code_dword(c, f->proc);
    code_dword(c, 0);
}

/* On from where generator f stopped: to the yield its resume index names (a sole yield's
   straight); a resumable's done (-1) to its end, 0 on into its body. */
void qcg_resume(QCg *const g, const QFn *const f)
{
    Code *const c = g->c;
    const uint32_t n = f->nyields;

    for (uint32_t k = 0; k < n; k++) {
        const QStmt *const y = f->yields[k];
        const QStmt *const to = y->kind == QS_RETURN ? f->end : y->link;
        const uint32_t body = y->kind == QS_RETURN ? 0 : BOFF;

        if (k + 1 == n && !f->resumable) {
            qcg_to(g, O_JMP, to, body, TO_STMT);
            return;
        }

        qcg_load(c, f->resume);
        uint8_t *const right = c->out;
        code_int(c, k + 1);
        qcg_compare(c, QY_INT, QK_NE, right);
        qcg_to(g, O_I_JF, to, body, TO_STMT);
    }

    if (f->resumable) {
        qcg_load(c, f->resume);
        uint8_t *const right = c->out;
        code_int(c, 0);
        qcg_compare(c, QY_INT, QK_GE, right);
        qcg_to(g, O_I_JF, f->end, 0, TO_STMT);
    }
}

/* yield v: its number the resume index (when it needs one), v the loop's variable or next()'s
   result; then to the loop's body, or back to next()'s caller. */
void qcg_yield(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;
    const QFn *const f = s->fn;

    if (f->resume) {
        code_int(c, (uint32_t)s->by);
        qcg_store(c, f->resume);
    }

    if (f->resumable) {
        const QExpr res = qcg_named(f->res);
        qcg_set(g, &res, s->e);
        qcg_pret(c, f);
        return;
    }

    const QStmt *const loop = f->loop;

    if (!loop->var->place) {
        const QExpr x = qcg_named(loop->var);
        qcg_set(g, &x, s->e);
    }

    qcg_to(g, O_JMP, loop, BOFF, TO_STMT);
}

/* Generator f's return: past its loop, or to its end. */
void qcg_done(QCg *const g, const QFn *const f)
{
    if (f->resumable)
        qcg_to(g, O_JMP, f->end, 0, TO_STMT);
    else
        qcg_to(g, O_JMP, f->loop->link, BOFF, TO_STMT);
}

/* Generator s->fn's end: past its loop; a resumable's done, next() none from now on. */
void qcg_ended(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;
    const QFn *const f = s->fn;

    if (!f->resumable) {
        qcg_done(g, f);
        return;
    }

    code_int(c, (uint32_t)-1);
    qcg_store(c, f->resume);
    const QExpr res = qcg_named(f->res);
    qcg_set(g, &res, s->e);
    qcg_pret(c, f);
}

/* for x in g(..): g's parameters given their values, then into its body. */
void qcg_genfor(QCg *const g, const QStmt *const s)
{
    const QFn *const f = s->fn;

    for (uint32_t k = 0; k < f->npar; k++) {
        QVar *const v = f->params[k].var;

        if (!f->params[k].lam && !f->params[k].hidden && !v->constant && !v->place) {
            const QExpr p = qcg_named(v);
            qcg_set(g, &p, s->e->argv[k]);
        }
    }

    qcg_to(g, O_JMP, f->def, BOFF, TO_STMT);
}
