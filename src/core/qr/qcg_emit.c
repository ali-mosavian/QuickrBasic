/* Quickr's code generator: the ops every part emits, and the line's jumps and loops.
 *
 * # How it works
 * An op by its operands' type (I, F, D, S); a variable's load and store, a parameter's through
 * its ref; an operator with its right operand's load folded in. A jump within the line goes to
 * a label, an entry of the line's statement map; with a string on the stack it is CALL QJ. A
 * loop is a top, a counter's step and a jump back.
 */
#include "qr/qcg_int.h"
#include "comp/codegen.h"
#include "comp/fuse.h"
#include "num/double.h"

static const uint8_t qcg_iops[3][6] = {
    {O_I_ADD, O_I_SUB, O_I_MUL, O_I_DIV, O_I_AND, O_I_OR},
    {O_I_ADD_V, O_I_SUB_V, O_I_MUL_V, O_I_DIV_V, O_I_AND_V, O_I_OR_V},
    {O_I_ADD_U8, O_I_SUB_U8, O_I_MUL_U8, O_I_DIV_U8, O_I_AND_U8, O_I_OR_U8},
};

static const uint8_t qcg_fops[5][4] = {
    /* by cg_fltkind's form */
    {O_F_ADD, O_F_SUB, O_F_MUL, O_F_DIV},
    {O_F_ADD_V, O_F_SUB_V, O_F_MUL_V, O_F_DIV_V},
    {O_F_ADD_VI, O_F_SUB_VI, O_F_MUL_VI, O_F_DIV_VI},
    {O_F_ADD_U8, O_F_SUB_U8, O_F_MUL_U8, O_F_DIV_U8},
    {O_F_ADD_F32, O_F_SUB_F32, O_F_MUL_F32, O_F_DIV_F32},
};

static const uint8_t qcg_dops[4] = {O_D_ADD, O_D_SUB, O_D_MUL, O_D_DIV};

/* A relation's D.CMP mask (num/double.h's DC_*). */
static uint8_t qcg_mask(const QKind op)
{
    switch (op) {
    case QK_EQ:
        return DC_EQ;

    case QK_NE:
        return DC_GT | DC_LT;

    case QK_LT:
        return DC_LT;

    case QK_LE:
        return DC_LT | DC_EQ;

    case QK_GT:
        return DC_GT;

    default:
        return DC_GT | DC_EQ;
    }
}

/* One of four ops by type: an int's or a bool's, a single's, a double's, a string's. */
uint8_t qcg_pick(const QType t, const uint8_t i, const uint8_t f, const uint8_t d, const uint8_t s)
{
    switch (t) {
    case QY_SINGLE:
        return f;

    case QY_DOUBLE:
    case QY_LONG: /* its 8 bytes moved as a double's */
    case QY_ULONG:
        return d;

    case QY_STRING:
        return s;

    default:
        return i;
    }
}

/* A parameter's ref: a fn's own, a lambda's capture's, a resumable generator's state, and a
   record's or list's result. */
bool qcg_par(const QVar *const v)
{
    return v->kind == QV_PARAM || v->kind == QV_MUT || v->cap || v->byref ||
           (v->kind == QV_RESULT && v->fn->byarg);
}

void qcg_load(Code *const c, const QVar *const v)
{
    code_opid(c, qcg_pick(v->type, O_I_LD_V, O_F_LD_V, O_D_LD_V, O_S_LD_V), v->id, qcg_par(v));
}

void qcg_store(Code *const c, const QVar *const v)
{
    code_opid(c, qcg_pick(v->type, O_I_ST_V, O_F_ST_V, O_D_ST_V, O_S_ST_V), v->id, qcg_par(v));
}

/* Operator k (QA_*) of type w after its operands: the right one's load, at right, folded in when
   one variable's or literal's. */
