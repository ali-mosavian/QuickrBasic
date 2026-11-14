/* The code generator (comp/gen.c): a tree to stack-machine code. */
#ifndef COMP_GEN_H
#define COMP_GEN_H
#include "comp/ast.h"

typedef struct {
    uint8_t sh;          /* g_csh: the SHORT class of the code ending at shend */
    uint8_t *shend;      /* g_cshe */
    uint8_t shclass;     /* g_cshc: gen_shclass's answer */
    bool fixdbl;         /* g_xdbl: FIX kept a DOUBLE */
} Gen;

void gen_tree(Session *S, uint16_t node);  /* the tree at node compiled */
uint8_t gen_shclass(Session *S);           /* cm_shv: the value just compiled's SH_*, to gen.shclass too */
void gen_shortid(Session *S, uint16_t id); /* cm_shid: id's load just compiled: a SHORT's, class 1 */
void gen_icmp(Session *S, uint8_t rel, uint8_t *right); /* an INT relation, rel REL + mask */
void gen_fcmp(Session *S, uint8_t rel, uint8_t *right); /* a FLOAT relation, the right's load folded */
/* cm_lflt: the left INT operand as a FLOAT: its load (left, null none) in place, else I.TOF
   before right. Returns right, moved. */
uint8_t *gen_lfloat(Session *S, uint8_t *left, uint8_t *right);
#endif
