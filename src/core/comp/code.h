/* Code writing's interface. How it works: code.c. */
#ifndef COMP_CODE_H
#define COMP_CODE_H
#include "defs.h"

typedef struct {
    uint8_t *buf;      /* cxb: the code's start */
    uint8_t *out;      /* (EDI) where the next byte goes */
    uint8_t *lim;      /* cxlim: the code's limit */
    bool full;         /* a byte was dropped at lim */
    uint8_t type;      /* cty: the value's T_* */
    uint8_t *iload;    /* cll: the last INT load emitted, its end */
    uint8_t *iloadend; /* clle */
    uint8_t *cmp;      /* ccmp: the last compare, its end (a one-line IF's CMPIF) */
    uint8_t *cmpend;   /* ccmpe */
    uint8_t sh;        /* g_csh: the SHORT class of the code ending at shend */
    uint8_t *shend;    /* g_cshe */
    uint8_t shclass;   /* g_cshc: code_shclass's answer */
    bool fixdbl;       /* g_xdbl: FIX kept a DOUBLE */
} Code;

/* A byte past lim dropped, full set */
static inline void code_byte(Code *const c, const uint8_t b)
{
    if (c->out >= c->lim)
        c->full = true;
    else
        *c->out++ = b;
}
static inline void code_op(Code *const c, const uint8_t op) { code_byte(c, op); }
void code_opb(Code *c, uint8_t op, uint8_t b);  /* op, a byte */
void code_opw(Code *c, uint8_t op, uint16_t w); /* op, a word */
/* op, variable id (an FV op's VCELL, an FA op's id and cell); a parameter's (par) X.LD.V, X.ST.V
   as V.LD, V.ST id kind */
void code_opid(Code *c, uint8_t op, uint32_t id, bool par);
void code_word(Code *c, uint16_t w);
void code_dword(Code *c, uint32_t d);
void code_insert(Code *c, uint8_t b, uint8_t *at); /* b at at, the code after it moved up */
void code_int(Code *c, uint32_t n);                /* an INT load: I.LD.U8, I.LD.I16 or I.LD.I32 */
void code_call(Code *c, uint8_t k);                /* CALL k */
uint8_t *code_intleaf(Code *c); /* the code's last op if an INT load, else null */
uint8_t code_shclass(Code *c);  /* cm_shv: the value's SH_*, to shclass too */

/* The value (of code.type, a number) converted. */
void code_toint(Code *c);
void code_tofloat(Code *c); /* an INT load just made loads a FLOAT instead */
void code_todouble(Code *c);
void code_tobyte(Code *c);
void code_toaddr(Code *c);
void code_toinfo(Code *c, uint8_t info); /* as a variable of TVAR.info info holds it */
void code_dtest(Code *c);                /* cm_dnz: a DOUBLE test as an INT, D.NZ */

/* Type's one of four: INT, FLOAT, DOUBLE, STRING. */
static inline uint8_t code_pick(
    const uint8_t type,
    const uint8_t i,
    const uint8_t f,
    const uint8_t d,
    const uint8_t s
)
{
    switch (type) {
    case T_INT:
        return i;

    case T_FLT:
        return f;

    case T_DBL:
        return d;

    default:
        return s;
    }
}
#endif
