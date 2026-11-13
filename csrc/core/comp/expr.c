/* The expression compiler (comp/expr.asm). Precedences: OR 46h, AND 50h, NOT 5Ah, > = < 64h,
   + - 78h, * / 7Bh, unary - 7Dh, ^ 7Fh. */
#include "comp/expr.h"
#include "comp/blok.h"
#include "comp/ctrl.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "io/repl.h"
#include "mem/scop.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/dubl.h"

Expr expr;
const uint8_t expr_ktype[5] = {T_FLT, T_INT, T_STR, T_DBL, T_DBL};

/* An operator's index: + - * / ^ AND OR > = < from TK_PLUS, then \ MOD XOR EQV IMP. A relation
   compiles as REL and its DC_* mask. */
enum : uint8_t {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW, OP_AND, OP_OR, OP_GT, OP_EQ, OP_LT,
    OP_IDIV, OP_MOD, OP_XOR, OP_EQV, OP_IMP, NOPS,
};
/* Precedences: the higher binds tighter. */
enum : uint8_t {
    P_IMP = 0x40, P_EQV = 0x42, P_XOR = 0x44, P_OR = 0x46, P_AND = 0x50, P_NOT = 0x5A,
    P_REL = 0x64, P_ADD = 0x78, P_MOD = 0x79, P_IDIV = 0x7A, P_MUL = 0x7B, P_NEG = 0x7D,
    P_POW = 0x7F,
};
static const uint8_t prectab[NOPS] = {
    [OP_ADD] = P_ADD,   [OP_SUB] = P_ADD, [OP_MUL] = P_MUL, [OP_DIV] = P_MUL, [OP_POW] = P_POW,
    [OP_AND] = P_AND,   [OP_OR] = P_OR,   [OP_GT] = P_REL,  [OP_EQ] = P_REL,  [OP_LT] = P_REL,
    [OP_IDIV] = P_IDIV, [OP_MOD] = P_MOD, [OP_XOR] = P_XOR, [OP_EQV] = P_EQV, [OP_IMP] = P_IMP,
};
static const uint8_t xop[] = {O_I_DIV, O_I_MOD, O_I_XOR, O_I_EQV, O_I_IMP}; /* from OP_IDIV */
static const uint8_t relbit[] = {DC_GT, DC_EQ, DC_LT};                      /* from OP_GT */
static const uint8_t fcmpk[4] = {O_F_CMP_V, O_F_CMP_VI, O_F_CMP_U8, O_F_CMP_F32};
static const uint8_t lda1[5] = {O_F_LD_A1, O_I_LD_A1, O_S_LD_A1, O_D_LD_A1, O_D_LD_A1};
static const uint8_t ldan[5] = {O_F_LD_A, O_I_LD_A, O_S_LD_A, O_D_LD_A, O_D_LD_A};
/* SGN .. CHR$ of a DOUBLE: D.FN's operand, DFN_STR STR$'s, DFN_NONE none. */
enum : uint8_t { DFN_STR = 0x80, DFN_NONE = 0xFF };
#define FN(tk) [TK_##tk - TK_SGN]
static const uint8_t dfnk[20] = {
    FN(SGN) = DF_SGN,    FN(INT) = DF_INT,    FN(ABS) = DF_ABS,    FN(USR) = DFN_NONE,
    FN(FRE) = DFN_NONE,  FN(POS) = DFN_NONE,  FN(SQR) = DF_SQR,    FN(RND) = DFN_NONE,
    FN(LOG) = DF_LOG,    FN(EXP) = DF_EXP,    FN(COS) = DF_COS,    FN(SIN) = DF_SIN,
    FN(TAN) = DF_TAN,    FN(ATN) = DF_ATN,    FN(PEEK) = DFN_NONE, FN(LEN) = DFN_NONE,
    FN(STRS) = DFN_STR,  FN(VAL) = DFN_NONE,  FN(ASC) = DFN_NONE,  FN(CHRS) = DFN_NONE,
};

