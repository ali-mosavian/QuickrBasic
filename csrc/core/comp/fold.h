/* Constant folding's interface. How it works: fold.c. */
#ifndef COMP_FOLD_H
#define COMP_FOLD_H
#include "defs.h"
#include "comp/ast.h"

/* cm_fold (for a CONST's value after its conversion): the code from start to comp.out, of type comp.type: all FK ops, two or more, becomes
   one load of its value. */
void fold_code(Session *S, uint8_t *start);
/* cf_fail: repl_error while fold_code runs: back to it, the code as it was. */
void fold_fail(Session *S);
/* cm_krec: a CONST's scalar store, variable id's: its value's code from start folded; one literal
   load is kept as the variable's. */
void fold_const(Session *S, uint8_t *start, uint16_t id);
/* The tree at node: each operator whose operands are literals becomes the literal of its value. */
void fold_tree(Session *S, Node *node);
/* Op a literal's load. */
bool fold_isliteral(uint8_t op);
/* The type literal load op gives. */
uint8_t fold_littype(uint8_t op);
/* var_kclr: no CONST's literal kept: a new link's CONSTs record theirs. */
void fold_clear(Session *S);
#endif
