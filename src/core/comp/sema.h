/* The semantic pass's interface. How it works: sema.c. */
#ifndef COMP_SEMA_H
#define COMP_SEMA_H
#include "comp/ast.h"
typedef struct Comp Comp;

/* n's facts resolved; Type mismatch where its values do not fit. */
void sema_tree(Comp *C, Node *n);
/* A step's own facts, before gen: a statement's node is made unchecked; its children were checked. */
void sema_resolve(Comp *C, Node *n);
#endif
