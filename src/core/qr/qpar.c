/* Quickr's parser: tokens to statements, one a line, and their expressions' trees.
 *
 * # How it works
 * Recursive descent, one function a precedence level, lowest first: a if c else b, or, xor, and,
 * not, comparisons (chained, and in a..b), | ^ & << >>, + -, * / div mod, unary - ~, **. A
 * statement is parsed to its QK_NL; an error longjmps to the statement's start, which skips to
 * the QK_NL, so one bad statement makes one error.
 *
 * Statements form one flat list in source order. A stack of the open blocks links each clause to
 * the next (if, elseif, else, end if), each loop to its end and back, each break and continue to
 * its loop. A block's opener is linked before its condition is parsed, so a bad condition still
 * pairs with its end.
 *
 * # Example
 *    if x < 0 then        QS_IF.link = the else
 *    else                 QS_ELSE.link = the end if
 *    end if               QS_ENDIF.link = the if
 */
#include "qr/qpar.h"
#include <setjmp.h>

/* An open block: its opener, its last clause. */
typedef struct {
    QStmt *open;
    QStmt *clause;
} QOpen;

typedef struct {
    QProg *prog;
    QMem *mem;
    const QTok *t; /* the next token */
    jmp_buf fail;
    QStmt *last;
    QFn *lastfn;
    QOpen *open;
    uint32_t nopen, capopen;
} QPar;

static QExpr *qpar_expr(QPar *P);

/* ---- tokens */

[[noreturn]]
static void qpar_fail(QPar *const P, const QTok *const t, const char *const msg)
{
    qast_error(&P->prog->errs, t, msg, t->text, t->len);
    longjmp(P->fail, 1);
}

/* The next token not what was wanted: "expected {want}". */
[[noreturn]]
static void qpar_expected(QPar *const P, const char *const want)
{
    const QTok *const t = P->t;

    if (t->kind == QK_BAD)
        longjmp(P->fail, 1); /* the lexer said why */

    const char *const got =
        t->kind == QK_NL || t->kind == QK_EOF ? qast_spelling(t->kind) : nullptr;
    char msg[96] = "expected ";
    const uint32_t n = (uint32_t)strlen(want);
    memcpy(msg + 9, want, n);
    memcpy(msg + 9 + n, got ? ", not the line's end" : ", not {}", got ? 21 : 9);
    qpar_fail(P, t, msg);
}

static const QTok *qpar_next(QPar *const P) { return P->t++; }
static bool qpar_is(const QPar *const P, const QKind k) { return P->t->kind == k; }

static bool qpar_take(QPar *const P, const QKind k)
{
    if (!qpar_is(P, k))
        return false;

    P->t++;
    return true;
}

static const QTok *qpar_need(QPar *const P, const QKind k, const char *const want)
{
    if (!qpar_is(P, k))
        qpar_expected(P, want);

    return qpar_next(P);
}

[[noreturn]]
static void qpar_later(QPar *const P, const QTok *const t, const char *const what)
{
    char msg[80];
    const uint32_t n = (uint32_t)strlen(what);
    memcpy(msg, what, n);
    memcpy(msg + n, " not supported yet", 19);
    qpar_fail(P, t, msg);
}

/* ---- expressions */

static QExpr *qpar_binary(QPar *const P, const QTok *const op, QExpr *const a, QExpr *const b)
{
    QExpr *const e = qast_expr(P->mem, QE_BINARY, op);
    e->op = op->kind;
    e->a = a;
    e->b = b;
    return e;
}

/* name(args): each [name:] [mut] value. */
static QExpr *qpar_call(QPar *const P, const QTok *const name)
{
    QExpr *const e = qast_expr(P->mem, QE_CALL, name);
    QArg **last = &e->args;
    qpar_next(P); /* ( */

    while (!qpar_is(P, QK_RPAREN)) {
        QArg *const a = qast_alloc(P->mem, sizeof *a);

        if (qpar_is(P, QK_NAME) && P->t[1].kind == QK_COLON) {
            a->name = qpar_next(P);
            qpar_next(P);
        }

        if (qpar_is(P, QK_MUT))
            a->mut = qpar_next(P);

        a->e = qpar_expr(P);
        *last = a;
        last = &a->next;

        if (!qpar_take(P, QK_COMMA))
            break;
    }

    qpar_need(P, QK_RPAREN, ")");
    return e;
}

