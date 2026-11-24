/* Quickr's parser: tokens to statements, one a line, and their expressions' trees.
 *
 * # How it works
 * Recursive descent, one function a precedence level, lowest first: a if c else b, or, xor, and,
 * not, comparisons (chained, and in a..b), | ^ & << >>, + -, * / div mod, unary - ~, **; then
 * a.f, a.f(args) and a[i] after a primary. A line starting with a name is tried as places then
 * =, else parsed as a call. A statement is parsed to its QK_NL; an error longjmps to the statement's start, which skips to
 * the QK_NL, so one bad statement makes one error.
 *
 * Statements form one flat list in source order. A stack of the open blocks links each clause to
 * the next (if, elseif, else, end if), each loop to its end and back, each break and continue to
 * its loop, each case to the next (and to its match, by body). A block's opener is linked before
 * its condition is parsed, so a bad condition still pairs with its end. In a type or an enum each
 * line is a field or a value.
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

/* (a, b ..) after its first item a, the next a comma: a QE_TUPLE. */
static QExpr *qpar_tuple(QPar *const P, QExpr *const a)
{
    QExpr *const e = qast_expr(P->mem, QE_TUPLE, a->tok);
    uint32_t cap = 0;
    e->items = qast_room(P->mem, e->items, 0, &cap, sizeof *e->items);
    e->items[e->n++] = a;

    while (qpar_take(P, QK_COMMA)) {
        e->items = qast_room(P->mem, e->items, e->n, &cap, sizeof *e->items);
        e->items[e->n++] = qpar_expr(P);
    }

    return e;
}

/* f"..": its parts, each a string's (a QE_CONST) or a value's with its spec. */
static QExpr *qpar_fstring(QPar *const P)
{
    QExpr *const e = qast_expr(P->mem, QE_FSTR, qpar_next(P));
    uint32_t cap = 0, capspec = 0;

    while (!qpar_take(P, QK_FEND)) {
        QExpr *part;
        const QTok *spec = nullptr;

        if (qpar_is(P, QK_FPART)) {
            part = qast_expr(P->mem, QE_CONST, P->t);
            QPiece *const s = qast_alloc(P->mem, sizeof *s);
            s->lit = qpar_next(P);
            part->type = QY_STRING;
            part->val.s = s;
        } else {
            qpar_need(P, QK_LBRACE, "{");
            part = qpar_expr(P);

            if (qpar_is(P, QK_FSPEC))
                spec = qpar_next(P);

            qpar_need(P, QK_RBRACE, "}");
        }

        e->items = qast_room(P->mem, e->items, e->n, &cap, sizeof *e->items);
        e->specs = qast_room(P->mem, e->specs, e->n, &capspec, sizeof *e->specs);
        e->specs[e->n] = spec;
        e->items[e->n++] = part;
    }

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

    case QK_NONE:
        return qast_expr(P->mem, QE_NONE, qpar_next(P));

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
            qpar_expected(P, "a value");

        QExpr *const e = qpar_expr(P);

        if (qpar_is(P, QK_COMMA)) {
            QExpr *const tu = qpar_tuple(P, e);
            qpar_need(P, QK_RPAREN, ")");
            return tu;
        }

        qpar_need(P, QK_RPAREN, ")");
        return e;
    }

    case QK_FSTR:
        return qpar_fstring(P);

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

