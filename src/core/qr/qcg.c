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
 * A for loop is QBasic's FOR and NEXT: its limit made inclusive once the loop is entered, so its
 * end for fuses to I.NEXT.UP or I.NEXT.DN; a step whose sign is known only when run, I.PAST.
 *
 * A string literal loads from its line's text in the program store; an escape is CHR$ of its
 * byte, joined on.
 *
 * # Example
 *    while i < n          I.LD.V i, I.JFC.V n < (end while's line, BOFF)
 *    end while            JMP (while's line, 0)
 */
#include "qr/qcg.h"
#include "comp/codegen.h"
#include "comp/fuse.h"
#include "num/double.h"
#include "qr/qrt.h"
#include "vm/exec.h"

enum { TXT_MAX = 255 }; /* S.LD.TXT's length at most */

/* The int and single operators a right operand folds into: by form (plain, a variable's load, a
   literal's), then operator. */
enum { QA_ADD, QA_SUB, QA_MUL, QA_DIV, QA_AND, QA_OR };
static const uint8_t qcg_iops[3][6] = {
    {O_I_ADD, O_I_SUB, O_I_MUL, O_I_DIV, O_I_AND, O_I_OR},
    {O_I_ADD_V, O_I_SUB_V, O_I_MUL_V, O_I_DIV_V, O_I_AND_V, O_I_OR_V},
    {O_I_ADD_U8, O_I_SUB_U8, O_I_MUL_U8, O_I_DIV_U8, O_I_AND_U8, O_I_OR_U8},
};
static const uint8_t qcg_fops[3][4] = {
    {O_F_ADD, O_F_SUB, O_F_MUL, O_F_DIV},
    {O_F_ADD_V, O_F_SUB_V, O_F_MUL_V, O_F_DIV_V},
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
static uint8_t qcg_pick(
    const QType t,
    const uint8_t i,
    const uint8_t f,
    const uint8_t d,
    const uint8_t s
)
{
    switch (t) {
    case QY_SINGLE:
        return f;

    case QY_DOUBLE:
        return d;

    case QY_STRING:
        return s;

    default:
        return i;
    }
}

static bool qcg_par(const QVar *const v) { return v->kind == QV_PARAM || v->kind == QV_MUT; }

static void qcg_load(Code *const c, const QVar *const v)
{
    code_opid(c, qcg_pick(v->type, O_I_LD_V, O_F_LD_V, O_D_LD_V, O_S_LD_V), v->id, qcg_par(v));
}

static void qcg_store(Code *const c, const QVar *const v)
{
    code_opid(c, qcg_pick(v->type, O_I_ST_V, O_F_ST_V, O_D_ST_V, O_S_ST_V), v->id, qcg_par(v));
}

/* Is the code from right to the end one op, op, of size bytes? */
static bool qcg_right(
    const Code *const c,
    const uint8_t *const right,
    const uint8_t op,
    const uint32_t size
)
{
    return (uint32_t)(c->out - right) == size && *right == op;
}

/* Operator k (QA_*) of type w after its operands: the right one's load, at right, folded in when
   one variable's or literal's. */
static void qcg_arith(Code *const c, uint8_t *const right, const QType w, const uint32_t k)
{
    uint32_t form = 0;

    if (w == QY_DOUBLE) {
        code_op(c, qcg_dops[k]);
        return;
    }

    if (w == QY_SINGLE) {
        form = qcg_right(c, right, O_F_LD_V, 1 + VCELL) ? 1
               : qcg_right(c, right, O_F_LD_F32, 5)     ? 2
                                                        : 0;
    } else {
        form = qcg_right(c, right, O_I_LD_V, 1 + VCELL) ? 1
               : qcg_right(c, right, O_I_LD_U8, 2)      ? 2
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

/* A compare of two values of type w, the right one's load at right (folded when it can be). */
static void qcg_compare(Code *const c, const QType w, const QKind op, uint8_t *const right)
{
    const uint8_t m = qcg_mask(op);

    if (w == QY_SINGLE)
        cg_fcmp(c, m, right);
    else if (w == QY_DOUBLE || w == QY_STRING)
        code_opb(c, w == QY_DOUBLE ? O_D_CMP : O_S_CMP, m);
    else
        cg_icmp(c, m, right);
}

/* ---- jumps */

/* The code since the last segment's end fused; the next segment starts here. */
static void qcg_flush(QCg *const g)
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
static void qcg_target(QCg *const g, const QStmt *const s, const uint32_t body, const JumpMode mode)
{
    code_dword(g->c, g->env->lines[s->tok->line].at);
    code_dword(g->c, body);
    code_dword(g->c, mode);
}

/* Jump op op to qcg_target's. */
static void qcg_to(
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

/* A jump forward within the line, landed by qcg_land: a label's body, or a QJ's operand. */
typedef struct {
    uint32_t body;
    uint8_t *qj;
} QCgHop;

/* CALL QJ when, its offset patched by qcg_land; null: the code is full, the line made again. */
static uint8_t *qcg_qj(Code *const c, const QrtWhen when)
{
    uint8_t *const at = c->out + 1;
    code_op(c, O_I_LD_I32);
    code_dword(c, when);
    code_call(c, K_QJ);
    return c->full ? nullptr : at;
}

/* A jump within the line when: JMP, I.JF or I.JT to a label; with a string on the stack, QJ. */
static QCgHop qcg_hop(QCg *const g, const QrtWhen when)
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
static void qcg_land(QCg *const g, const QCgHop h)
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

/* ---- values */

static void qcg_expr(QCg *g, const QExpr *e);

/* A string constant's text: each literal's runs from its line, its escapes CHR$, joined. */
static void qcg_string(QCg *const g, const QPiece *s)
{
    Code *const c = g->c;
    bool first = true;

    if (!s) { /* "" */
        code_op(c, O_S_LD_TXT);
        code_byte(c, 0);
        code_dword(c, g->env->base);
        return;
    }

    for (; s; s = s->next) {
        const QTok *const t = s->lit;
        const QLine *const ln = &g->env->lines[t->line];
        const Addr text = g->env->base + ln->at + HDR + (uint32_t)(t->text - ln->text);
        uint32_t i = 0;

        do {
            uint32_t n = 0;

            while (i + n < t->len && t->text[i + n] != '\\' && n < TXT_MAX)
                n++;

            if (n || !t->len) {
                code_op(c, O_S_LD_TXT);
                code_byte(c, (uint8_t)n);
                code_dword(c, text + i);
            } else {
                const char e = t->text[i + 1];
                code_int(c, e == 'n' ? '\n' : e == 't' ? '\t' : (uint8_t)e);
                code_call(c, K_FN_CHRS);
                n = 2;
            }

            if (!first)
                code_op(c, O_S_CAT);

            first = false;
            i += n;
        } while (i < t->len);
    }
}

/* A constant as type as: an int's, a float's, a bool's -1 or 0, a string's. */
static void qcg_const(QCg *const g, const QExpr *const e, const QType as)
{
    Code *const c = g->c;
    const double d = e->type == QY_INT ? (double)e->val.i : e->val.d;

    switch (as) {
    case QY_SINGLE:
        code_op(c, O_F_LD_F32);
        code_dword(c, (Cell){.f = (float)d}.u);

        return;

    case QY_DOUBLE: {
        const uint64_t raw = (Cell){.d = d}.raw;

        code_op(c, O_D_LD_K);
        code_dword(c, (uint32_t)raw);
        code_dword(c, (uint32_t)(raw >> 32));
        return;
    }

    case QY_STRING:
        qcg_string(g, e->val.s);
        return;

    case QY_BOOL:
        code_int(c, e->val.i ? 0xFFFFFFFFu : 0);
        return;

    default:
        code_int(c, (uint32_t)e->val.i);
    }
}

/* The value on top, of type from, as type to; trunc: a float to an int toward 0. */
static void qcg_convert(Code *const c, const QType from, const QType to)
{
    if (from == to)
        return;

    if (to == QY_STRING) {
        const QrtKind k = from == QY_INT      ? QRT_INT
                          : from == QY_SINGLE ? QRT_SINGLE
                          : from == QY_DOUBLE ? QRT_DOUBLE
                                              : QRT_BOOL;
        code_int(c, k);
        code_call(c, K_QSTR);
        return;
    }

    if (to == QY_INT) { /* int(x): truncated */
        if (from == QY_SINGLE)
            code_op(c, O_F_TOD);

        code_opb(c, O_D_FN, DF_FIX);
        code_op(c, O_D_TOI);
        return;
    }

    if (to == QY_SINGLE)
        code_op(c, from == QY_INT ? O_I_TOF : O_D_TOF);
    else
        code_op(c, from == QY_INT ? O_I_TOD : O_F_TOD);
}

/* e's value as type w: a constant made so, else converted. */
static void qcg_as(QCg *const g, const QExpr *const e, const QType w)
{
    if (e->kind == QE_CONST) {
        qcg_const(g, e, w);
        return;
    }

    qcg_expr(g, e);
    qcg_convert(g->c, e->type, w);
}

/* e's value pushed, held under what follows: a string counted (qcg_hop). */
static void qcg_hold(QCg *const g, const QType t, const int32_t by)
{
    if (t == QY_STRING)
        g->strings += (uint32_t)by;
}

static void qcg_binary(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QType w = e->work;
    qcg_as(g, e->a, w);
    qcg_hold(g, w, 1);
    uint8_t *const right = c->out;
    qcg_as(g, e->b, w);
    qcg_hold(g, w, -1);

    switch (e->op) {
    case QK_PLUS:
        if (w == QY_STRING)
            code_op(c, O_S_CAT);
        else
            qcg_arith(c, right, w, QA_ADD);

        return;

    case QK_MINUS:
        qcg_arith(c, right, w, QA_SUB);
        return;

    case QK_STAR:
        qcg_arith(c, right, w, QA_MUL);
        return;

    case QK_SLASH:
        qcg_arith(c, right, w, QA_DIV);
        return;

    case QK_POW:
        if (w == QY_DOUBLE)
            code_op(c, O_D_POW);
        else
            code_call(c, w == QY_INT ? K_IPOW : K_FPOW);

        return;

    case QK_DIV:
        qcg_arith(c, right, QY_INT, QA_DIV);
        return;

    case QK_MOD:
        code_op(c, O_I_MOD);
        return;

    case QK_AMP:
        qcg_arith(c, right, QY_INT, QA_AND);
        return;

    case QK_BAR:
        qcg_arith(c, right, QY_INT, QA_OR);
        return;

    case QK_SHL:
        code_call(c, K_QSHL);
        return;

    case QK_SHR:
        code_call(c, K_QSHR);
        return;

    default: /* ^, xor */
        code_op(c, O_I_XOR);
    }
}

static void qcg_unary(QCg *const g, const QExpr *const e)
{
    qcg_expr(g, e->a);

    if (e->op == QK_MINUS)
        code_op(g->c, qcg_pick(e->type, O_I_NEG, O_F_NEG, O_D_NEG, 0));
    else
        code_op(g->c, O_I_NOT);
}

/* Chain item k as the pair's type w: a middle's from its temporary. */
static void qcg_item(QCg *const g, const QExpr *const e, const uint32_t k, const QType w)
{
    if (!e->temps[k]) {
        qcg_as(g, e->items[k], w);
        return;
    }

    qcg_load(g->c, e->temps[k]);
    qcg_convert(g->c, e->items[k]->type, w);
}

/* a < b <= c from pair k on: the pair compared, a middle stored once; false, past the rest. */
static void qcg_chain(QCg *const g, const QExpr *const e, const uint32_t k)
{
    const QType w = e->works[k];
    qcg_item(g, e, k, w);
    qcg_hold(g, w, 1);

    if (e->temps[k + 1]) {
        qcg_expr(g, e->items[k + 1]);
        qcg_store(g->c, e->temps[k + 1]);
    }

    uint8_t *const right = g->c->out;
    qcg_item(g, e, k + 1, w);
    qcg_hold(g, w, -1);
    qcg_compare(g->c, w, e->ops[k], right);

    if (k + 2 >= e->n)
        return;

    const QCgHop no = qcg_hop(g, QJ_FALSE);
    qcg_chain(g, e, k + 1);
    const QCgHop over = qcg_hop(g, QJ_ALWAYS);
    qcg_land(g, no);
    code_int(g->c, 0);
    qcg_land(g, over);
}

/* a and b, a or b. */
static void qcg_logic(QCg *const g, const QExpr *const e)
{
    const bool and = e->kind == QE_AND;
    qcg_expr(g, e->a);
    const QCgHop skip = qcg_hop(g, and? QJ_FALSE : QJ_TRUE);
    qcg_expr(g, e->b);
    const QCgHop over = qcg_hop(g, QJ_ALWAYS);
    qcg_land(g, skip);
    code_int(g->c, and? 0 : 0xFFFFFFFFu);
    qcg_land(g, over);
}

/* a if c else b: a's value on the stack at the jump past b. */
static void qcg_cond(QCg *const g, const QExpr *const e)
{
    qcg_expr(g, e->c);
    const QCgHop other = qcg_hop(g, QJ_FALSE);
    qcg_expr(g, e->a);
    qcg_hold(g, e->type, 1);
    const QCgHop over = qcg_hop(g, QJ_ALWAYS);
    qcg_hold(g, e->type, -1);
    qcg_land(g, other);
    qcg_expr(g, e->b);
    qcg_land(g, over);
}

/* x in a..b: a <= x and x < b, x once. */
static void qcg_in(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QType w = e->work;

    if (e->var) {
        qcg_expr(g, e->a);
        qcg_store(c, e->var);
    }

    for (uint32_t side = 0; side < 2; side++) {
        if (e->var)
            qcg_load(c, e->var);
        else
            qcg_as(g, e->a, w);

        qcg_hold(g, w, 1);
        uint8_t *const right = c->out;
        qcg_as(g, side ? e->c : e->b, w);
        qcg_hold(g, w, -1);
        qcg_compare(c, w, side ? QK_LT : QK_GE, right);
    }

    /* the first compare's flag is under the second's: and of -1 and 0 */
    code_op(c, O_I_AND);

    if (e->neg)
        code_op(c, O_I_NOT);
}

/* print(a, b): each spelled, a space between, a new line. */
static void qcg_print(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;

    for (uint32_t i = 0; i < e->nargv; i++) {
        const QExpr *const a = e->argv[i];

        if (i) {
            code_int(c, QRT_SPACE);
            code_call(c, K_QPR);
        }

        qcg_expr(g, a);
        const QType t = a->type;
        code_int(
            c,
            t == QY_INT      ? QRT_INT
            : t == QY_SINGLE ? QRT_SINGLE
            : t == QY_DOUBLE ? QRT_DOUBLE
            : t == QY_BOOL   ? QRT_BOOL
                             : QRT_STRING
        );
        code_call(c, K_QPR);
    }

    code_call(c, K_PNL);
}

/* A fn's call: each argument a value cell and its ref; a var by its own cell. */
static void qcg_call(QCg *const g, const QExpr *const e)
{
    if (e->builtin == QB_PRINT) {
        qcg_print(g, e);
        return;
    }

    Code *const c = g->c;
    const QFn *const f = e->fn;
    uint32_t held = 0;

    for (uint32_t k = 0; k < f->npar; k++) {
        const QParam *const p = &f->params[k];
        const QExpr *const a = e->argv[k];
        const bool cell = a->kind == QE_NAME && a->type == p->var->type;

        if (p->mut || cell) {
            code_int(c, 0);
            code_opid(c, qcg_par(a->var) ? O_V_REF : O_V_CELL, a->var->id, false);
            continue;
        }

        qcg_expr(g, a);
        code_opb(c, O_BYVAL, a->type == QY_STRING);
        held += a->type == QY_STRING;
        qcg_hold(g, a->type, 1);
    }

    g->strings -= held;
    code_op(c, O_PCALL); /* the procedure, its locals' cells, its parameters (link's), its body */
    code_dword(c, f->proc);
    code_dword(c, 0);
    code_word(c, 0);
    qcg_target(g, f->def, 0, PAST_JMP);
}

static void qcg_expr(QCg *const g, const QExpr *const e)
{
    switch (e->kind) {
    case QE_CONST:
        qcg_const(g, e, e->type);
        return;

    case QE_NAME:
        qcg_load(g->c, e->var);
        return;

    case QE_UNARY:
        qcg_unary(g, e);
        return;

    case QE_BINARY:
        qcg_binary(g, e);
        return;

    case QE_CHAIN:
        qcg_chain(g, e, 0);
        return;

    case QE_AND:
    case QE_OR:
        qcg_logic(g, e);
        return;

    case QE_COND:
        qcg_cond(g, e);
        return;

    case QE_IN:
        qcg_in(g, e);
        return;

    case QE_CALL:
        qcg_call(g, e);
        return;

    case QE_CONV:
        qcg_expr(g, e->a);
        qcg_convert(g->c, e->a->type, e->type);
        return;
    }
}

/* ---- statements */

/* A variable's zero: 0, 0.0, false, "". */
static void qcg_zero(QCg *const g, const QType t)
{
    const QExpr zero = {.kind = QE_CONST, .type = t};

    qcg_const(g, &zero, t);
}

/* A clause's target from the one before it: past an elseif's or else's JMP, or the end if. */
static void qcg_clause(QCg *const g, const uint8_t op, const QStmt *const next)
{
    const JumpMode mode = next->kind == QS_ENDIF ? TO_STMT : PAST_JMP;
    qcg_to(g, op, next, 0, mode);
}

/* The end if of the if whose clause s is. */
static const QStmt *qcg_endif(const QStmt *s)
{
    while (s->kind != QS_ENDIF)
        s = s->link;

    return s;
}

/* The flag of for s's counter past its limit, its step's sign known (by) or not (I.PAST).
   entered: the limit inclusive, as end for makes it; else the range's end, past when reached. */
static void qcg_past(QCg *const g, const QStmt *const s, const bool entered)
{
    Code *const c = g->c;
    qcg_load(c, s->var);

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
    code_op(c, O_I_PAST);

    if (entered)
        return;

    qcg_load(c, s->var);
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

/* A statement a one-line if may take, or one that is no block's. */
static void qcg_simple(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;

    switch (s->kind) {
    case QS_DECL:
        if (s->op == QK_CONST)
            return;

        for (const QDecl *d = s->decls; d; d = d->next) {
            if (d->value)
                qcg_expr(g, d->value);
            else
                qcg_zero(g, d->var->type);

            qcg_store(c, d->var);
        }

        return;

    case QS_ASSIGN:
        qcg_expr(g, s->e);
        qcg_store(c, s->target->var);
        return;

    case QS_CALL:
        qcg_expr(g, s->e);

        if (s->e->type != QY_VOID)
            code_op(c, O_DROP);

        return;

    case QS_BREAK:
        qcg_to(g, O_JMP, s->link->link, BOFF, TO_STMT);
        return;

    case QS_CONTINUE: {
        const QStmt *const loop = s->link;

        if (loop->kind == QS_WHILE)
            qcg_to(g, O_JMP, loop, 0, TO_STMT);
        else if (loop->kind == QS_FOR)
            qcg_to(g, O_JMP, loop->link, 0, TO_STMT);
        else
            qcg_to(g, O_JMP, loop, BOFF, TO_STMT);

        return;
    }

    case QS_RETURN:
        if (s->e) {
            qcg_expr(g, s->e);
            qcg_store(c, s->fn->res);
        }

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
    case QS_ENDFOR:
    case QS_BREAK:
    case QS_CONTINUE:
    case QS_RETURN:
        return true;

    default:
        return false;
    }
}

/* Statement s's code, unfused. */
static void qcg_body(QCg *const g, const QStmt *const s)
{
    Code *const c = g->c;

    switch (s->kind) {
    case QS_IF:
        qcg_expr(g, s->e);

        if (s->oneline) {
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
        qcg_to(g, O_JMP, s->link, BOFF, TO_STMT);
        return;

    case QS_FOR:
        qcg_expr(g, s->e);
        qcg_store(c, s->var);
        qcg_expr(g, s->to);
        qcg_store(c, s->lim);

        if (s->by)
            code_int(c, (uint32_t)s->by);
        else
            qcg_expr(g, s->step);

        qcg_store(c, s->stp);
        qcg_past(g, s, false);
        qcg_to(g, O_I_JT, s->link, BOFF, TO_STMT);
        qcg_inclusive(g, s);
        return;

    case QS_ENDFOR: { /* NEXT's six ops (comp/fuse.c): I.NEXT.UP or I.NEXT.DN */
        const QStmt *const f = s->link;
        qcg_load(c, f->var);
        code_opid(c, O_I_STEP_V, f->stp->id, false);
        qcg_store(c, f->var);
        qcg_past(g, f, true);
        qcg_to(g, O_I_JF, f, BOFF, TO_STMT);
        return;
    }

    case QS_FN:
        qcg_to(g, O_JMP, s->fn->end, BOFF, TO_STMT);
        return;

    case QS_ENDFN:
        code_op(c, O_PRET);
        code_dword(c, s->fn->proc);
        code_dword(c, 0);
        return;

    default:
        qcg_simple(g, s);
    }
}

void qcg_stmt(QCg *const g, const QStmt *const s)
{
    g->seg = g->c->out;
    g->nlabels = g->hops = g->strings = 0;

    if (g->env->dbg && qcg_stops(s))
        code_op(g->c, O_DBG);

    qcg_body(g, s);
    qcg_flush(g);
}