static QExpr *qpar_primary(QPar *const P)
{
    const QTok *const t = P->t;

    switch (t->kind) {
    case QK_INT:
    case QK_REAL: {
        QExpr *const e = qast_expr(P->mem, QE_CONST, qpar_next(P));
        e->untyped = true;
        e->type = t->kind == QK_INT ? QY_INT : QY_DOUBLE;
        e->val.i = t->i;
        e->val.d = t->d;
        return e;
    }

    case QK_STR: {
        QExpr *const e = qast_expr(P->mem, QE_CONST, qpar_next(P));
        QPiece *const s = qast_alloc(P->mem, sizeof *s);
        s->lit = t;
        e->type = QY_STRING;
        e->val.s = s;
        return e;
    }

    case QK_TRUE:
    case QK_FALSE: {
        QExpr *const e = qast_expr(P->mem, QE_CONST, qpar_next(P));
        e->type = QY_BOOL;
        e->val.i = t->kind == QK_TRUE;
        return e;
    }

    case QK_NAME:
        qpar_next(P);

        if (qpar_is(P, QK_LPAREN))
            return qpar_call(P, t);

        return qast_expr(P->mem, QE_NAME, t);

    case QK_LPAREN: {
        qpar_next(P);

        if (qpar_is(P, QK_RPAREN))
            qpar_later(P, t, "tuples are");

        QExpr *const e = qpar_expr(P);

        if (qpar_is(P, QK_COMMA))
            qpar_later(P, P->t, "tuples are");

        qpar_need(P, QK_RPAREN, ")");
        return e;
    }

    case QK_LBRACK:
        qpar_later(P, t, "lists are");

    case QK_FN:
        qpar_later(P, t, "lambdas are");

    case QK_LATER:
        qpar_later(P, t, "{} is");

    default:
        qpar_expected(P, "a value");
    }
}

/* A primary, then what may follow it. */
static QExpr *qpar_postfix(QPar *const P)
{
    QExpr *const e = qpar_primary(P);

    if (qpar_is(P, QK_DOT))
        qpar_later(P, P->t, "fields and methods are");
    if (qpar_is(P, QK_LBRACK))
        qpar_later(P, P->t, "indexing is");
    if (qpar_is(P, QK_QUEST) || qpar_is(P, QK_BANG))
        qpar_later(P, P->t, "{} is");

    return e;
}

static QExpr *qpar_unary(QPar *P);

/* a ** b: right to left, tighter than a unary minus on its left. */
static QExpr *qpar_power(QPar *const P)
{
    QExpr *const a = qpar_postfix(P);

    if (!qpar_is(P, QK_POW))
        return a;

    const QTok *const op = qpar_next(P);
    return qpar_binary(P, op, a, qpar_unary(P));
}

static QExpr *qpar_unary(QPar *const P)
{
    if (!qpar_is(P, QK_MINUS) && !qpar_is(P, QK_TILDE))
        return qpar_power(P);

    const QTok *const op = qpar_next(P);
    QExpr *const e = qast_expr(P->mem, QE_UNARY, op);
    e->op = op->kind;
    e->a = qpar_unary(P);
    return e;
}

/* One left-to-right level: its operands by next, its operators ops. */
static QExpr *qpar_level(
    QPar *const P,
    QExpr *(*const next)(QPar *),
    const QKind *const ops,
    const uint32_t nops
)
{
    QExpr *e = next(P);

    for (;;) {
        bool found = false;

        for (uint32_t i = 0; i < nops && !found; i++)
            found = qpar_is(P, ops[i]);

        if (!found)
            return e;

        const QTok *const op = qpar_next(P);
        e = qpar_binary(P, op, e, next(P));
    }
}

static QExpr *qpar_mul(QPar *const P)
{
    static const QKind ops[] = {QK_STAR, QK_SLASH, QK_DIV, QK_MOD};

    return qpar_level(P, qpar_unary, ops, countof(ops));
}

static QExpr *qpar_sum(QPar *const P)
{
    static const QKind ops[] = {QK_PLUS, QK_MINUS};

    return qpar_level(P, qpar_mul, ops, countof(ops));
}

