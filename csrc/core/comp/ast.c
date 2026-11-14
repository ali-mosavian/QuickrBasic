/* The tree's nodes and what they compute: their types, a binary node's work. */
#include "comp/ast.h"
#include "session.h"
#include "comp/dfa.h"
#include "io/repl.h"
#include "mem/symb.h"

static Node *node(Session *const S, const uint16_t i) { return ast_node(S, i); }

bool ast_islit(Session *const S, const uint16_t i) { return node(S, i)->kind == N_LIT; }

uint16_t ast_new(Session *const S, const NodeKind kind)
{
    if (S->ast.nnodes >= AST_NODES)
        repl_error(S, E_OM);
    const uint16_t i = S->ast.nnodes++;
    S->ast.node[i] = (Node){.kind = kind};
    return i;
}

Node *ast_node(Session *const S, const uint16_t i) { return &S->ast.node[i]; }

void ast_link(Session *const S, const uint16_t n, uint16_t *const last, const uint16_t a)
{
    ast_node(S, a)->next = NO_NODE;
    if (*last == NO_NODE)
        ast_node(S, n)->b = a;
    else
        ast_node(S, *last)->next = a;
    *last = a;
    ast_node(S, n)->nargs++;
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

uint8_t ast_work(Session *const S, const Node *const n)
{
    const uint8_t rel = n->op;
    if (ast_islogic(rel) || ast_isxop(rel))
        return W_INT;
    const uint8_t lt = ast_type(S, n->a), rt = ast_type(S, n->b);
    if (lt & T_STR)
        return W_STR;
    const bool bothk = rel != OP_POW && ast_islit(S, n->a) && ast_islit(S, n->b);
    if (wants_double(n, lt, rt, bothk))
        return W_DBL;
    if (rel == OP_POW)
        return W_POW;
    return lt == T_INT && rt == T_INT && rel != OP_DIV ? W_INT : W_FLT;
}

uint8_t ast_type(Session *const S, const uint16_t i)
{
    const Node *const n = node(S, i);
    switch (n->kind) {
    case N_VAR: {
        const uint8_t info = symb_var(&S->symb, n->id)->info;
        return expr_ktype[info & (SK_DBL | VK_MASK)];
    }
    case N_UN: return n->op == TK_NOT ? T_INT : ast_type(S, n->a);
    case N_BIN:
        if (ast_isrel(n->op))
            return T_INT;
        switch (ast_work(S, n)) {
        case W_INT: return T_INT;
        case W_DBL: return T_DBL;
        case W_STR: return T_STR;
        default: return T_FLT;
        }
    default: return n->type;
    }
}
