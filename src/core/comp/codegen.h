/* The code generator's interface. How it works: codegen.c. */
#ifndef COMP_CODEGEN_H
#define COMP_CODEGEN_H
#include "comp/ast.h"
#include "comp/code.h"

void cg_tree(Code *c, const Node *n); /* n's tree compiled: a checked one (comp/sema.h) */
/* A CONST's N_STORE n in two: its value converted for the target; then, the value folded, the
   store. */
void cg_store_value(Code *c, const Node *n);
void cg_store_op(Code *c, const Node *n);
void cg_icmp(Code *c, uint8_t rel, uint8_t *right); /* an INT relation, rel REL + mask */
void cg_fcmp(Code *c, uint8_t rel, uint8_t *right); /* a FLOAT relation, the right's load folded */
/* A compare that ends the code made its CMPIF: false on, the line ends. False: none. */
bool cg_cmpif(Code *c);
/* The FLOAT right operand's load at right that folds: 1 .V, 2 .VI, 3 .U8, 4 .F32; 0 none. */
uint8_t cg_fltkind(Code *c, const uint8_t *right);
/* cm_lflt: the left INT operand as a FLOAT: its load (left, null none) in place, else I.TOF
   before right. Returns right, moved. */
uint8_t *cg_lfloat(Code *c, uint8_t *left, uint8_t *right);
#endif
