/* Block statements compiled (comp/ctl.asm): IF blocks, SELECT CASE, DO, WHILE, EXIT; procedures'
   calls and arguments; records' references; ON ERROR, events, RESUME, COMMON, SWAP. Each block
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
#ifndef COMP_CTRL_H
#define COMP_CTRL_H
#include "comp/comp.h"

/* A target none: line -1 when run. */
enum : uint32_t { NOTARGET = 0xFFFFFFFF };

/* A record's reference: its field's ET_* (ET_REC + t a whole record of TYPE t), its offset, its
   STRING * n or a record's size. */
typedef struct {
    uint8_t et;
    uint16_t off;
    uint16_t fix;
} RecRef;

typedef struct {
    RecRef ref;          /* g_rfet, g_rfoff, g_rffix: ctrl_recref's */
    uint8_t fieldet;     /* g_ctfet: expr_target's field (its offset in comp.tgt): ET_*, n */
    uint16_t fieldfix;   /* g_ctffix */
    uint16_t selid;      /* g_ctvr: a SELECT's or SWAP's variable */
    uint8_t seltype;     /* g_cttp: its T_* */
    bool byval;          /* g_cbyv: the argument by value */
    uint16_t param;      /* g_cpar: arg's parameter */
} Ctrl;
extern Ctrl ctrl;

bool ctrl_stmt(void);          /* ct_stmt: the statement at comp.p if one of these; false none */
void ctrl_blockif(void);       /* ct_bif: a block IF's test (comp.p at THEN): JF its next clause */
bool ctrl_else(uint32_t *target); /* a one-line IF's ELSE: the target after it; false none */
void ctrl_jf(uint32_t target); /* JF by comp.type; Type mismatch a string */
void ctrl_forskip(void);       /* ct_fskip: FSKIP past FOR's NEXT, comp.kw the FOR */
void ctrl_fcall(void);         /* a FUNCTION's call in an expression; comp.type its result's */
void ctrl_dropargs(bool paren); /* ct_uarg: a DECLAREd-only call's arguments checked, dropped */
/* QBasic's ERR for a call of no procedure. */
enum : uint8_t { ERR_NO_FUNCTION = 18, ERR_NO_SUB = 35 };
void ctrl_undef(uint8_t err);  /* ct_undef: ERROR err when run: no such procedure */
void ctrl_swap(void);          /* cm_swap: SWAP a, b */
bool ctrl_recref(void);        /* rc_ref: a record reference at comp.p to ctrl.ref, its address's
                                  code; false none, comp.p and the code kept */
void ctrl_recload(void);       /* rc_load: the field's value; Type mismatch a whole record */
void ctrl_recstore(void);      /* rc_store: store to expr_target's field */
uint16_t ctrl_recarray(uint16_t name, uint8_t type); /* rc_avar: record array name's variable */
#endif
