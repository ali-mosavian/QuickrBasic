/* The expression tree's nodes: making them, and the type and work of each.
 *
 * # How it works
 * The parsers (expr.c, refs.c) build a line's trees; sema.c checks them, fold.c folds them and
 * codegen.c emits them. Nodes come from the statement's arena (Ast), freed all at once when the
 * next statement starts.
 *
 * Leaves are a literal load (N_LIT: a number, a string, a CONST's value, a size) and a scalar
 * variable (N_VAR). An element (N_ELEM), QBasic's functions (N_FN: SGN .. MID$; N_XFN: the others),
 * a place (N_REF) and a procedure's call (N_CALL) hold their arguments as children linked by next.
 * A record's field (N_FIELD), an address (N_ADDR), a call's argument (N_ARG) and a conversion
 * (N_CONV) have one child, a.
 *
 * A statement is its steps (Ast.step): trees codegen emits in order at the statement's end. A step
 * is an expression, a conversion of one, or a CALL of the VM's (N_KCALL).
 *
 * # asm structure
 * No asm counterpart: the asm emits code as it parses, with no tree (see expr.c).
 */
#include "comp/ast.h"
#include "comp/dfa.h"
#include "mem/symbols.h"

void ast_init(Ast *const t, Fault *const f)
{
    arena_init(&t->arena, AST_CHUNK);
    t->fault = f;
}

void ast_reset(Ast *const t)
{
    arena_reset(&t->arena);
    t->steps = t->last = nullptr;
}

bool ast_islit(const Node *const n) { return n->kind == N_LIT; }

Node *ast_new(Ast *const t, const NodeKind kind)
{
    Node *const n = arena_alloc(&t->arena, sizeof *n);

    if (!n)
        fault_error(t->fault, E_OM);

    n->kind = kind;
    return n;
}

Node *ast_last(const Node *const parent)
{
    Node *a = parent->b;

    while (a && a->next)
        a = a->next;

    return a;
}

void ast_link(Node *const parent, Node **const last, Node *const arg)
{
    arg->next = nullptr;

    if (*last)
        (*last)->next = arg;
    else
        parent->b = arg;

    *last = arg;
    parent->nargs++;
}

/* A DOUBLE either side; or for a DOUBLE target ^, and unless of constants / or a SINGLE's. */
static bool wants_double(const Node *const n, const uint8_t lt, const uint8_t rt, const bool bothk)
{
    if (lt == T_DBL || rt == T_DBL)
        return true;
    if (!n->dbl)
        return false;

    return n->op == OP_POW || (!bothk && (n->op == OP_DIV || lt == T_FLT || rt == T_FLT));
}

uint8_t ast_work(const Node *const n)
{
    const uint8_t rel = n->op;

    if (ast_islogic(rel) || ast_isxop(rel))
        return W_INT;

    const uint8_t lt = ast_type(n->a), rt = ast_type(n->b);

    if (lt & T_STR)
        return W_STR;

    const bool bothk = rel != OP_POW && ast_islit(n->a) && ast_islit(n->b);

    if (wants_double(n, lt, rt, bothk))
        return W_DBL;
    if (rel == OP_POW)
        return W_POW;

    return lt == T_INT && rt == T_INT && rel != OP_DIV ? W_INT : W_FLT;
}

uint8_t ast_type(const Node *const n)
{
    switch (n->kind) {
    case N_UN:
        return n->op == TK_NOT ? T_INT : ast_type(n->a);

    case N_BIN:
        if (ast_isrel(n->op))
            return T_INT;
        switch (ast_work(n)) {
        case W_INT:
            return T_INT;

        case W_DBL:
            return T_DBL;

        case W_STR:
            return T_STR;

        default:
            return T_FLT;
        }

    default:
        return n->type;
    }
}

void ast_step(Ast *const t, Node *const n)
{
    n->next = nullptr;

    if (t->last)
        t->last->next = n;
    else
        t->steps = n;

    t->last = n;
}

Node *ast_wrapat(Ast *const t, Node *const n, const NodeKind kind)
{
    Node *const a = ast_new(t, kind);
    *a = *n;
    a->next = nullptr;
    *n = (Node){.kind = kind, .a = a, .next = n->next};

    return n;
}

Node *ast_wrap(Ast *const t, const NodeKind kind) { return ast_wrapat(t, t->last, kind); }

void ast_conv(Ast *const t, const uint8_t cv, const uint8_t *const end) { ast_convat(t, t->last, cv, end); }

void ast_convat(Ast *const t, Node *const at, const uint8_t cv, const uint8_t *const end)
{
    static const uint8_t type[] = {
        [CV_INT] = T_INT,
        [CV_FLT] = T_FLT,
        [CV_DBL] = T_DBL,
        [CV_STR] = T_STR,
        [CV_BYTE] = T_INT,
        [CV_ADDR] = T_INT,
    };

    Node *const n = ast_wrapat(t, at, N_CONV);
    n->op = cv;
    n->type = type[cv];
    n->end = end;
}
