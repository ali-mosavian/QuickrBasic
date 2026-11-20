/* The expression parser's interface. How it works: expr.c. */
#ifndef COMP_EXPR_H
#define COMP_EXPR_H
#include "comp/comp.h"
#include "comp/ast.h"
#include "comp/codegen.h"

typedef struct Expr {
    bool dbltgt; /* g_cwd: the assignment's target is a DOUBLE */
} Expr;

/* By kind, SK_DBL a DOUBLE: its T_*. */
extern const uint8_t expr_ktype[5];

/* An expression parsed, for a DOUBLE target or not: its tree, nothing checked or compiled. */
Node *expr_tree(Comp *C, bool dbl);
/* A_LV as a step: an element's subscripts an N_TARGET step; comp.tgt set. A record's field the old
   way, its address emitted. */
void expr_compile(Comp *C); /* cm_expr: an expression at comp.p, a step */
Node *expr_arg(Comp *C);    /* an argument: an expression's tree of its own, not for a DOUBLE */
void expr_args(Comp *C, Node *node); /* ( e {, e} ): node's subscripts, MAXDIMS at most */
/* VARPTR, VARSEG, SSEG, SADD of place at: CALL k after it, 0 none; a record's field's offset
   added. */
Node *expr_addr(Comp *C, Node *at, uint8_t k);
Node *expr_intleaf(Comp *C, uint32_t v, uint8_t sh); /* an INT literal's node */
void expr_paren(Comp *C);                            /* ( expr ) */
void expr_load(Comp *C);    /* a variable's, an element's, a FUNCTION's value */
void expr_target(Comp *C);  /* cm_lval: a store's target; an element's subscripts a step */
void expr_string(Comp *C);  /* cm_str1: a literal after its quote, C.LD.TXT */
void expr_varaddr(Comp *C); /* cm_va: a variable's place for GET, PUT, FIELD: ( -- addr kind ) */
#endif