static QExpr *qpar_shift(QPar *const P)
{
    static const QKind ops[] = {QK_SHL, QK_SHR};

    return qpar_level(P, qpar_sum, ops, countof(ops));
}

static QExpr *qpar_band(QPar *const P)
{
    static const QKind ops[] = {QK_AMP};

    return qpar_level(P, qpar_shift, ops, countof(ops));
}

static QExpr *qpar_bxor(QPar *const P)
{
    static const QKind ops[] = {QK_CARET};

    return qpar_level(P, qpar_band, ops, countof(ops));
}

static QExpr *qpar_bor(QPar *const P)
{
    static const QKind ops[] = {QK_BAR};

    return qpar_level(P, qpar_bxor, ops, countof(ops));
}

static bool qpar_relop(const QKind k)
{
    return k == QK_EQ || k == QK_NE || k == QK_LT || k == QK_LE || k == QK_GT || k == QK_GE;
}

/* a [not] in b..c. */
static QExpr *qpar_in(QPar *const P, QExpr *const a)
{
    const QTok *const t = P->t;
    const bool neg = qpar_take(P, QK_NOT);
    qpar_next(P); /* in */
    QExpr *const e = qast_expr(P->mem, QE_IN, t);
    e->neg = neg;
    e->a = a;
    e->b = qpar_bor(P);
    qpar_need(P, QK_DOTDOT, "..");
    e->c = qpar_bor(P);
    return e;
}

/* Comparisons, a < b <= c one chain; or a in b..c. */
static QExpr *qpar_compare(QPar *const P)
{
    QExpr *const a = qpar_bor(P);

    if (qpar_is(P, QK_IN) || (qpar_is(P, QK_NOT) && P->t[1].kind == QK_IN))
        return qpar_in(P, a);
    if (!qpar_relop(P->t->kind))
        return a;

    QExpr *const e = qast_expr(P->mem, QE_CHAIN, P->t);
    uint32_t cap = 0, capops = 0;
    e->items = qast_room(P->mem, e->items, 0, &cap, sizeof *e->items);
    e->items[e->n++] = a;

    while (qpar_relop(P->t->kind)) {
        e->ops = qast_room(P->mem, e->ops, e->n - 1, &capops, sizeof *e->ops);
        e->ops[e->n - 1] = qpar_next(P)->kind;
        e->items = qast_room(P->mem, e->items, e->n, &cap, sizeof *e->items);
        e->items[e->n++] = qpar_bor(P);
    }

    return e;
}

static QExpr *qpar_not(QPar *const P)
{
    if (!qpar_is(P, QK_NOT))
        return qpar_compare(P);

    const QTok *const op = qpar_next(P);
    QExpr *const e = qast_expr(P->mem, QE_UNARY, op);
    e->op = QK_NOT;
    e->a = qpar_not(P);
    return e;
}

/* A short-circuit level: and, or. */
static QExpr *qpar_logic(
    QPar *const P,
    QExpr *(*const next)(QPar *),
    const QKind op,
    const QExprKind kind
)
{
    QExpr *e = next(P);

    while (qpar_is(P, op)) {
        QExpr *const x = qast_expr(P->mem, kind, qpar_next(P));
        x->a = e;
        x->b = next(P);
        e = x;
    }

    return e;
}

static QExpr *qpar_and(QPar *const P) { return qpar_logic(P, qpar_not, QK_AND, QE_AND); }

static QExpr *qpar_xor(QPar *const P)
{
    static const QKind ops[] = {QK_XOR};

    return qpar_level(P, qpar_and, ops, countof(ops));
}

static QExpr *qpar_or(QPar *const P) { return qpar_logic(P, qpar_xor, QK_OR, QE_OR); }

/* a if c else b. */
static QExpr *qpar_expr(QPar *const P)
{
    QExpr *const a = qpar_or(P);

    if (!qpar_is(P, QK_IF))
        return a;

    QExpr *const e = qast_expr(P->mem, QE_COND, qpar_next(P));
    e->a = a;
    e->c = qpar_or(P);
    qpar_need(P, QK_ELSE, "else");
    e->b = qpar_expr(P);
    return e;
}

/* ---- statements */

