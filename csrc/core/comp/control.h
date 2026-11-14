/* Block statements compiled (comp/ctl.asm): IF blocks, SELECT CASE, DO, WHILE, EXIT; CALL, a
   record's copy (their references are comp/refs.c's); ON ERROR, events, RESUME, COMMON, SWAP. Each block
   statement finds its Block (blok_find) and jumps to the targets blok_pair gave it:
     IF c THEN         c, JF NXT
     ELSEIF c THEN     JMP END, c, JF NXT
     ELSE              JMP END
     SELECT CASE e     e to its value's variable #n (n its depth), JMP NXT
     CASE tests        JMP END, the tests or'd, JF NXT
     DO [WHILE|UNTIL c]    c, JF|JT END
     LOOP [WHILE|UNTIL c]  JMP NXT, or c, JT|JF NXT
     WHILE c, WEND     c, JF END; JMP NXT
     EXIT FOR, EXIT DO FPOP, JMP END; JMP END
   A direct line has no entries: its targets are none (FFFFFFFFh), an error when run. */
#ifndef COMP_CONTROL_H
#define COMP_CONTROL_H
#include "comp/comp.h"
#include "comp/ast.h"

/* A target none: line -1 when run. */
enum : uint32_t { NOTARGET = 0xFFFFFFFF };

typedef struct {
    uint8_t fieldet;   /* g_ctfet: expr_target's field (its offset in comp.tgt): ET_*, n */
    uint16_t fieldfix; /* g_ctffix */
    uint16_t selid;    /* g_ctvr: a SELECT's or SWAP's variable */
    uint8_t seltype;   /* g_cttp: its T_* */
} Ctrl;

bool ctrl_stmt(Session *S);    /* ct_stmt: the statement at comp.p if one of these; false none */
void ctrl_blockif(Session *S); /* ct_bif: a block IF's test (comp.p at THEN): JF its next clause */
bool ctrl_else(
    Session *S,
    uint32_t *target
); /* a one-line IF's ELSE: the target after it; false none */
void ctrl_jf(Session *S, uint32_t target); /* JF by comp.type; Type mismatch a string */
void ctrl_forskip(Session *S);             /* ct_fskip: FSKIP past FOR's NEXT, comp.kw the FOR */
Jump ctrl_jump(
    Session *S,
    uint32_t target
); /* ct_emitjump's: a target (ct_target's) as code holds it */
/* QBasic's ERR for a call of no procedure. */
enum : uint8_t { ERR_NO_FUNCTION = 18, ERR_NO_SUB = 35 };
void ctrl_undef(Session *S, uint8_t err); /* ct_undef: ERROR err when run: no such procedure */
void ctrl_swap(Session *S);               /* cm_swap: SWAP a, b */
void ctrl_recstore(Session *S);           /* rc_store: store to expr_target's field */
#endif
