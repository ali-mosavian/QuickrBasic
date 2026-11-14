/* Constant folding (comp/fold.asm): code of FK ops only (vm/ops.def) runs as it compiles,
   through the VM's own handlers, so its value is the run's to the bit, and becomes one load of
   it. An error while it runs leaves the code as it was, to raise when run.

   A CONST whose value folds to one literal load keeps that load (VarX.kop, symb_kval): a use
   compiled after it loads the literal instead of the variable. The CONST still stores the
   variable, for the uses compiled before. */
#ifndef COMP_FOLD_H
#define COMP_FOLD_H
#include "defs.h"
#include "comp/ast.h"

/* cm_fold (for a CONST's value after its conversion): the code from start to comp.out, of type comp.type: all FK ops, two or more, becomes
   one load of its value. */
void fold_code(Session *S, uint8_t *start);
/* cf_fail: repl_error while fold_code runs: back to it, the code as it was. */
void fold_fail(Session *S);
/* cm_krec: a CONST's scalar store: its value (from comp.vat) folded; one literal load is kept
   as the variable's. */
void fold_const(Session *S);
/* The tree at node: each operator whose operands are literals becomes the literal of its value. */
void fold_tree(Session *S, Node *node);
/* Op a literal's load. */
bool fold_isliteral(uint8_t op);
/* The type literal load op gives. */
uint8_t fold_littype(uint8_t op);
/* var_kclr: no CONST's literal kept: a new link's CONSTs record theirs. */
void fold_clear(Session *S);
#endif