/* A type after as: a name, nothing of a later milestone after it. */
static const QTok *qpar_type(QPar *const P)
{
    if (qpar_is(P, QK_LPAREN))
        qpar_later(P, P->t, "tuples are");
    if (qpar_is(P, QK_FN))
        qpar_later(P, P->t, "fn types are");

    const QTok *const t = qpar_need(P, QK_NAME, "a type");

    if (qpar_is(P, QK_LBRACK))
        qpar_later(P, P->t, "lists, maps and arrays are");
    if (qpar_is(P, QK_QUEST))
        qpar_later(P, P->t, "optional types are");
    if (qpar_is(P, QK_BANG))
        qpar_later(P, P->t, "error types are");
    if (qpar_is(P, QK_DOT))
        qpar_later(P, P->t, "imported types are");

    return t;
}

static QStmt *qpar_new(QPar *const P, const QStmtKind kind, const QTok *const tok)
{
    QStmt *const s = qast_alloc(P->mem, sizeof *s);
    s->kind = kind;
    s->tok = tok;
    return s;
}

/* s on the program's list. */
static void qpar_append(QPar *const P, QStmt *const s)
{
    s->prev = P->last;

    if (P->last)
        P->last->next = s;
    else
        P->prog->first = s;

    P->last = s;
}

static void qpar_push(QPar *const P, QStmt *const s)
{
    P->open = qast_room(P->mem, P->open, P->nopen, &P->capopen, sizeof *P->open);
    P->open[P->nopen++] = (QOpen){.open = s, .clause = s};
}

static QOpen *qpar_top(QPar *const P) { return P->nopen ? &P->open[P->nopen - 1] : nullptr; }

/* The innermost open loop, inside its fn; null none. */
static QStmt *qpar_loop(const QPar *const P)
{
    for (uint32_t i = P->nopen; i-- > 0;) {
        QStmt *const s = P->open[i].open;

        if (s->kind == QS_FN)
            return nullptr;
        if (s->kind == QS_WHILE || s->kind == QS_FOR || s->kind == QS_DO)
            return s;
    }

    return nullptr;
}

/* name [as type] [= value], one or more. */
static QDecl *qpar_decls(QPar *const P)
{
    QDecl *first = nullptr, **last = &first;

    do {
        QDecl *const d = qast_alloc(P->mem, sizeof *d);
        d->name = qpar_need(P, QK_NAME, "a name");

        if (qpar_take(P, QK_AS))
            d->type = qpar_type(P);
        if (qpar_take(P, QK_EQ))
            d->value = qpar_expr(P);

        *last = d;
        last = &d->next;
    } while (qpar_take(P, QK_COMMA));

    return first;
}

/* break, continue: its loop's. */
static QStmt *qpar_jump(QPar *const P, const QStmtKind kind)
{
    QStmt *const s = qpar_new(P, kind, qpar_next(P));
    s->link = qpar_loop(P);

    if (!s->link)
        qpar_fail(P, s->tok, "{} outside a loop");
    if (kind == QS_BREAK)
        s->link->broken = true;

    return s;
}

static QStmt *qpar_return(QPar *const P)
{
    QStmt *const s = qpar_new(P, QS_RETURN, qpar_next(P));

    for (uint32_t i = 0; i < P->nopen; i++)
        if (P->open[i].open->kind == QS_FN)
            s->fn = P->open[i].open->fn;

    if (!s->fn)
        qpar_fail(P, s->tok, "return outside a fn");
    if (!qpar_is(P, QK_NL))
        s->e = qpar_expr(P);

    return s;
}

/* An assignment or a call. */
static QStmt *qpar_simple(QPar *const P)
{
    const QTok *const t = P->t;

    switch (t->kind) {
    case QK_BREAK:
        return qpar_jump(P, QS_BREAK);

    case QK_CONTINUE:
        return qpar_jump(P, QS_CONTINUE);

    case QK_RETURN:
        return qpar_return(P);

    default:
        break;
    }

    const QKind op = t[1].kind;

    if (t->kind == QK_NAME &&
        (op == QK_EQ || op == QK_ADDEQ || op == QK_SUBEQ || op == QK_MULEQ || op == QK_DIVEQ)) {
        QStmt *const s = qpar_new(P, QS_ASSIGN, t);
        s->target = qast_expr(P->mem, QE_NAME, qpar_next(P));
        s->op = qpar_next(P)->kind;
        s->e = qpar_expr(P);
        return s;
    }

    QExpr *const e = qpar_expr(P);

    if (e->kind != QE_CALL)
        qpar_fail(P, t, "a value alone is not a statement: assign it or call a fn");

    QStmt *const s = qpar_new(P, QS_CALL, t);
    s->e = e;
    return s;
}