void qcg_arith(Code *const c, uint8_t *const right, const QType w, const uint32_t k)
{
    uint32_t form = 0;

    if (w == QY_DOUBLE) {
        code_op(c, qcg_dops[k]);
        return;
    }

    if (w == QY_SINGLE)
        form = cg_fltkind(c, right);
    else {
        form = code_ends_op(c, right, O_I_LD_V, 1 + VCELL) ? 1
               : code_ends_op(c, right, O_I_LD_U8, 2)      ? 2
                                                           : 0;
    }

    const uint8_t op = w == QY_SINGLE ? qcg_fops[form][k] : qcg_iops[form][k];

    if (!form) {
        code_op(c, op);
        return;
    }

    *right = op;
    c->iload = c->iloadend = nullptr; /* no longer a load */
}

QrtNum qcg_num(const QType t)
{
    switch (t) {
    case QY_BYTE:
        return QN_I8;

    case QY_SHORT:
        return QN_I16;

    case QY_LONG:
        return QN_I64;

    case QY_UBYTE:
        return QN_U8;

    case QY_USHORT:
        return QN_U16;

    case QY_UINT:
        return QN_U32;

    case QY_ULONG:
        return QN_U64;

    case QY_SINGLE:
        return QN_F32;

    case QY_DOUBLE:
        return QN_F64;

    default:
        return QN_I32;
    }
}

/* CALL QX.INT: operator op (a power, an int's wrapped division) of type t, wrapped or not. */
void qcg_xint(Code *const c, const QrtIop op, const QType t, const bool wrap)
{
    code_int(
        c,
        QX_INT | (uint32_t)op << QX_OP | (uint32_t)qcg_num(t) << QX_NUM | (uint32_t)wrap << QX_WRAP
    );
    code_call(c, K_QX);
}

/* X N.CONV: a from as a to, wrapped or not. */
void qcg_nconv(Code *const c, const QType from, const QType to, const bool wrap)
{
    code_xop(c, X_N_CONV);
    code_byte(c, qcg_num(from));
    code_byte(c, qcg_num(to) | (wrap ? QN_WRAP : 0));
}

/* Is t's value an int cell's int, compared and computed by the int ops: an integer to 16 bits,
   an int, a bool, an enum? A uint's bits are no int. */
bool qcg_icell(const QType t) { return !qast_wide(t) && t != QY_UINT; }

/* A compare of two values of type w, the right one's load at right (folded when it can be). */
void qcg_compare(Code *const c, const QType w, const QKind op, uint8_t *const right)
{
    const uint8_t m = qcg_mask(op);

    if (w == QY_LONG)
        code_opb(c, O_L_CMP, m);
    else if (w == QY_ULONG || w == QY_UINT) {
        code_xop(c, w == QY_UINT ? X_U_CMP : X_UL_CMP);
        code_byte(c, m);
    } else if (w == QY_SINGLE)
        cg_fcmp(c, m, right);
    else if (w == QY_DOUBLE || w == QY_STRING)
        code_opb(c, w == QY_DOUBLE ? O_D_CMP : O_S_CMP, m);
    else
        cg_icmp(c, m, right);
}

/* ---- jumps */

/* The code since the last segment's end fused; the next segment starts here. */
void qcg_flush(QCg *const g)
{
    Code *const c = g->c;

    if (!c->full && c->out != g->seg) {
        c->out = fuse_code(g->env->sy, g->seg, c->out);
        c->iload = c->iloadend = c->cmp = c->cmpend = c->shend = nullptr;
    }

    g->seg = c->out;
}

/* A Jump to statement s's line: its start (body 0), its end (BOFF), or past the JMP it starts
   with. */
void qcg_target(QCg *const g, const QStmt *const s, const uint32_t body, const JumpMode mode)
{
    code_dword(g->c, g->env->lines[s->line].at);
    code_dword(g->c, body);
    code_dword(g->c, mode);
}

/* Jump op op to qcg_target's. */
void qcg_to(
    QCg *const g,
    const uint8_t op,
    const QStmt *const s,
    const uint32_t body,
    const JumpMode mode
)
{
    code_op(g->c, op);
    qcg_target(g, s, body, mode);
}

/* CALL QJ when, its offset patched by qcg_land; null: the code is full, the line made again. */
uint8_t *qcg_qj(Code *const c, const QrtWhen when)
{
    uint8_t *const at = c->out + 1;
    code_op(c, O_I_LD_I32);
    code_dword(c, when);
    code_call(c, K_QJ);
    return c->full ? nullptr : at;
}