/* Per function, TK_SGN on: its argument's conversion (FA_*), its result (FR_*). */
enum { FA_F, FA_A, FA_S, FA_Y, FA_B, FA_N, FA_I, FA_MASK = 0x0F };
enum { FR_I = T_INT, FR_F = T_FLT, FR_S = 0xB0, FR_MASK = 0xF0 };
static const uint8_t fnsig[20] = {
    FN(SGN) = FA_F + FR_I,  FN(INT) = FA_F + FR_F,  FN(ABS) = FA_F + FR_F,  FN(USR) = FA_F + FR_F,
    FN(FRE) = FA_Y + FR_F,  FN(POS) = FA_N + FR_I,  FN(SQR) = FA_F + FR_F,  FN(RND) = FA_F + FR_F,
    FN(LOG) = FA_F + FR_F,  FN(EXP) = FA_F + FR_F,  FN(COS) = FA_F + FR_F,  FN(SIN) = FA_F + FR_F,
    FN(TAN) = FA_F + FR_F,  FN(ATN) = FA_F + FR_F,  FN(PEEK) = FA_I + FR_I, FN(LEN) = FA_S + FR_I,
    FN(STRS) = FA_F + FR_S, FN(VAL) = FA_S + FR_F,  FN(ASC) = FA_S + FR_I,  FN(CHRS) = FA_B + FR_S,
};
#undef FN

/* QBasic's other functions: the token (100h + n for TK_X n), the CALL (0 none), the arguments at
   least (| XF_LEAD: a leading one optional), at most (0: no ( ); least 0: ( ) optional), the
   result, each argument's kind: I an INT, W a SHORT's 16 bits, F a FLOAT, D a DOUBLE, S a string,
   X a string or a number, G FIX's, H a file number, R an array. */
typedef struct {
    uint16_t tok;
    uint8_t k, min, max, res;
    char arg[3];
} XFn;
enum : uint8_t { XF_LEAD = 0x80, XF_MIN = 0x7F };
static const XFn xfns[] = {
    {0x100 + TK1_CINT, 0, 1, 1, T_INT, "I"},
    {0x100 + TK1_CLNG, 0, 1, 1, T_INT, "I"},
    {0x100 + TK1_CSNG, 0, 1, 1, T_FLT, "F"},
    {0x100 + TK1_CDBL, 0, 1, 1, T_DBL, "D"},
    {TK_FIX, 0, 1, 1, T_FLT, "G"},
    {TK_INSTR, K_INSTR, XF_LEAD | 2, 3, T_INT, {'I', 'S', 'S'}},
    {TK_LBOUND, K_LBND, 1, 2, T_INT, "RI"},
    {TK_UBOUND, K_UBND, 1, 2, T_INT, "RI"},
    {0x100 + TK1_SPACES, K_SPACE, 1, 1, T_STR, "I"},
    {0x100 + TK1_STRINGS, K_STRG, 2, 2, T_STR, "IX"},
    {0x100 + TK1_LTRIMS, K_LTRIM, 1, 1, T_STR, "S"},
    {0x100 + TK1_RTRIMS, K_RTRIM, 1, 1, T_STR, "S"},
    {0x100 + TK1_UCASES, K_UCASE, 1, 1, T_STR, "S"},
    {0x100 + TK1_LCASES, K_LCASE, 1, 1, T_STR, "S"},
    {0x100 + TK1_HEXS, K_HEX, 1, 1, T_STR, "W"},
    {0x100 + TK1_OCTS, K_OCT, 1, 1, T_STR, "W"},
    {0x100 + TK1_ERR, K_ERR, 0, 0, T_INT, ""},
    {0x100 + TK1_ERL, K_ERL, 0, 0, T_INT, ""},
    {0x100 + TK1_TIMER, K_TIMER, 0, 0, T_FLT, ""},
    {0x100 + TK1_DATES, K_DATE, 0, 0, T_STR, ""},
    {0x100 + TK1_TIMES, K_TIME, 0, 0, T_STR, ""},
    {TK_INKEYS, K_INKEY, 0, 0, T_STR, ""},
    {0x100 + TK1_COMMANDS, K_CMD, 0, 0, T_STR, ""},
    {0x100 + TK1_ENVIRONS, K_ENVS, 1, 1, T_STR, "X"},
    {0x100 + TK1_SETMEM, K_SETMEM, 1, 1, T_INT, "I"},
    {0x100 + TK1_ERDEV, K_ERDEV, 0, 0, T_INT, ""},
    {0x100 + TK1_ERDEVS, K_ERDEVS, 0, 0, T_STR, ""},
    {0x100 + TK1_MKIS, K_MKI, 1, 1, T_STR, "I"},
    {0x100 + TK1_MKLS, K_MKL, 1, 1, T_STR, "I"},
    {0x100 + TK1_MKSS, K_MKS, 1, 1, T_STR, "F"},
    {0x100 + TK1_MKDS, K_MKD, 1, 1, T_STR, "D"},
    {0x100 + TK1_MKSMBFS, K_MKSM, 1, 1, T_STR, "F"},
    {0x100 + TK1_MKDMBFS, K_MKDM, 1, 1, T_STR, "D"},
    {0x100 + TK1_CVI, K_CVI, 1, 1, T_INT, "S"},
    {0x100 + TK1_CVL, K_CVL, 1, 1, T_INT, "S"},
    {0x100 + TK1_CVS, K_CVS, 1, 1, T_FLT, "S"},
    {0x100 + TK1_CVD, K_CVD, 1, 1, T_DBL, "S"},
    {0x100 + TK1_CVSMBF, K_CVSM, 1, 1, T_FLT, "S"},
    {0x100 + TK1_CVDMBF, K_CVDM, 1, 1, T_DBL, "S"},
    {0x100 + TK1_EOF, K_EOF, 1, 1, T_INT, "H"},
    {0x100 + TK1_LOF, K_LOF, 1, 1, T_INT, "H"},
    {0x100 + TK1_LOC, K_LOC, 1, 1, T_INT, "H"},
    {0x100 + TK1_SEEK, K_SEEKF, 1, 1, T_INT, "H"},
    {0x100 + TK1_FREEFILE, K_FREEF, 0, 0, T_INT, ""},
    {0x100 + TK1_INP, K_INP, 1, 1, T_INT, "I"},
    {0x100 + TK1_INPUTS, K_INPS, 1, 2, T_STR, "IH"},
    {0x100 + TK1_FILEATTR, K_FATTR, 2, 2, T_INT, "HI"},
};

