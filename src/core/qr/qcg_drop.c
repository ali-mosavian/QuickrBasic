/* Quickr's code generator: drop: a value's end.
 *
 * # How it works
 * A value with drop, or holding one, is dropped where its place's life ends: a block's end, a
 * return, a value replaced; a moved one is not.
 */
#include "qr/qcg_int.h"

/* ---- drop */

void qcg_lastof(QCgLast *const l, const QExpr *const a, const QType elem)
{
    l->len = (QExpr){
        .kind = QE_CALL,
        .builtin = QB_LEN,
        .a = (QExpr *)a,
        .type = QY_INT,
    };

    l->one = (QExpr){.kind = QE_CONST, .type = QY_INT, .val.i = 1};

    l->last = (QExpr){
        .kind = QE_BINARY,
        .op = QK_MINUS,
        .a = &l->len,
        .b = &l->one,
        .type = QY_INT,
        .work = QY_INT,
    };

    l->at = (QExpr){
        .kind = QE_INDEX,
        .a = (QExpr *)a,
        .b = &l->last,
        .type = elem,
    };
}

bool qcg_drops(const QCg *const g, const QType t) { return qast_drops(g->env->prog, t); }

/* List or array place p's elements dropped in turn, by the frame's index at depth. */
static void qcg_dropeach(QCg *const g, const QExpr *const p, const uint32_t depth)
{
    Code *const c = g->c;
    const QTy *const y = qcg_ty(g, p->type);
    QVar *const i = g->own->ix[depth];
    const QExpr at = qcg_named(i);
    const QExpr elem = {
        .kind = QE_INDEX,
        .a = (QExpr *)p,
        .b = (QExpr *)&at,
        .type = y->elem,
    };

    code_int(c, 0);
    qcg_store(c, i);
    const uint32_t top = qcg_top(g);
    qcg_load(c, i);
    uint8_t *const right = c->out;

    if (y->kind == QT_ARRAY)
        code_int(c, y->count);
    else {
        qcg_cell(g, p);
        qcg_x(c, QX_LEN);
    }

    qcg_compare(c, QY_INT, QK_LT, right);
    const QCgHop out = qcg_hop(g, QJ_FALSE);
    qcg_drop(g, &elem, depth + 1);
    qcg_bump(c, i, 1);
    qcg_back(g, QJ_ALWAYS, top);
    qcg_land(g, out);
}

/* The value with drop in place p dropped: a record's fn drop, then its fields'; a list's
   elements'; T?'s value when it has one, T!'s when no error. */
void qcg_drop(QCg *const g, const QExpr *const p, const uint32_t depth)
{
    const QTy *const y = qcg_ty(g, p->type);

    switch (y->kind) {
    case QT_REC:
    case QT_TUPLE:
        if (y->drop) {
            qcg_ref(g, p);
            qcg_pcall(g, y->drop);
        }

        for (uint32_t k = 0; k < y->nfields; k++)
            if (qcg_drops(g, y->fields[k].type)) {
                const QExpr f = qcg_fieldof(g, p, k);
                qcg_drop(g, &f, depth);
            }

        return;

    case QT_OPT:
    case QT_RES: {
        const QExpr flag = qcg_fieldof(g, p, QF_FLAG);
        const QExpr v = qcg_fieldof(g, p, QF_VAL);
        qcg_loadat(g, &flag);
        const QCgHop none = qcg_hop(g, y->kind == QT_OPT ? QJ_FALSE : QJ_TRUE);
        qcg_drop(g, &v, depth);
        qcg_land(g, none);
        return;
    }

    default:
        qcg_dropeach(g, p, depth);
    }
}

/* Owner v's live flag set to on. */
void qcg_live(QCg *const g, const QVar *const v, const bool on)
{
    code_int(g->c, on ? 1 : 0);
    qcg_store(g->c, v->live);
}

/* Owner v's value dropped if it holds one. */
void qcg_dropvar(QCg *const g, QVar *const v)
{
    const QExpr p = qcg_named(v);
    qcg_load(g->c, v->live);
    const QCgHop none = qcg_hop(g, QJ_FALSE);
    qcg_drop(g, &p, 0);
    qcg_land(g, none);
}

/* The frame's owners' values dropped, the last declared first: its fn returns, the program
   ends. */
void qcg_dropall(QCg *const g, const QOwners *const o)
{
    for (uint32_t k = o->nowned; k-- > 0;)
        qcg_dropvar(g, o->owned[k]);
}

/* The value with drop in place p dropped before p takes another: an owner's if it holds one. */
void qcg_dropold(QCg *const g, const QExpr *const p)
{
    if (p->kind == QE_NAME && p->var->live)
        qcg_dropvar(g, p->var);
    else
        qcg_drop(g, p, 0);
}

/* List place a's last element dropped, if its type has drop. */
void qcg_droplast(QCg *const g, const QExpr *const a, const QType elem)
{
    if (!qcg_drops(g, elem))
        return;

    QCgLast l;
    qcg_lastof(&l, a, elem);
    qcg_drop(g, &l.at, 0);
}

/* Value e taken from its owner: its live flag cleared. */
void qcg_moved(QCg *const g, const QExpr *const e)
{
    if (e->kind == QE_NAME && e->move)
        qcg_live(g, e->var, false);
}
