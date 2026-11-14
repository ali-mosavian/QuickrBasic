/* Control statements' compiler's interface. How it works: control.c. */
#ifndef COMP_CONTROL_H
#define COMP_CONTROL_H
#include "comp/comp.h"
#include "comp/ast.h"

/* A target none: line -1 when run. */
enum : uint32_t { NOTARGET = 0xFFFFFFFF };

typedef struct Ctrl {
    uint8_t fieldet;   /* g_ctfet: expr_target's field (its offset in comp.tgt): ET_*, n */
    uint16_t fieldfix; /* g_ctffix */
    uint16_t selid;    /* g_ctvr: a SELECT's or SWAP's variable */
    uint8_t seltype;   /* g_cttp: its T_* */
} Ctrl;

bool ctrl_stmt(Comp *C);    /* ct_stmt: the statement at comp.p if one of these; false none */
void ctrl_blockif(Comp *C); /* ct_bif: a block IF's test (comp.p at THEN): JF its next clause */
bool ctrl_else(
    Comp *C,
    uint32_t *target
); /* a one-line IF's ELSE: the target after it; false none */
void ctrl_jf(Comp *C, uint32_t target); /* JF by comp.type; Type mismatch a string */
void ctrl_forskip(Comp *C);             /* ct_fskip: FSKIP past FOR's NEXT, comp.kw the FOR */
Jump ctrl_jump(
    Comp *C,
    uint32_t target
); /* ct_emitjump's: a target (ct_target's) as code holds it */
/* QBasic's ERR for a call of no procedure. */
enum : uint8_t { ERR_NO_FUNCTION = 18, ERR_NO_SUB = 35 };
void ctrl_undef(Comp *C, uint8_t err); /* ct_undef: ERROR err when run: no such procedure */
void ctrl_swap(Comp *C);               /* cm_swap: SWAP a, b */
void ctrl_recstore(Comp *C);           /* rc_store: store to expr_target's field */
#endif