[[noreturn]] static void synerr(void) { repl_error(E_SN); }
[[noreturn]] static void mismatch(void) { repl_error(E_TM); }

static void prec(uint8_t p);
static void unary(void);

void expr_compile(void)
{
    const bool dbl = expr.dbltgt; /* not an assignment's: its own */
    expr.dbltgt = false;
    prec(0);
    expr.dbltgt = dbl;
}

void expr_assign(void)
{
    expr.dbltgt = comp.tgtinfo & SK_DBL;
    prec(0);
    expr.dbltgt = false;
}

uint8_t expr_shclass(void)
{
    expr.shclass = comp.out == expr.shend ? expr.sh : SH_NONE;
    return expr.shclass;
}

/* Id's load just compiled: a SHORT's, class 1. */
static void shortid(const uint16_t id)
{
    if (symb_var(id)->type == ET_SH) {
        expr.sh = SH_SHORT;
        expr.shend = comp.out;
    }
}

static bool is_rel(const uint8_t rel) { return rel & REL; }
static bool is_logic(const uint8_t rel) { return rel == OP_AND || rel == OP_OR; }
static bool is_xop(const uint8_t rel) { return rel >= OP_IDIV && rel < REL; } /* \ MOD XOR EQV IMP */

/* cm_sop: operator rel's result is a SHORT's when both operands are SHORTs or literals that fit,
   one a SHORT: + - * \ MOD, QBasic's INTEGER arithmetic. lsh: the left's class. */
static uint8_t shortop(const uint8_t rel, const uint8_t lsh)
{
    const uint8_t rsh = expr_shclass();
    if (!lsh || !rsh || !((lsh | rsh) & SH_SHORT))
        return SH_NONE;
    const bool arith = rel <= OP_MUL || rel == OP_IDIV || rel == OP_MOD;
    return arith ? SH_SHORT : SH_NONE;
}

static bool isright(const uint8_t op, const uint32_t size, const uint8_t *const right)
{
    return (uint32_t)(comp.out - right) == size && *right == op;
}

uint8_t *expr_lfloat(uint8_t *const left, uint8_t *const right)
{
    if (left) {
        if (*left == O_I_LD_V) {
            *left = O_F_LD_VI;
            return right;
        }
        if (*left == O_I_LD_U8) {
            *left = O_F_LD_U8;
            return right;
        }
    }
    comp_insert(O_I_TOF, right);
    return right + 1;
}

/* INT operator o (0..5: + - * / AND OR): the right operand's load folds into .V or .U8. */
static void intop(const uint8_t o, uint8_t *const right)
{
    comp.type = T_INT;
    if (isright(O_I_LD_V, 3, right))
        *right = O_I_ADD_V + o;
    else if (isright(O_I_LD_U8, 2, right))
        *right = O_I_ADD_U8 + o;
    else
        comp_op(O_I_ADD + o);
}

