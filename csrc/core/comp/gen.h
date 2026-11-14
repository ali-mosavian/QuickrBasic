/* The code generator (comp/gen.c): a tree to stack-machine code. */
#ifndef COMP_GEN_H
#define COMP_GEN_H
#include "comp/ast.h"
#include "comp/code.h"

void gen_tree(Code *c, const Node *n); /* n's tree compiled: a checked one (comp/sema.h) */
void gen_icmp(Code *c, uint8_t rel, uint8_t *right); /* an INT relation, rel REL + mask */
void gen_fcmp(Code *c, uint8_t rel, uint8_t *right); /* a FLOAT relation, the right's load folded */
/* cm_lflt: the left INT operand as a FLOAT: its load (left, null none) in place, else I.TOF
   before right. Returns right, moved. */
uint8_t *gen_lfloat(Code *c, uint8_t *left, uint8_t *right);
#endif