/* if c then: a block; if c then s: one line. */
static void qpar_if(QPar *const P)
{
    QStmt *const s = qpar_new(P, QS_IF, qpar_next(P));
    qpar_append(P, s);
    QExpr *const c = qpar_expr(P);
    s->e = c;
    qpar_need(P, QK_THEN, "then");

    if (qpar_is(P, QK_NL)) {
        qpar_push(P, s);
        return;
    }

    s->oneline = true;
    const QKind k = P->t->kind;

    if (k == QK_LET || k == QK_VAR || k == QK_CONST || k == QK_IF || k == QK_WHILE || k == QK_FOR ||
        k == QK_DO || k == QK_FN)
        qpar_fail(P, P->t, "a one-line if takes one simple statement, not {}");

    s->body = qpar_simple(P);
}

/* elseif c then, else: the if's next clause. */
static void qpar_clause(QPar *const P, const QStmtKind kind)
{
    QStmt *const s = qpar_new(P, kind, qpar_next(P));
    QOpen *const o = qpar_top(P);

    if (!o || o->open->kind != QS_IF)
        qpar_fail(P, s->tok, "{} without if");
    if (o->clause->kind == QS_ELSE)
        qpar_fail(P, s->tok, "{} after else");

    qpar_append(P, s);
    o->clause->link = s;
    o->clause = s;

    if (kind == QS_ELSEIF) {
        s->e = qpar_expr(P);
        qpar_need(P, QK_THEN, "then");
    }
}

/* for name in a..b [step s]. */
static void qpar_for(QPar *const P)
{
    QStmt *const s = qpar_new(P, QS_FOR, qpar_next(P));
    qpar_append(P, s);
    qpar_push(P, s);

    if (qpar_is(P, QK_MUT))
        qpar_later(P, P->t, "for mut is");

    QDecl *const d = qast_alloc(P->mem, sizeof *d);
    d->name = qpar_need(P, QK_NAME, "a name");
    s->decls = d;

    if (qpar_is(P, QK_COMMA))
        qpar_later(P, P->t, "an index with its element is");

    qpar_need(P, QK_IN, "in");
    s->e = qpar_bor(P);

    if (!qpar_is(P, QK_DOTDOT))
        qpar_later(P, P->t, "for over lists, maps and strings is");

    qpar_next(P);
    s->to = qpar_bor(P);

    if (qpar_take(P, QK_STEP))
        s->step = qpar_expr(P);
}

/* fn name(params) [as type]. */
static void qpar_fn(QPar *const P)
{
    QStmt *const s = qpar_new(P, QS_FN, qpar_next(P));
    QFn *const f = qast_alloc(P->mem, sizeof *f);
    const bool nested = P->nopen;
    s->fn = f;
    f->def = s;
    qpar_append(P, s);
    qpar_push(P, s);
    f->name = qpar_need(P, QK_NAME, "the fn's name");

    if (nested)
        qpar_fail(P, s->tok, "a fn goes at the top level, not in a block");

    if (P->lastfn)
        P->lastfn->next = f;
    else
        P->prog->fns = f;

    P->lastfn = f;

    if (qpar_is(P, QK_LBRACK))
        qpar_later(P, P->t, "generic fns are");

    qpar_need(P, QK_LPAREN, "(");
    uint32_t cap = 0;

    while (!qpar_is(P, QK_RPAREN)) {
        f->params = qast_room(P->mem, f->params, f->npar, &cap, sizeof *f->params);
        QParam *const p = &f->params[f->npar];
        *p = (QParam){.mut = qpar_take(P, QK_MUT)};

        p->name = qpar_need(P, QK_NAME, "a parameter's name");

        if (qpar_take(P, QK_AS))
            p->type = qpar_type(P);
        if (qpar_take(P, QK_EQ))
            p->def = qpar_expr(P);

        f->npar++;

        if (!qpar_take(P, QK_COMMA))
            break;
    }

    qpar_need(P, QK_RPAREN, ")");

    if (qpar_take(P, QK_AS))
        f->rtype = qpar_type(P);
    if (qpar_is(P, QK_LATER))
        qpar_later(P, P->t, "{} is");
}