void expr_icmp(const uint8_t rel, uint8_t *const right)
{
    comp.type = T_INT;
    const uint8_t m = rel & REL_MASK;
    uint8_t op;
    if (isright(O_I_LD_V, 3, right))
        op = O_I_CMP_V;
    else if (isright(O_I_LD_U8, 2, right))
        op = O_I_CMP_U8;
    else {
        comp.cmp = comp.out;
        comp_opb(O_I_CMP, m);
        comp.cmpend = comp.out;
        return;
    }
    comp.cmp = right;
    *right = op;
    comp_byte(m);
    comp.cmpend = comp.out;
}

/* The right operand's load that folds: the first op of its group, F_ADD_V .. F_ADD_F32; 0 none. */
static uint8_t fltkind(const uint8_t *const right)
{
    if (isright(O_F_LD_V, 3, right))
        return O_F_ADD_V;
    if (isright(O_F_LD_VI, 3, right))
        return O_F_ADD_VI;
    if (isright(O_F_LD_U8, 2, right))
        return O_F_ADD_U8;
    if (isright(O_F_LD_F32, 5, right))
        return O_F_ADD_F32;
    return 0;
}

/* FLOAT operator o (0..3: + - * /): the right's load folded into .V, .VI, .U8 or .F32. */
static void fltop(const uint8_t o, uint8_t *const right)
{
    comp.type = T_FLT;
    const uint8_t k = fltkind(right);
    if (k)
        *right = k + o;
    else
        comp_op(O_F_ADD + o);
}

void expr_fcmp(const uint8_t rel, uint8_t *const right)
{
    comp.type = T_INT;
    const uint8_t m = rel & REL_MASK;
    const uint8_t k = fltkind(right);
    if (k) {
        comp.cmp = right;
        *right = fcmpk[(k - O_F_ADD_V) >> 2];
        comp_byte(m);
    } else {
        comp.cmp = comp.out;
        comp_opb(O_F_CMP, m);
    }
    comp.cmpend = comp.out;
}

/* String operator rel: + joins, a relation compares. */
static void strop(const uint8_t rel)
{
    if (!(comp.type & T_STR))
        mismatch();
    if (rel == OP_ADD)
        comp_op(O_S_CAT);
    else if (!is_rel(rel))
        mismatch();
    else {
        comp.cmp = comp.out;
        comp_opb(O_S_CMP, rel & REL_MASK);
        comp.cmpend = comp.out;
        comp.type = T_INT;
    }
}

/* A DOUBLE either side; or for a DOUBLE target ^, and unless of constants / or a SINGLE's. */
static bool wants_double(const uint8_t rel, const uint8_t ltype)
{
    if (ltype == T_DBL || comp.type == T_DBL)
        return true;
    if (!expr.dbltgt)
        return false;
    return rel == OP_POW ||
           (!expr.bothk && (rel == OP_DIV || ltype & T_FLT || comp.type == T_FLT));
}

/* Number operator rel in DOUBLEs, the left converted before right. */
static void dblop(const uint8_t rel, const uint8_t ltype, uint8_t *const right)
{
    if (ltype != T_DBL)
        comp_insert(ltype == T_INT ? O_I_TOD : O_F_TOD, right);
    comp_todouble();
    comp.iload = comp.iloadend = nullptr;
    if (rel == OP_POW)
        comp_op(O_D_POW);
    else if (is_rel(rel)) {
        comp_opb(O_D_CMP, rel & REL_MASK);
        comp.type = T_INT;
    } else
        comp_op(O_D_ADD + rel);
}

/* Number operator rel: DOUBLEs, ^ a SINGLE's, INTs (but /, a FLOAT as QBasic's), or FLOATs. */
static void numop(const uint8_t rel, const uint8_t ltype, uint8_t *const left, uint8_t *right)
{
    if (comp.type & T_STR)
        mismatch();
    const bool lint = !(ltype & T_FLT);
    if (wants_double(rel, ltype))
        dblop(rel, ltype, right);
    else if (rel == OP_POW) {
        if (lint)
            expr_lfloat(left, right);
        comp_tofloat();
        comp_call(K_FPOW);
        comp.type = T_FLT;
    } else if (lint && comp.type == T_INT && rel != OP_DIV) {
        if (is_rel(rel))
            expr_icmp(rel, right);
        else
            intop(rel, right);
    } else {
        if (lint)
            right = expr_lfloat(left, right);
        comp_tofloat();
        if (is_rel(rel))
            expr_fcmp(rel, right);
        else
            fltop(rel, right);
    }
}

