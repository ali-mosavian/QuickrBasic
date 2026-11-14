/* The expression parser (comp/expr.asm): precedence climbing over comp.p builds a tree
   (comp/ast.h); fold_tree folds it and cg_tree (comp/codegen.h) turns it into code. */
#ifndef COMP_EXPR_H
#define COMP_EXPR_H
#include "comp/comp.h"
#include "comp/ast.h"
#include "comp/codegen.h"

typedef struct {
    bool dbltgt; /* g_cwd: the assignment's target is a DOUBLE */
} Expr;

/* By kind, SK_DBL a DOUBLE: its T_*. */
extern const uint8_t expr_ktype[5];

void expr_compile(Session *S); /* cm_expr: an expression at comp.p: parsed, folded, compiled */
void expr_emit(Session *S, Node *root); /* root's tree folded and compiled */
Node *expr_arg(Session *S); /* an argument: an expression's tree of its own, not for a DOUBLE */
void expr_args(Session *S, Node *node);                 /* ( e {, e} ): node's arguments */
Node *expr_intleaf(Session *S, uint32_t v, uint8_t sh); /* an INT literal's node */
void expr_assign(Session *S);  /* cm_lexpr: an assignment's value, for a DOUBLE target as BC */
void expr_paren(Session *S);   /* ( expr ) */
uint8_t expr_subs(Session *S); /* cm_subs0: ( e {, e} ), each an INT; how many */
void expr_load(Session *S);    /* a variable's, an element's, a FUNCTION's value */
void expr_target(Session *S);  /* cm_lval: a store's target, its subscripts compiled */
void expr_string(Session *S);  /* cm_str1: a literal after its quote, S.LD.TXT */
void expr_varaddr(Session *S); /* cm_va: a variable's place for GET, PUT, FIELD: ( -- addr kind ) */
#endif
