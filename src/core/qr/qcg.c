/* Quickr's code generator: a checked statement to the VM's stack code.
 *
 * # How it works
 * Every operand is pushed, then its op: I.ADD, F.ADD, D.ADD by the operands' type, which sema
 * made one. A bool is an int, -1 true, as QBasic's compares give it. A variable is its id, a
 * parameter's through its ref (V.LD, V.ST); link_resolve (comp/select.c) places each.
 *
 * Control flow across lines is the VM's jumps, each a line and a body offset: 0 the statement,
 * BOFF the line's end, which threads to the next line. Within a line (and, or, a if c else b,
 * chains, in) it is CALL QJ by an offset (qr/qrt.c), patched once its target is known.
 *
 * A string literal loads from its line's text in the program store; an escape is CHR$ of its
 * byte, joined on.
 *
 * # Example
 *    while i < n          I.LD.V i, I.LD.V n, I.CMP <, I.JF (end while's line, BOFF)
 *    end while            JMP (while's line, 0)
 */
#include "qr/qcg.h"
#include "num/double.h"
#include "qr/qrt.h"
#include "vm/exec.h"

enum { TXT_MAX = 255 }; /* S.LD.TXT's length at most */

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

/* ---- jumps */

/* A Jump to statement s's line: its start (body 0), its end (BOFF), or past the JMP it starts
   with. */
static void qcg_target(
    Code *const c,
    const QCgEnv *const env,
    const QStmt *const s,
    const uint32_t body,
    const JumpMode mode
)
{
    code_dword(c, env->lines[s->tok->line].at);
    code_dword(c, body);
    code_dword(c, mode);
}

/* Jump op op to qcg_target's. */
static void qcg_to(
    Code *const c,
    const QCgEnv *const env,
    const uint8_t op,
    const QStmt *const s,
    const uint32_t body,
    const JumpMode mode
)
{
    code_op(c, op);
    qcg_target(c, env, s, body, mode);
}

/* CALL QJ when, its offset patched by qcg_land; null: the code is full, the line made again. */
static uint8_t *qcg_qj(Code *const c, const QrtWhen when)
{
    uint8_t *const at = c->out + 1;
    code_op(c, O_I_LD_I32);
    code_dword(c, when);
    code_call(c, K_QJ);
    return c->full ? nullptr : at;
}

/* The QJ whose operand is at at lands here. */
static void qcg_land(Code *const c, uint8_t *const at)
{
    if (!at || c->full)
        return;

    const int32_t from = (int32_t)(at + 4 + 2 - c->buf);
    const int32_t rel = (int32_t)(c->out - c->buf) - from;
    store32(at, (uint32_t)(rel * (1 << QJ_SHIFT)) | load32(at));
}

/* ---- values */

static void qcg_expr(Code *c, const QCgEnv *env, const QExpr *e);