/* The operator at c's index; false: none. */
static bool operator_of(const uint8_t c, uint8_t *const op)
{
    if (c == '\\')
        *op = OP_IDIV;
    else if (c >= TK_MOD && c <= TK_IMP)
        *op = c - TK_MOD + OP_MOD;
    else if (c >= TK_PLUS && c <= TK_LT)
        *op = c - TK_PLUS;
    else
        return false;
    return true;
}

/* A run of > = < from op: REL and their mask, comp.p past it. */
static uint8_t relation(uint8_t op)
{
    uint8_t rel = REL;
    for (;;) {
        rel |= relbit[op - OP_GT];
        const uint8_t c = comp_curn();
        if (c < TK_GT || c > TK_LT)
            return rel;
        op = c - TK_PLUS;
    }
}

/* Operator rel after its left operand: its right one at precedence p, then the operator. */
static void binary(const uint8_t rel, const uint8_t p)
{
    if (is_logic(rel) || is_xop(rel))
        comp_toint();
    const bool leftk = comp.out == expr.kend; /* the left a constant */
    const uint8_t ltype = comp.type;
    uint8_t *const left = comp_intleaf();
    const uint8_t lsh = expr_shclass();
    uint8_t *const right = comp.out;
    prec(p);
    const uint8_t sh = shortop(rel, lsh);
    expr.bothk = rel != OP_POW && leftk && comp.out == expr.kend; /* ^ not folded */
    if (is_xop(rel)) {
        comp_toint();
        comp_op(xop[rel - OP_IDIV]);
    } else if (is_logic(rel)) {
        comp_toint();
        intop(rel - 1, right); /* AND, OR: O_I_ADD's 4th, 5th on */
    } else if (ltype == T_STR)
        strop(rel);
    else
        numop(rel, ltype, left, right);
    if (sh && comp.type == T_INT) /* a SHORT's result checked */
        comp_op(O_I_CHK16);
    expr.sh = sh;
    expr.shend = comp.out;
    expr.kend = expr.bothk ? comp.out : nullptr; /* of constants: one */
    expr.bothk = false;
}

/* cm_p: compile while the operators bind tighter than precedence p. */
static void prec(const uint8_t p)
{
    if (++comp.depth > COMP_MAXDEPTH)
        repl_error(E_OM);
    unary();
    uint8_t op;
    while (operator_of(comp_cur(), &op) && p < prectab[op]) {
        uint8_t rel = op;
        if (op >= OP_GT && op <= OP_LT)
            rel = relation(op);
        else
            comp.p++;
        binary(rel, prectab[op]);
    }
    comp.depth--;
}

void expr_paren(void)
{
    comp_open();
    expr_compile();
    comp_close();
}

uint8_t expr_subs(void)
{
    comp_open();
    uint8_t n = 0;
    do {
        n++;
        expr_compile();
        comp_toint();
    } while (comp_optcomma());
    comp_close();
    return n;
}

void expr_string(void)
{
    comp.type = T_STR;
    const uint8_t *const start = comp.p;
    const uint8_t *q = start;
    const uint32_t len = strg_span(&q, '"', '"');
    comp_opb(O_S_LD_TXT, (uint8_t)len);
    comp_dword(addr_of(start));
    comp.p = strg_skipq(q);
}

/* A FUNCTION DECLAREd only: its arguments, Function not defined when run; its result's id. */
static uint16_t undeclared(void)
{
    const uint16_t id = comp_var(1, 0);
    if (*comp.p == '(') {
        comp.p++;
        ctrl_dropargs(true);
        comp_close();
    }
    ctrl_undef(QB_NO_FUNCTION);
    return id;
}

/* A variable's value, its id's: X.LD.V, or the subscripts and X.LD.A1 id, X.LD.A id n. */
static void loadvar(const uint16_t id)
{
    const uint8_t info = symb_var(id)->info;
    if (info & VI_ARR) {
        const uint8_t n = expr_subs();
        const uint8_t k = info & (SK_DBL | VK_MASK);
        comp.type = expr_ktype[k];
        if (n == 1)
            comp_opid(lda1[k], id);
        else {
            comp_opid(ldan[k], id);
            comp_byte(n);
        }
        shortid(id);
        return;
    }
    if (info & SK_DBL) {
        comp.type = T_DBL;
        comp_opid(O_D_LD_V, id);
        return;
    }
    const uint8_t k = info & VK_MASK;
    if (k == VK_INT) {
        comp.iload = comp.out;
        comp_opid(O_I_LD_V, id);
        comp.iloadend = comp.out;
        comp.type = T_INT;
        shortid(id);
    } else if (k == VK_STR) {
        comp.type = T_STR;
        comp_opid(O_S_LD_V, id);
    } else {
        comp.type = T_FLT;
        comp_opid(O_F_LD_V, id);
    }
}

