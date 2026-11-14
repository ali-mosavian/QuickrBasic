/* The semantic pass's interface. How it works: sema.c. */
#ifndef COMP_SEMA_H
#define COMP_SEMA_H
#include "comp/ast.h"

void sema_tree(Session *S, Node *n); /* Type mismatch where n's values do not fit */
#endif