/* A string constant's text: each literal's runs from its line, its escapes CHR$, joined. */
static void qcg_string(Code *const c, const QCgEnv *const env, const QPiece *s)
{
    bool first = true;

    if (!s) { /* "" */
        code_op(c, O_S_LD_TXT);
        code_byte(c, 0);
        code_dword(c, env->base);
        return;
    }

    for (; s; s = s->next) {
        const QTok *const t = s->lit;
        const QLine *const ln = &env->lines[t->line];
        const Addr text = env->base + ln->at + HDR + (uint32_t)(t->text - ln->text);
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
static void qcg_const(Code *const c, const QCgEnv *const env, const QExpr *const e, const QType as)
{
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
        qcg_string(c, env, e->val.s);
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
static void qcg_as(Code *const c, const QCgEnv *const env, const QExpr *const e, const QType w)
{
    if (e->kind == QE_CONST) {
        qcg_const(c, env, e, w);
        return;
    }

    qcg_expr(c, env, e);
    qcg_convert(c, e->type, w);
}

static void qcg_compare(Code *const c, const QType w, const QKind op)
{
    code_opb(c, qcg_pick(w, O_I_CMP, O_F_CMP, O_D_CMP, O_S_CMP), qcg_mask(op));
}

static void qcg_binary(Code *const c, const QCgEnv *const env, const QExpr *const e)
{
    const QType w = e->work;
    qcg_as(c, env, e->a, w);
    qcg_as(c, env, e->b, w);

    switch (e->op) {
    case QK_PLUS:
        code_op(c, qcg_pick(w, O_I_ADD, O_F_ADD, O_D_ADD, O_S_CAT));
        return;

    case QK_MINUS:
        code_op(c, qcg_pick(w, O_I_SUB, O_F_SUB, O_D_SUB, 0));
        return;

    case QK_STAR:
        code_op(c, qcg_pick(w, O_I_MUL, O_F_MUL, O_D_MUL, 0));
        return;

    case QK_SLASH:
        code_op(c, w == QY_SINGLE ? O_F_DIV : O_D_DIV);
        return;

    case QK_POW:
        if (w == QY_DOUBLE)
            code_op(c, O_D_POW);
        else
            code_call(c, w == QY_INT ? K_IPOW : K_FPOW);

        return;

    case QK_DIV:
        code_op(c, O_I_DIV);
        return;

    case QK_MOD:
        code_op(c, O_I_MOD);
        return;

    case QK_AMP:
        code_op(c, O_I_AND);
        return;

    case QK_BAR:
        code_op(c, O_I_OR);
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

static void qcg_unary(Code *const c, const QCgEnv *const env, const QExpr *const e)
{
    qcg_expr(c, env, e->a);

    if (e->op == QK_MINUS)
        code_op(c, qcg_pick(e->type, O_I_NEG, O_F_NEG, O_D_NEG, 0));
    else
        code_op(c, O_I_NOT);
}

/* Chain item k as the pair's type w: a middle's from its temporary. */
static void qcg_item(
    Code *const c,
    const QCgEnv *const env,
    const QExpr *const e,
    const uint32_t k,
    const QType w
)
{
    if (!e->temps[k]) {
        qcg_as(c, env, e->items[k], w);
        return;
    }

    qcg_load(c, e->temps[k]);
    qcg_convert(c, e->items[k]->type, w);
}

/* a < b <= c from pair k on: the pair compared, a middle stored once; false, past the rest. */
static void qcg_chain(
    Code *const c,
    const QCgEnv *const env,
    const QExpr *const e,
    const uint32_t k
)
{
    const QType w = e->works[k];
    qcg_item(c, env, e, k, w);

    if (e->temps[k + 1]) {
        qcg_expr(c, env, e->items[k + 1]);
        qcg_store(c, e->temps[k + 1]);
    }

    qcg_item(c, env, e, k + 1, w);
    qcg_compare(c, w, e->ops[k]);

    if (k + 2 >= e->n)
        return;

    uint8_t *const no = qcg_qj(c, QJ_FALSE);
    qcg_chain(c, env, e, k + 1);
    uint8_t *const over = qcg_qj(c, QJ_ALWAYS);
    qcg_land(c, no);
    code_int(c, 0);
    qcg_land(c, over);
}

/* a and b, a or b. */
static void qcg_logic(Code *const c, const QCgEnv *const env, const QExpr *const e)
{
    const bool and = e->kind == QE_AND;
    qcg_expr(c, env, e->a);
    uint8_t *const skip = qcg_qj(c, and? QJ_FALSE : QJ_TRUE);
    qcg_expr(c, env, e->b);
    uint8_t *const over = qcg_qj(c, QJ_ALWAYS);
    qcg_land(c, skip);
    code_int(c, and? 0 : 0xFFFFFFFFu);
    qcg_land(c, over);
}

static void qcg_cond(Code *const c, const QCgEnv *const env, const QExpr *const e)
{
    qcg_expr(c, env, e->c);
    uint8_t *const other = qcg_qj(c, QJ_FALSE);
    qcg_expr(c, env, e->a);
    uint8_t *const over = qcg_qj(c, QJ_ALWAYS);
    qcg_land(c, other);
    qcg_expr(c, env, e->b);
    qcg_land(c, over);
}

/* x in a..b: a <= x and x < b, x once. */
static void qcg_in(Code *const c, const QCgEnv *const env, const QExpr *const e)
{
    const QType w = e->work;

    if (e->var) {
        qcg_expr(c, env, e->a);
        qcg_store(c, e->var);
    }

    const QExpr *const x = e->a;

    for (uint32_t side = 0; side < 2; side++) {
        if (e->var)
            qcg_load(c, e->var);
        else
            qcg_as(c, env, x, w);

        qcg_as(c, env, side ? e->c : e->b, w);
        qcg_compare(c, w, side ? QK_LT : QK_GE);
    }

    /* the first compare's flag is under the second's: and of -1 and 0 */
    code_op(c, O_I_AND);

    if (e->neg)
        code_op(c, O_I_NOT);
}

/* print(a, b): each spelled, a space between, a new line. */
static void qcg_print(Code *const c, const QCgEnv *const env, const QExpr *const e)
{
    for (uint32_t i = 0; i < e->nargv; i++) {
        const QExpr *const a = e->argv[i];

        if (i) {
            code_int(c, QRT_SPACE);
            code_call(c, K_QPR);
        }

        qcg_expr(c, env, a);
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
static void qcg_call(Code *const c, const QCgEnv *const env, const QExpr *const e)
{
    if (e->builtin == QB_PRINT) {
        qcg_print(c, env, e);
        return;
    }

    const QFn *const f = e->fn;

    for (uint32_t k = 0; k < f->npar; k++) {
        const QParam *const p = &f->params[k];
        const QExpr *const a = e->argv[k];
        const bool cell = a->kind == QE_NAME && a->type == p->var->type;

        if (p->mut || cell) {
            code_int(c, 0);
            code_opid(c, qcg_par(a->var) ? O_V_REF : O_V_CELL, a->var->id, false);
            continue;
        }

        qcg_expr(c, env, a);
        code_opb(c, O_BYVAL, a->type == QY_STRING);
    }

    code_op(c, O_PCALL); /* the procedure, its locals' cells, its parameters (link's), its body */
    code_dword(c, f->proc);
    code_dword(c, 0);
    code_word(c, 0);
    qcg_target(c, env, f->def, 0, PAST_JMP);
}

static void qcg_expr(Code *const c, const QCgEnv *const env, const QExpr *const e)
{
    switch (e->kind) {
    case QE_CONST:
        qcg_const(c, env, e, e->type);
        return;

    case QE_NAME:
        qcg_load(c, e->var);
        return;

    case QE_UNARY:
        qcg_unary(c, env, e);
        return;

    case QE_BINARY:
        qcg_binary(c, env, e);
        return;

    case QE_CHAIN:
        qcg_chain(c, env, e, 0);
        return;

    case QE_AND:
    case QE_OR:
        qcg_logic(c, env, e);
        return;

    case QE_COND:
        qcg_cond(c, env, e);
        return;

    case QE_IN:
        qcg_in(c, env, e);
        return;

    case QE_CALL:
        qcg_call(c, env, e);
        return;

    case QE_CONV:
        qcg_expr(c, env, e->a);
        qcg_convert(c, e->a->type, e->type);
        return;
    }
}

/* ---- statements */

/* A variable's zero: 0, 0.0, false, "". */
static void qcg_zero(Code *const c, const QCgEnv *const env, const QType t)
{
    const QExpr zero = {.kind = QE_CONST, .type = t};

    qcg_const(c, env, &zero, t);
}

/* A clause's target from the one before it: past an elseif's or else's JMP, or the end if. */
static void qcg_clause(
    Code *const c,
    const QCgEnv *const env,
    const uint8_t op,
    const QStmt *const next
)
{
    const JumpMode mode = next->kind == QS_ENDIF ? TO_STMT : PAST_JMP;
    qcg_to(c, env, op, next, 0, mode);
}

/* The end if of the if whose clause s is. */
static const QStmt *qcg_endif(const QStmt *s)
{
    while (s->kind != QS_ENDIF)
        s = s->link;

    return s;
}

/* A for's test: the counter not past its limit, by its step's sign. */
static void qcg_fortest(Code *const c, const QCgEnv *const env, const QStmt *const s)
{
    const QKind ops[2] = {QK_LT, QK_GT};

    uint8_t *down = nullptr, *over = nullptr;

    for (uint32_t dir = 0; dir < 2; dir++) {
        if (!s->stp && (dir == 0) != (s->by > 0))
            continue;

        if (s->stp && !dir) {
            qcg_load(c, s->stp);
            code_int(c, 0);
            qcg_compare(c, QY_INT, QK_GT);
            down = qcg_qj(c, QJ_FALSE);
        }

        if (s->stp && dir)
            qcg_land(c, down);

        qcg_load(c, s->var);

        if (s->lim)
            qcg_load(c, s->lim);
        else
            qcg_const(c, env, s->to, QY_INT);

        qcg_compare(c, QY_INT, ops[dir]);

        if (s->stp && !dir)
            over = qcg_qj(c, QJ_ALWAYS);
    }

    qcg_land(c, over);
}

/* A statement a one-line if may take, or one that is no block's. */
static void qcg_simple(Code *const c, const QCgEnv *const env, const QStmt *const s)
{
    switch (s->kind) {
    case QS_DECL:
        if (s->op == QK_CONST)
            return;

        for (const QDecl *d = s->decls; d; d = d->next) {
            if (d->value)
                qcg_expr(c, env, d->value);
            else
                qcg_zero(c, env, d->var->type);

            qcg_store(c, d->var);
        }

        return;

    case QS_ASSIGN:
        qcg_expr(c, env, s->e);
        qcg_store(c, s->target->var);
        return;

    case QS_CALL:
        qcg_expr(c, env, s->e);

        if (s->e->type != QY_VOID)
            code_op(c, O_DROP);

        return;

    case QS_BREAK:
        qcg_to(c, env, O_JMP, s->link->link, BOFF, TO_STMT);
        return;

    case QS_CONTINUE: {
        const QStmt *const loop = s->link;

        if (loop->kind == QS_WHILE)
            qcg_to(c, env, O_JMP, loop, 0, TO_STMT);
        else if (loop->kind == QS_FOR)
            qcg_to(c, env, O_JMP, loop->link, 0, TO_STMT);
        else
            qcg_to(c, env, O_JMP, loop, BOFF, TO_STMT);

        return;
    }

    case QS_RETURN:
        if (s->e) {
            qcg_expr(c, env, s->e);
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

void qcg_stmt(Code *const c, const QCgEnv *const env, const QStmt *const s)
{
    if (env->dbg && qcg_stops(s))
        code_op(c, O_DBG);

    switch (s->kind) {
    case QS_IF:
        qcg_expr(c, env, s->e);

        if (s->oneline) {
            qcg_to(c, env, O_I_JF, s, BOFF, TO_STMT);
            qcg_simple(c, env, s->body);
        } else
            qcg_clause(c, env, O_I_JF, s->link);

        return;

    case QS_ELSEIF:
        qcg_to(c, env, O_JMP, qcg_endif(s), BOFF, TO_STMT);
        qcg_expr(c, env, s->e);
        qcg_clause(c, env, O_I_JF, s->link);
        return;

    case QS_ELSE:
        qcg_to(c, env, O_JMP, qcg_endif(s), BOFF, TO_STMT);
        return;

    case QS_WHILE:
        qcg_expr(c, env, s->e);
        qcg_to(c, env, O_I_JF, s->link, BOFF, TO_STMT);
        return;

    case QS_ENDWHILE:
        qcg_to(c, env, O_JMP, s->link, 0, TO_STMT);
        return;

    case QS_ENDDO:
        qcg_to(c, env, O_JMP, s->link, BOFF, TO_STMT);
        return;

    case QS_FOR:
        qcg_expr(c, env, s->e);
        qcg_store(c, s->var);

        if (s->lim) {
            qcg_expr(c, env, s->to);
            qcg_store(c, s->lim);
        }

        if (s->stp) {
            qcg_expr(c, env, s->step);
            qcg_store(c, s->stp);
        }

        qcg_fortest(c, env, s);
        qcg_to(c, env, O_I_JF, s->link, BOFF, TO_STMT);
        return;

    case QS_ENDFOR: {
        const QStmt *const f = s->link;
        qcg_load(c, f->var);

        if (f->stp)
            qcg_load(c, f->stp);
        else
            code_int(c, (uint32_t)f->by);

        code_op(c, O_I_ADD);
        qcg_store(c, f->var);
        qcg_fortest(c, env, f);
        qcg_to(c, env, O_I_JT, f, BOFF, TO_STMT);
        return;
    }

    case QS_FN:
        qcg_to(c, env, O_JMP, s->fn->end, BOFF, TO_STMT);
        return;

    case QS_ENDFN:
        code_op(c, O_PRET);
        code_dword(c, s->fn->proc);
        code_dword(c, 0);
        return;

    default:
        qcg_simple(c, env, s);
    }
}