void expr_load(void)
{
    if (comp_cur() == M_VAR) { /* a FUNCTION's name: its call, but its result in it */
        const Name *const n = symb_name(comp_nameof(comp.p));
        if (n->flags & (NM_FUNC | NM_FN)) {
            if (!n->proc) {
                loadvar(undeclared());
                return;
            }
            if (*comp_skipname(comp.p) == '(' || n->proc != comp.proc) {
                ctrl_fcall();
                return;
            }
        }
    }
    if (comp_cur() == M_VAR && ctrl_recref()) {
        ctrl_recload();
        return;
    }
    loadvar(comp_name());
}

void expr_target(void)
{
    if (comp_cur() == M_VAR && ctrl_recref()) { /* a record's field: its address */
        comp.tgt = ctrl.ref.off;
        comp.tgtsubs = TGT_FIELD;
        ctrl.fieldet = ctrl.ref.et;
        if (ctrl.ref.et >= ET_REC)
            mismatch();
        ctrl.fieldfix = ctrl.ref.fix;
        comp.tgtinfo = comp_etkind[ctrl.ref.et];
        return;
    }
    const uint16_t id = comp_name();
    if (!(symb_var(id)->info & VI_ARR)) {
        comp_target(id);
        return;
    }
    const uint8_t n = expr_subs(); /* may compile a call with its own target */
    comp_target(id);
    comp.tgtsubs = n;
}


/* Variable id's place: its kind and bytes; a string's Desc and its STRING * n. */
static uint32_t vakind(const uint16_t id)
{
    const Var *const v = symb_var(id);
    if (v->info & SK_DBL)
        return va_pack(VA_RAW, 8);
    switch (v->info & VK_MASK) {
    case VK_INT: /* 2 bytes but a LONG's */
        return va_pack(v->type == ET_I4 ? VA_RAW : VA_I2, 4);
    case VK_STR:
        return va_pack(VA_DESC, scop_fix(v->name, v->info & VI_ARR));
    default:
        return va_pack(VA_RAW, 4);
    }
}

/* ctrl.ref's field: its bytes. */
static uint32_t field_size(void)
{
    const uint8_t et = ctrl.ref.et;
    return et >= ET_REC || et == ET_SD ? ctrl.ref.fix : decl_etsize[et];
}

/* cm_addr: a variable's address: a record's or a field's code, else its slot's, its Desc's. */
static void varaddr(void)
{
    if (comp_cur() != M_VAR)
        synerr();
    if (ctrl_recref()) {
        expr.vakind = va_pack(ctrl.ref.et == ET_I2 ? VA_I2 : VA_RAW, field_size());
        if (ctrl.ref.off) {
            comp_int(ctrl.ref.off);
            comp_op(O_I_ADD);
        }
        return;
    }
    const uint16_t id = comp_var(0, 0);
    expr.vakind = vakind(id);
    if (symb_var(id)->info & VI_ARR) { /* an element: RA.ADDR's */
        const uint8_t n = expr_subs();
        comp_opw(O_RA_ADDR, id);
        comp_byte(n);
    } else if (comp_ispar(id)) /* a parameter's: its cell, the argument's */
        comp_opid(O_V_ADDR, id);
    else
        comp_int(symb_cell(id));
}

void expr_varaddr(void)
{
    varaddr();
    comp_int(expr.vakind);
}

/* The value as kind k (an XFn's). */
static void xarg(const char k)
{
    switch (k) {
    case 'D':
        comp_todouble();
        return;
    case 'G': /* FIX's: a DOUBLE's D.FN 3, else CALL FIX */
        if (comp.type == T_DBL) {
            expr.fixdbl = true;
            comp_opb(O_D_FN, DF_FIX);
        } else {
            comp_tofloat();
            comp_call(K_FIX);
        }
        return;
    case 'W': /* HEX$, OCT$: a SHORT's 16 bits */
        expr_shclass();
        comp_toint();
        if (expr.shclass == SH_SHORT) {
            comp_int(0xFFFF);
            comp_op(O_I_AND);
        }
        return;
    case 'I':
        comp_toint();
        return;
    case 'F':
        comp_tofloat();
        return;
    case 'S':
        comp_tostring();
        return;
    }
    if (comp.type & T_STR) /* X: a string or a number, then 1 or 0 */
        comp_int(1);
    else {
        comp_toint();
        comp_int(0);
    }
}