/* What end closes: QS_ENDIF .. QS_ENDFN by the word after end. */
static const struct {
    QKind word;
    QStmtKind open, close;
} qpar_ends[] = {
    {QK_IF, QS_IF, QS_ENDIF},
    {QK_WHILE, QS_WHILE, QS_ENDWHILE},
    {QK_FOR, QS_FOR, QS_ENDFOR},
    {QK_DO, QS_DO, QS_ENDDO},
    {QK_FN, QS_FN, QS_ENDFN},
};

/* end if, end while, end for, end do, end fn: its block closed. */
static void qpar_end(QPar *const P)
{
    const QTok *const t = qpar_next(P);
    uint32_t k = 0;

    while (k < countof(qpar_ends) && !qpar_is(P, qpar_ends[k].word))
        k++;
    if (k == countof(qpar_ends))
        qpar_expected(P, "if, while, for, do or fn after end");

    const QTok *const word = qpar_next(P);
    QOpen *const o = qpar_top(P);

    if (!o || o->open->kind != qpar_ends[k].open) {
        char msg[32];
        memcpy(msg, "end {} without ", 15);
        const uint32_t n = word->len;
        memcpy(msg + 15, word->text, n);
        msg[15 + n] = 0;
        qast_error(&P->prog->errs, t, msg, word->text, word->len);
        longjmp(P->fail, 1);
    }

    QStmt *const s = qpar_new(P, qpar_ends[k].close, t);
    qpar_append(P, s);
    QStmt *const open = o->open;
    o->clause->link = s;
    s->link = open;

    if (s->kind == QS_ENDFN) {
        s->fn = open->fn;
        open->fn->end = s;
    }

    P->nopen--;
}

static void qpar_statement(QPar *const P)
{
    const QTok *const t = P->t;

    switch (t->kind) {
    case QK_LET:
    case QK_VAR:
    case QK_CONST: {
        QStmt *const s = qpar_new(P, QS_DECL, qpar_next(P));
        s->op = t->kind;
        qpar_append(P, s);
        s->decls = qpar_decls(P);
        return;
    }

    case QK_IF:
        qpar_if(P);
        return;

    case QK_ELSEIF:
        qpar_clause(P, QS_ELSEIF);
        return;

    case QK_ELSE:
        qpar_clause(P, QS_ELSE);
        return;

    case QK_END:
        qpar_end(P);
        return;

    case QK_WHILE: {
        QStmt *const s = qpar_new(P, QS_WHILE, qpar_next(P));
        qpar_append(P, s);
        qpar_push(P, s);
        s->e = qpar_expr(P);
        return;
    }

    case QK_DO: {
        QStmt *const s = qpar_new(P, QS_DO, qpar_next(P));
        qpar_append(P, s);
        qpar_push(P, s);
        return;
    }

    case QK_FOR:
        qpar_for(P);
        return;

    case QK_FN:
        qpar_fn(P);
        return;

    case QK_LATER:
        qpar_later(P, t, "{} is");

    default:
        qpar_append(P, qpar_simple(P));
    }
}

/* Each block left open: its error at its opener. */
static void qpar_unclosed(QPar *const P)
{
    static const char *const msgs[] = {
        [QS_IF] = "if without end if",
        [QS_WHILE] = "while without end while",
        [QS_FOR] = "for without end for",
        [QS_DO] = "do without end do",
        [QS_FN] = "fn without end fn",
    };

    for (uint32_t i = 0; i < P->nopen; i++) {
        const QStmt *const s = P->open[i].open;
        qast_error(&P->prog->errs, s->tok, msgs[s->kind], nullptr, 0);
    }
}

void qpar_program(QProg *const p, QMem *const m)
{
    QPar P = {.prog = p, .mem = m, .t = p->src.toks};

    while (P.t->kind != QK_EOF) {
        if (!setjmp(P.fail)) {
            qpar_statement(&P);

            if (!qpar_is(&P, QK_NL))
                qpar_expected(&P, "the line's end");
        }

        while (P.t->kind != QK_NL && P.t->kind != QK_EOF)
            P.t++;

        qpar_take(&P, QK_NL);
    }

    qpar_unclosed(&P);
}