/* A primary, then its fields, methods and elements: a.f, a.f(args), a[i]. */
static QExpr *qpar_postfix(QPar *const P)
{
    QExpr *e = qpar_primary(P);

    for (;;) {
        if (qpar_take(P, QK_DOT)) {
            const QTok *const name = qpar_need(P, QK_NAME, "a field's name");

            if (qpar_is(P, QK_LPAREN)) {
                QExpr *const call = qpar_call(P, name);
                call->a = e;
                e = call;
                continue;
            }

            QExpr *const f = qast_expr(P->mem, QE_FIELD, name);
            f->a = e;
            e = f;
            continue;
        }

        if (qpar_is(P, QK_LBRACK)) {
            QExpr *const x = qast_expr(P->mem, QE_INDEX, qpar_next(P));
            x->a = e;
            x->b = qpar_is(P, QK_DOTDOT) ? nullptr : qpar_expr(P);

            if (qpar_take(P, QK_DOTDOT)) {
                x->kind = QE_SLICE;
                x->c = qpar_is(P, QK_RBRACK) ? nullptr : qpar_expr(P);
            }

            qpar_need(P, QK_RBRACK, "]");
            e = x;
            continue;
        }

        if (qpar_is(P, QK_QUEST)) {
            QExpr *const x = qast_expr(P->mem, QE_TRY, qpar_next(P));
            x->a = e;
            e = x;
            continue;
        }

        if (qpar_is(P, QK_BANG))
            qpar_later(P, P->t, "{} is");

        return e;
    }
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

    if (qpar_take(P, QK_DOTDOT))
        e->c = qpar_bor(P);

    return e;
}

/* Comparisons, a < b <= c one chain; or a in b..c. */
/* a ?? b, binding tighter than a comparison; right to left. */
static QExpr *qpar_coalesce(QPar *const P)
{
    QExpr *const a = qpar_bor(P);

    if (!qpar_is(P, QK_QQ))
        return a;

    const QTok *const op = qpar_next(P);
    return qpar_binary(P, op, a, qpar_coalesce(P));
}

/* a is [not] none, a is [not] Error. */
static QExpr *qpar_test(QPar *const P, QExpr *const a)
{
    QExpr *const e = qast_expr(P->mem, QE_IS, qpar_next(P));
    e->a = a;
    e->neg = qpar_take(P, QK_NOT);

    if (qpar_is(P, QK_NONE) ||
        (qpar_is(P, QK_NAME) && P->t->len == 5 && !memcmp(P->t->text, "Error", 5))) {
        e->op = qpar_next(P)->kind;
        return e;
    }

    qpar_need(P, QK_NONE, "none or Error");
    return e;
}

