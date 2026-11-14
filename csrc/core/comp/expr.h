/* The expression compiler (comp/expr.asm): precedence climbing over comp.p. Every operand is
   pushed; an operator takes the cells it needs. A right operand that is one load folds into the
   operator: F.ADD.V, I.CMP.U8 ... */
#ifndef COMP_EXPR_H
#define COMP_EXPR_H
#include "comp/comp.h"

/* A SHORT's class (expr.shclass): a value of no SHORT, a SHORT's, a literal that fits one. */
enum : uint8_t { SH_NONE = 0, SH_SHORT = 1, SH_LIT = 2 };
/* A relation's operator byte: REL and its DC_* mask. */
enum : uint8_t { REL = 0x80, REL_MASK = 7 };

typedef struct {
    uint8_t sh;          /* g_csh: the SHORT class of the code ending at shend */
    uint8_t *shend;      /* g_cshe */
    uint8_t shclass;     /* g_cshc: expr_shclass's answer */
    uint8_t *kend;       /* g_ckon: a constant's code ends here */
    bool dbltgt;         /* g_cwd: the assignment's target is a DOUBLE */
    bool bothk;          /* g_cbk: the operator's operands are constants */
    bool fixdbl;         /* g_xdbl: FIX kept a DOUBLE */
    uint32_t vakind;     /* g_vakind: cm_addr's VA_* << 24 + its bytes */
} Expr;

/* By kind, SK_DBL a DOUBLE: its T_*. */
extern const uint8_t expr_ktype[5];

void expr_compile(Session *S);   /* cm_expr: an expression at comp.p */
void expr_assign(Session *S);    /* cm_lexpr: an assignment's value, for a DOUBLE target as BC */
void expr_paren(Session *S);     /* ( expr ) */
uint8_t expr_subs(Session *S);   /* cm_subs0: ( e {, e} ), each an INT; how many */
void expr_load(Session *S);      /* a variable's, an element's, a FUNCTION's value */
void expr_target(Session *S);    /* cm_lval: a store's target, its subscripts compiled */
void expr_string(Session *S);    /* cm_str1: a literal after its quote, S.LD.TXT */
void expr_varaddr(Session *S);   /* cm_va: a variable's place for GET, PUT, FIELD: ( -- addr kind ) */
uint8_t expr_shclass(Session *S); /* cm_shv: the value just compiled's SH_*, to expr.shclass too */
void expr_shortid(Session *S, uint16_t id); /* cm_shid: id's load just compiled: a SHORT's, class 1 */
void expr_icmp(Session *S, uint8_t rel, uint8_t *right); /* an INT relation, rel REL + mask */
void expr_fcmp(Session *S, uint8_t rel, uint8_t *right); /* a FLOAT relation, the right's load folded */
/* cm_lflt: the left INT operand as a FLOAT: its load (left, null none) in place, else I.TOF
   before right. Returns right, moved. */
uint8_t *expr_lfloat(Session *S, uint8_t *left, uint8_t *right);
#endif