/* A jump within the line when: JMP, I.JF or I.JT to a label; with a string on the stack, QJ. */
QCgHop qcg_hop(QCg *const g, const QrtWhen when)
{
    static const uint8_t ops[] = {[QJ_ALWAYS] = O_JMP, [QJ_FALSE] = O_I_JF, [QJ_TRUE] = O_I_JT};

    if (g->strings) { /* a segment of its own: fusion can't move it */
        qcg_flush(g);
        uint8_t *const at = qcg_qj(g->c, when);
        qcg_flush(g);
        return (QCgHop){.qj = at};
    }

    const uint32_t body = ++g->hops;
    code_op(g->c, ops[when]);
    code_dword(g->c, g->line);
    code_dword(g->c, body);
    code_dword(g->c, TO_STMT);
    return (QCgHop){.body = body};
}

/* Hop h lands here: a label, its map entry's room taken from the code's; or the QJ's offset. */
void qcg_land(QCg *const g, const QCgHop h)
{
    qcg_flush(g);
    Code *const c = g->c;

    if (c->full)
        return;

    if (h.qj) {
        const int32_t from = (int32_t)(h.qj + 4 + 2 - c->buf);
        const int32_t rel = (int32_t)(c->out - c->buf) - from;
        store32(h.qj, (uint32_t)(rel * (1 << QJ_SHIFT)) | load32(h.qj));
        return;
    }

    g->labels = qast_room(g->mem, g->labels, g->nlabels, &g->caplabels, sizeof *g->labels);
    g->labels[g->nlabels++] = (QCgLabel){.body = h.body, .code = (uint32_t)(c->out - c->buf)};

    c->lim -= sizeof(Stmt);

    if (c->out > c->lim)
        c->full = true;
}

/* A loop's start here: a label's body; with strings on the stack, the code's offset. */
uint32_t qcg_top(QCg *const g)
{
    qcg_flush(g);

    if (g->strings)
        return (uint32_t)(g->c->out - g->c->buf);

    const uint32_t body = ++g->hops;
    qcg_land(g, (QCgHop){.body = body});

    return body;
}

/* A jump back to qcg_top's top when; with strings on the stack, a QJ. */
void qcg_back(QCg *const g, const QrtWhen when, const uint32_t top)
{
    static const uint8_t ops[] = {[QJ_ALWAYS] = O_JMP, [QJ_FALSE] = O_I_JF, [QJ_TRUE] = O_I_JT};

    Code *const c = g->c;

    if (!g->strings) {
        code_op(c, ops[when]);
        code_dword(c, g->line);
        code_dword(c, top);
        code_dword(c, TO_STMT);
        return;
    }

    qcg_flush(g);
    uint8_t *const at = qcg_qj(c, when);

    if (at) {
        const int32_t rel = (int32_t)top - (int32_t)(at + 4 + 2 - c->buf);
        store32(at, (uint32_t)(rel * (1 << QJ_SHIFT)) | load32(at));
    }

    qcg_flush(g);
}

/* Counter i's next, not past lim, by stp of sign by (0: known only when run): the flag to go on,
   i stepped then. It stops at an int's last value; comp/fuse.c makes it I.COUNT.UP or .DN. */
void qcg_count(Code *const c, QVar *const i, QVar *const lim, QVar *const stp, const int32_t by)
{
    qcg_load(c, i);
    qcg_load(c, lim);
    qcg_load(c, stp);
    code_xop(c, X_I_COUNT);
    code_byte(c, by > 0 ? COUNT_UP : by < 0 ? COUNT_DN : COUNT_BY);
    qcg_store(c, i);
}

/* v += by. */
void qcg_bump(Code *const c, QVar *const v, const uint32_t by)
{
    qcg_load(c, v);
    uint8_t *const right = c->out;
    code_int(c, by);
    qcg_arith(c, right, QY_INT, QA_ADD);
    qcg_store(c, v);
}