static QExpr *qpar_compare(QPar *const P)
{
    QExpr *const a = qpar_coalesce(P);

    if (qpar_is(P, QK_IS))
        return qpar_test(P, a);

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
        e->items[e->n++] = qpar_coalesce(P);
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

static QTypeEx *qpar_typex(QPar *const P, const QTypeExKind kind, const QTok *const tok)
{
    QTypeEx *const x = qast_alloc(P->mem, sizeof *x);
    x->kind = kind;
    x->tok = tok;
    return x;
}

/* A type after as: a name, (T, U), then [] or [n] any times. */
static QTypeEx *qpar_type(QPar *const P)
{
    if (qpar_is(P, QK_FN))
        qpar_later(P, P->t, "fn types are");

    QTypeEx *x;

    if (qpar_is(P, QK_LPAREN)) {
        x = qpar_typex(P, QX_TUPLE, qpar_next(P));
        uint32_t cap = 0;

        do {
            x->items = qast_room(P->mem, x->items, x->n, &cap, sizeof *x->items);
            x->items[x->n++] = qpar_type(P);
        } while (qpar_take(P, QK_COMMA));

        qpar_need(P, QK_RPAREN, ")");
    } else
        x = qpar_typex(P, QX_NAME, qpar_need(P, QK_NAME, "a type"));

    if (qpar_is(P, QK_DOT))
        qpar_later(P, P->t, "imported types are");

    while (qpar_is(P, QK_LBRACK)) {
        const QTok *const at = qpar_next(P);
        QTypeEx *const y = qpar_typex(P, QX_LIST, at);
        y->elem = x;

        if (!qpar_is(P, QK_RBRACK)) {
            y->kind = QX_ARRAY;
            y->count = qpar_expr(P);
        }

        qpar_need(P, QK_RBRACK, "]");
        x = y;
    }

    if (qpar_is(P, QK_QUEST) || qpar_is(P, QK_BANG)) {
        QTypeEx *const y = qpar_typex(P, qpar_is(P, QK_QUEST) ? QX_OPT : QX_RES, qpar_next(P));
        y->elem = x;
        return y;
    }

    return x;
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

/* A value, or a, b ..: a QE_TUPLE. */
static QExpr *qpar_values(QPar *const P)
{
    QExpr *const e = qpar_expr(P);
    return qpar_is(P, QK_COMMA) ? qpar_tuple(P, e) : e;
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
        s->e = qpar_values(P);

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

    if (t->kind == QK_NAME) { /* places, then = : an assignment */
        QExpr *to = qpar_postfix(P);

        if (qpar_is(P, QK_COMMA)) {
            QExpr *const tu = qast_expr(P->mem, QE_TUPLE, t);
            uint32_t cap = 0;
            tu->items = qast_room(P->mem, tu->items, 0, &cap, sizeof *tu->items);
            tu->items[tu->n++] = to;

            while (qpar_take(P, QK_COMMA)) {
                tu->items = qast_room(P->mem, tu->items, tu->n, &cap, sizeof *tu->items);
                tu->items[tu->n++] = qpar_postfix(P);
            }

            to = tu;
        }

        const QKind op = P->t->kind;

        if (op == QK_EQ || op == QK_ADDEQ || op == QK_SUBEQ || op == QK_MULEQ || op == QK_DIVEQ) {
            QStmt *const s = qpar_new(P, QS_ASSIGN, t);
            s->target = to;
            s->op = qpar_next(P)->kind;
            s->e = to->kind == QE_TUPLE ? qpar_values(P) : qpar_expr(P);
            return s;
        }

        P->t = t;
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
        k == QK_DO || k == QK_FN || k == QK_TYPE || k == QK_ENUM || k == QK_MATCH || k == QK_CASE)
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

    s->mut = qpar_take(P, QK_MUT);
    QDecl *const d = qast_alloc(P->mem, sizeof *d);
    d->name = qpar_need(P, QK_NAME, "a name");
    s->decls = d;

    if (qpar_take(P, QK_COMMA)) { /* i, x: the index first */
        d->next = qast_alloc(P->mem, sizeof *d);
        d->next->name = qpar_need(P, QK_NAME, "a name");
    }

    qpar_need(P, QK_IN, "in");
    s->e = qpar_bor(P);

    if (!qpar_is(P, QK_DOTDOT)) {
        s->each = true;
        return;
    }

    if (s->mut || d->next)
        qpar_fail(P, P->t, "a range's for takes one name, not mut");

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
    {QK_TYPE, QS_TYPE, QS_ENDTYPE},
    {QK_ENUM, QS_ENUM, QS_ENDENUM},
    {QK_MATCH, QS_MATCH, QS_ENDMATCH},
};

/* end if, end while, end for, end do, end fn: its block closed. */
static void qpar_end(QPar *const P)
{
    const QTok *const t = qpar_next(P);
    uint32_t k = 0;

    while (k < countof(qpar_ends) && !qpar_is(P, qpar_ends[k].word))
        k++;
    if (k == countof(qpar_ends))
        qpar_expected(P, "if, while, for, do, fn, type, enum or match after end");

    const QTok *const word = qpar_next(P);
    QOpen *const o = qpar_top(P);

    if (o && o->open->kind == QS_MATCH && o->clause == o->open)
        qpar_fail(P, t, "a match needs a case");

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

/* type Name, enum Name, match e: a block whose lines are its own. */
static void qpar_block(QPar *const P, const QStmtKind kind)
{
    QStmt *const s = qpar_new(P, kind, qpar_next(P));
    qpar_append(P, s);
    qpar_push(P, s);

    if (kind == QS_MATCH)
        s->e = qpar_expr(P);
    else
        s->decls = qpar_decls(P); /* its name alone */

    if (kind != QS_MATCH && (s->decls->type || s->decls->value || s->decls->next))
        qpar_fail(P, s->tok, "{} takes a name alone");
    if (kind == QS_TYPE && P->nopen > 1)
        qpar_fail(P, s->tok, "a type goes at the top level, not in a block");
    if (kind == QS_ENUM && P->nopen > 1)
        qpar_fail(P, s->tok, "an enum goes at the top level, not in a block");
}

/* A line in a type: name as type [= default]. */
static void qpar_field(QPar *const P)
{
    QStmt *const s = qpar_new(P, QS_FIELD, P->t);
    qpar_append(P, s);
    s->decls = qpar_decls(P);
}

/* A line in an enum: name, or name(field, ..) with data. */
static void qpar_variant(QPar *const P)
{
    QStmt *const s = qpar_new(P, QS_VARIANT, P->t);
    qpar_append(P, s);
    QDecl *const d = qast_alloc(P->mem, sizeof *d);
    d->name = qpar_need(P, QK_NAME, "a value's name");
    s->decls = d;

    if (!qpar_take(P, QK_LPAREN))
        return;

    d->next = qpar_decls(P);
    qpar_need(P, QK_RPAREN, ")");
}

/* One pattern of a case: is op v, a to b, a value. */
static QPat *qpar_pat(QPar *const P)
{
    QPat *const pt = qast_alloc(P->mem, sizeof *pt);
    pt->tok = P->t;

    if (qpar_take(P, QK_IS)) {
        if (!qpar_relop(P->t->kind))
            qpar_expected(P, "a comparison after is");

        pt->kind = QP_IS;
        pt->op = qpar_next(P)->kind;
        pt->a = qpar_bor(P);
        return pt;
    }

    pt->a = qpar_bor(P);

    if (qpar_is(P, QK_NAME) && P->t->len == 2 && !memcmp(P->t->text, "to", 2)) {
        qpar_next(P);
        pt->kind = QP_RANGE;
        pt->b = qpar_bor(P);
    }

    return pt;
}

/* case pattern, ..; case else: the match's next clause. */
static void qpar_case(QPar *const P)
{
    QStmt *const s = qpar_new(P, QS_CASE, qpar_next(P));
    QOpen *const o = qpar_top(P);

    if (!o || o->open->kind != QS_MATCH)
        qpar_fail(P, s->tok, "case outside a match");
    if (o->clause->kind == QS_CASE && !o->clause->pats)
        qpar_fail(P, s->tok, "case after case else");

    qpar_append(P, s);
    o->clause->link = s;
    o->clause = s;
    s->body = o->open;

    if (qpar_take(P, QK_ELSE))
        return;

    QPat **last = &s->pats;

    do {
        *last = qpar_pat(P);
        last = &(*last)->next;
    } while (qpar_take(P, QK_COMMA));
}

static void qpar_statement(QPar *const P)
{
    const QTok *const t = P->t;
    const QOpen *const o = qpar_top(P);

    if (o && t->kind != QK_END) {
        if (o->open->kind == QS_TYPE) {
            qpar_field(P);
            return;
        }

        if (o->open->kind == QS_ENUM) {
            qpar_variant(P);
            return;
        }

        if (o->open->kind == QS_MATCH && o->clause == o->open && t->kind != QK_CASE)
            qpar_fail(P, t, "a match's first line is a case");
    }

    switch (t->kind) {
    case QK_TYPE:
        qpar_block(P, QS_TYPE);
        return;

    case QK_ENUM:
        qpar_block(P, QS_ENUM);
        return;

    case QK_MATCH:
        qpar_block(P, QS_MATCH);
        return;

    case QK_CASE:
        qpar_case(P);
        return;

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
        [QS_TYPE] = "type without end type",
        [QS_ENUM] = "enum without end enum",
        [QS_MATCH] = "match without end match",
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