/* The function token at comp.p (FFh n as 100h + n), comp.p past it: its XFn. */
static const XFn *xfn_at(void)
{
    uint16_t tok = *comp.p;
    if (tok == TK_X)
        tok = 0x100 + *++comp.p;
    comp.p++;
    for (size_t i = 0; i < countof(xfns); i++)
        if (xfns[i].tok == tok)
            return &xfns[i];
    synerr();
}

/* f's ( arguments ), each converted to its kind; their count after them when it varies. */
static void xfn_args(const XFn *const f)
{
    comp_open();
    uint32_t n = 0, i = 0; /* the arguments, the kind's index */
    for (;;) {
        char k = f->arg[i];
        if (k == 'R') { /* an array, n or n(): its id */
            if (comp_cur() != M_VAR)
                synerr();
            const uint16_t id = comp_var(2, 0);
            if (*comp.p == '(') {
                if (comp.p[1] != ')')
                    synerr();
                comp.p += 2;
            }
            comp_int(id);
        } else {
            if (k == 'H') { /* a file number, its # or not */
                if (comp_cur() == '#')
                    comp.p++;
                k = 'I';
            }
            expr_compile();
            if (!n && f->min & XF_LEAD && comp.type == T_STR && k != 'S') /* a leading optional
                                                                             left out */
                k = f->arg[++i];
            xarg(k);
        }
        n++;
        i++;
        if (i >= f->max || !comp_optcomma())
            break;
    }
    if (i < (f->min & XF_MIN))
        synerr();
    comp_close();
    if ((f->min & XF_MIN) != f->max)
        comp_int(n);
}

/* QBasic's other functions, by xfns: f(args), then CALL k, or none. */
static void xfunc(void)
{
    const XFn *const f = xfn_at();
    if (f->max && ((f->min & XF_MIN) || comp_cur() == '('))
        xfn_args(f);
    else if (f->max) /* all optional, ( ) left out */
        comp_int(0);
    if (f->k)
        comp_call(f->k);
    comp.type = expr.fixdbl ? T_DBL : f->res;
    expr.fixdbl = false;
    comp.iload = comp.iloadend = nullptr;
}

/* Function f's (TK_SGN on) argument converted; comp.type its result. */
static void farg(const uint8_t f)
{
    const uint8_t sig = fnsig[f];
    switch (sig & FA_MASK) {
    case FA_F: comp_tofloat(); break;
    case FA_A: comp_toaddr(); break;
    case FA_S: comp_tostring(); break;
    case FA_B: comp_tobyte(); break;
    case FA_N: /* POS's: a number */
        if (comp.type & T_STR)
            mismatch();
        break;
    case FA_I: comp_toint(); break;
    }
    const uint8_t res = sig & FR_MASK;
    comp.type = res == FR_S ? T_STR : res;
}

/* LEN(r): a record's, a field's, a number's size; false: LEN's own call, nothing compiled. */
static bool lensize(void)
{
    const uint8_t *const p = comp.p;
    uint8_t *const out = comp.out;
    comp.p++;
    if (comp_cur() == '(' && comp_curn() == M_VAR) {
        if (ctrl_recref()) {
            if (comp_cur() == ')') {
                comp.p++;
                comp_int(field_size());
                return true;
            }
        } else { /* a scalar: % 2, & ! 4, # 8 */
            const uint16_t id = comp_var(0, 0);
            if (!(symb_var(id)->info & VI_ARR)) {
                const uint32_t v = vakind(id);
                if (comp_cur() == ')' && va_kind(v) != VA_DESC) {
                    comp.p++;
                    comp_int(va_kind(v) == VA_I2 ? 2 : (uint8_t)v);
                    return true;
                }
            }
        }
    }
    comp.p = p;
    comp.out = out;
    return false;
}

