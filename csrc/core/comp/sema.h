/* The semantic pass: a parsed tree's names resolved to the facts gen needs (Node.var, par, jump)
   and its types checked, every error raised, so gen (comp/codegen.h) gets a tree it compiles without
   one. The walk is gen's, so an error's place (comp.p) is where gen's was. */
#ifndef COMP_SEMA_H
#define COMP_SEMA_H
#include "comp/ast.h"

void sema_tree(Session *S, Node *n); /* Type mismatch where n's values do not fit */
#endif
