/* The tree's nodes and what they compute: their types, a binary node's work. */
#include "comp/ast.h"
#include "session.h"
#include "comp/dfa.h"
#include "io/repl.h"
#include "mem/symbols.h"

void ast_init(Session *const S) { arena_init(&S->ast.arena, AST_CHUNK); }

void ast_reset(Session *const S)
{
    arena_reset(&S->ast.arena);
    S->ast.steps = S->ast.last = nullptr;
}

bool ast_islit(const Node *const n) { return n->kind == N_LIT; }

Node *ast_new(Session *const S, const NodeKind kind)
{
    Node *const n = arena_alloc(&S->ast.arena, sizeof *n);
    if (!n)
        repl_error(S, E_OM);
    n->kind = kind;
    return n;
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
    case N_UN: return n->op == TK_NOT ? T_INT : ast_type(n->a);
    case N_BIN:
        if (ast_isrel(n->op))
            return T_INT;
        switch (ast_work(n)) {
        case W_INT: return T_INT;
        case W_DBL: return T_DBL;
        case W_STR: return T_STR;
        default: return T_FLT;
        }
    default: return n->type;
    }
}

void ast_step(Session *const S, Node *const n)
{
    n->next = nullptr;
    if (S->ast.last)
        S->ast.last->next = n;
    else
        S->ast.steps = n;
    S->ast.last = n;
}

/* The last step becomes its conversion: the node in place, its tree a copy under it. */
void ast_conv(Session *const S, const uint8_t cv)
{
    Node *const last = S->ast.last;
    Node *const a = ast_new(S, N_CONV);
    *a = *last;
    a->next = nullptr;
    *last = (Node){.kind = N_CONV, .op = cv, .a = a, .end = S->comp.p};
}