/* SGN .. MID$, from cm_un's @cu_7a: f the function, TK_SGN on. */
static void func(const uint8_t f)
{
    if (f == TK_RND - TK_SGN && comp.p[1] != '(') { /* RND alone: RND(1) */
        comp.p++;
        comp_int(1);
        farg(f);
        comp_call(K_FN + f);
        return;
    }
    comp.p++;
    comp_open();
    expr_compile();
    if (f >= TK_MID - TK_SGN - 2) { /* LEFT$, RIGHT$, MID$ */
        comp_comma();
        comp_tostring();
        expr_compile();
        comp_toaddr();
        uint8_t k = f;
        if (f == TK_MID - TK_SGN) {
            if (comp_optcomma()) {
                expr_compile();
                comp_toaddr();
            } else
                k = K_MID2 - K_FN;
        }
        comp_close();
        comp.type = T_STR;
        comp_call(K_FN + k);
        return;
    }
    comp_close();
    const uint8_t df = dfnk[f];
    if (comp.type == T_INT && df == DFN_STR) /* STR$ of an INT: through a DOUBLE, every digit */
        comp_todouble();
    else if (expr.dbltgt && !(comp.type & (T_STR | T_DBL)) && df < DFN_STR)
        comp_todouble(); /* for a DOUBLE: of an INT or a SINGLE a DOUBLE's, as BC */
    if (comp.type == T_DBL && df != DFN_NONE) {
        if (df == DFN_STR) {
            comp_call(K_DSTR);
            comp.type = T_STR;
            return;
        }
        comp_opb(O_D_FN, df);
        comp.type = df == DF_SGN ? T_INT : T_DBL;
        comp.iload = comp.iloadend = nullptr;
        return;
    }
    farg(f);
    if (f == TK_VAL - TK_SGN) { /* VAL: a DOUBLE, as BC's */
        comp.type = T_DBL;
        comp_call(K_DVAL);
    } else
        comp_call(K_FN + f);
}

/* VARPTR, VARSEG, SSEG, SADD (v) after FFh: its CALL in k, 0 none; false: not one. */
static bool addr_call(const uint8_t tok, uint8_t *const k)
{
    switch (tok) {
    case TK1_VARPTR: *k = 0; return true;
    case TK1_VARSEG:
    case TK1_SSEG: *k = K_VSEG; return true;
    case TK1_SADD: *k = K_SADD; return true;
    default: return false;
    }
}

/* cm_un: an operand: a variable, an element, a literal, ( expr ), - NOT +, a function. */
static void unary(void)
{
    expr.shend = nullptr; /* not a SHORT's unless it says */
    for (;;) {
        const uint8_t c = comp_cur();
        switch (c) {
        case M_VAR:
            expr_load();
            return;
        case M_INT: {
            const uint32_t v = load32(comp.p + 1);
            comp.p += INT_TOKEN_SIZE;
            comp_int(v);
            if (v <= INT16_MAX) {
                expr.sh = SH_LIT;
                expr.shend = comp.out;
            }
            return;
        }
        case '(':
            expr_paren();
            return;
        case TK_MINUS:
        case TK_NOT: {
            comp.p++;
            prec(c == TK_MINUS ? P_NEG : P_NOT);
            const bool k = expr.kend == comp.out; /* a constant's, so the result */
            if (c == TK_NOT) {
                comp_toint();
                comp_op(O_I_NOT);
            } else if (comp.type & T_STR)
                mismatch();
            else if (comp.type == T_DBL)
                comp_op(O_D_NEG);
            else if (comp.type == T_FLT)
                comp_op(O_F_NEG);
            else {
                expr_shclass(); /* a SHORT's negated is one */
                comp_op(O_I_NEG); /* I.LD.I16 is unsigned: no -literal fold */
                expr.sh = expr.shclass;
                expr.shend = comp.out;
            }
            if (k)
                expr.kend = comp.out;
            return;
        }
        case TK_PLUS:
            comp.p++;
            continue;
        case '"':
            comp.p++;
            expr_string();
            return;
        }
        if (c >= '.' && c <= '9') {
            uint8_t kind;
            const double d = dubl_in(&comp.p, &kind);
            if (kind == DK_DBL) { /* a DOUBLE: D.LD.K q */
                comp.type = T_DBL;
                comp_op(O_D_LD_K);
                const uint64_t q = (Cell){.d = d}.raw;
                comp_dword((uint32_t)q);
                comp_dword((uint32_t)(q >> 32));
                comp.iload = comp.iloadend = nullptr;
            } else {
                comp.type = T_FLT;
                comp_op(O_F_LD_F32);
                comp_dword(sngl_from_dbl(d));
            }
            expr.kend = comp.out;
            return;
        }
        if (c == TK_LEN && lensize())
            return;
        uint8_t k;
        if (c == TK_X && addr_call(comp.p[1], &k)) {
            comp.p += 2;
            comp_open();
            varaddr();
            comp_close();
            if (k)
                comp_call(k);
            comp.type = T_INT;
            comp.iload = comp.iloadend = nullptr;
            return;
        }
        const uint8_t f = c - TK_SGN;
        if (f > TK_MID - TK_SGN)
            xfunc();
        else
            func(f);
        return;
    }
}
