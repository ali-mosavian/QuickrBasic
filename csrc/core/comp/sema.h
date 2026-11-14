/* The semantic pass's interface. How it works: sema.c. */
#ifndef COMP_SEMA_H
#define COMP_SEMA_H
#include "comp/ast.h"
typedef struct Comp Comp;

/* n's facts resolved; Type mismatch where its values do not fit. */
void sema_tree(Comp *C, Node *n);
/* n's facts resolved again, as the tables stand now: before gen, after names were made. */
void sema_resolve(Comp *C, Node *n);
#endif
