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

/* An operator's index: + - * / ^ AND OR > = < (TK_PLUS on), then \ MOD XOR EQV IMP from 10. */
static const uint8_t prectab[15] = {0x78, 0x78, 0x7B, 0x7B, 0x7F, 0x50, 0x46, 0x64, 0x64, 0x64,
                                    0x7A, 0x79, 0x44, 0x42, 0x40};
static const uint8_t xop[5] = {O_I_DIV, O_I_MOD, O_I_XOR, O_I_EQV, O_I_IMP};
static const uint8_t relbit[3] = {DC_GT, DC_EQ, DC_LT}; /* > = < */
static const uint8_t fcmpk[4] = {O_F_CMP_V, O_F_CMP_VI, O_F_CMP_U8, O_F_CMP_F32};
static const uint8_t lda1[5] = {O_F_LD_A1, O_I_LD_A1, O_S_LD_A1, O_D_LD_A1, O_D_LD_A1};
static const uint8_t ldan[5] = {O_F_LD_A, O_I_LD_A, O_S_LD_A, O_D_LD_A, O_D_LD_A};
/* SGN .. CHR$ of a DOUBLE: D.FN's operand, 80h STR$'s, FFh none. */
static const uint8_t dfnk[20] = {1, 2, 0, 0xFF, 0xFF, 0xFF, 4, 0xFF, 9, 10,
                                 6, 5, 7, 8, 0xFF, 0xFF, 0x80, 0xFF, 0xFF, 0xFF};

/* Per function, TK_SGN on: its argument's conversion (FA_*), its result (FR_*). */
enum { FA_F, FA_A, FA_S, FA_Y, FA_B, FA_N, FA_I };
enum { FR_I = T_INT, FR_F = T_FLT, FR_S = 0xB0 };
static const uint8_t fnsig[20] = {
    FA_F + FR_I, FA_F + FR_F, FA_F + FR_F, FA_F + FR_F, FA_Y + FR_F, FA_N + FR_I, FA_F + FR_F,
    FA_F + FR_F, FA_F + FR_F, FA_F + FR_F, FA_F + FR_F, FA_F + FR_F, FA_F + FR_F, FA_F + FR_F,
    FA_I + FR_I, FA_S + FR_I, FA_F + FR_S, FA_S + FR_F, FA_S + FR_I, FA_B + FR_S,
};

/* QBasic's other functions: the token (100h + n for TK_X n), the CALL (0 none), the arguments at
   least (| 80h: a leading one optional), at most (0: no ( ); least 0: ( ) optional), the result,
   each argument's kind: I an INT, W a SHORT's 16 bits, F a FLOAT, D a DOUBLE, S a string, X a
   string or a number, G FIX's, H a file number, R an array. */
typedef struct {
    uint16_t tok;
    uint8_t k, min, max, res;
    char arg[3];
} XFn;
static const XFn xfns[] = {
    {0x100 + TK1_CINT, 0, 1, 1, T_INT, "I"},
    {0x100 + TK1_CLNG, 0, 1, 1, T_INT, "I"},
    {0x100 + TK1_CSNG, 0, 1, 1, T_FLT, "F"},
    {0x100 + TK1_CDBL, 0, 1, 1, T_DBL, "D"},
    {TK_FIX, 0, 1, 1, T_FLT, "G"},
    {TK_INSTR, K_INSTR, 0x82, 3, T_INT, {'I', 'S', 'S'}},
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
    bool dbl = expr.dbltgt; /* not an assignment's: its own */
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
static void shortid(uint16_t id)
{
    if (symb_var(id)->type == ET_SH) {
        expr.sh = SH_SHORT;
        expr.shend = comp.out;
    }
}

/* cm_sop: operator rel's result is a SHORT's when both operands are SHORTs or literals that fit,
   one a SHORT: + - * \ MOD, QBasic's INTEGER arithmetic. lsh: the left's class. */
static uint8_t shortop(uint8_t rel, uint8_t lsh)
{
    uint8_t rsh = expr_shclass();
    if (!lsh || !rsh || !((lsh | rsh) & 1))
        return 0;
    return rel <= 2 || rel == 10 || rel == 11;
}

/* Is the right operand at right one op, sized size? */
static bool isright(uint8_t op, uint32_t size, const uint8_t *right)
{
    return (uint32_t)(comp.out - right) == size && *right == op;
}

uint8_t *expr_lfloat(uint8_t *left, uint8_t *right)
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
static void intop(uint8_t o, uint8_t *right)
{
    comp.type = T_INT;
    if (isright(O_I_LD_V, 3, right))
        *right = O_I_ADD_V + o;
    else if (isright(O_I_LD_U8, 2, right))
        *right = O_I_ADD_U8 + o;
    else
        comp_op(O_I_ADD + o);
}

void expr_icmp(uint8_t rel, uint8_t *right)
{
    comp.type = T_INT;
    uint8_t m = rel & 7, op;
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
static uint8_t fltkind(const uint8_t *right)
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
static void fltop(uint8_t o, uint8_t *right)
{
    comp.type = T_FLT;
    uint8_t k = fltkind(right);
    if (k)
        *right = k + o;
    else
        comp_op(O_F_ADD + o);
}

void expr_fcmp(uint8_t rel, uint8_t *right)
{
    comp.type = T_INT;
    uint8_t m = rel & 7, k = fltkind(right);
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

/* cm_p: compile while the operators bind tighter than precedence p. */
static void prec(uint8_t p)
{
    if (++comp.depth > COMP_MAXDEPTH)
        repl_error(E_OM);
    unary();
    for (;;) {
        uint8_t c = comp_cur(), op;
        if (c == '\\')
            op = 10;
        else if (c >= TK_MOD && c <= TK_IMP)
            op = c - TK_MOD + 11;
        else if (c >= TK_PLUS && c <= TK_LT)
            op = c - TK_PLUS;
        else
            break;
        if (p >= prectab[op])
            break;
        uint8_t rel;
        if (op >= TK_GT - TK_PLUS && op < 10) { /* a relation: 80h + its mask */
            rel = 0x80;
            for (;;) {
                rel |= relbit[op - (TK_GT - TK_PLUS)];
                c = comp_curn();
                if (c < TK_GT || c > TK_LT)
                    break;
                op = c - TK_PLUS;
            }
        } else {
            rel = op;
            comp.p++;
        }
        if (!(rel & 0x80) && rel >= 5 && (rel < 7 || rel >= 10))
            comp_toint(); /* AND OR \ MOD XOR EQV IMP: INTs */
        bool leftk = comp.out == expr.kend; /* the left a constant */
        uint8_t ltype = comp.type;
        uint8_t *left = comp_intleaf();
        uint8_t lsh = expr_shclass();
        uint8_t *right = comp.out;
        prec(prectab[op]);
        uint8_t sh = shortop(rel, lsh);
        expr.bothk = rel != 4 && leftk && comp.out == expr.kend; /* both constants; ^ not
                                                                    folded */
        if (!(rel & 0x80) && rel >= 10) {
            comp_toint();
            comp_op(xop[rel - 10]);
        } else if (!(rel & 0x80) && rel >= 5 && rel < 7) {
            comp_toint();
            intop(rel - 1, right); /* AND 4, OR 5 */
        } else if (ltype == T_STR) {
            if (!(comp.type & T_STR))
                mismatch();
            if (!rel)
                comp_op(O_S_CAT);
            else if (!(rel & 0x80))
                mismatch();
            else {
                comp.cmp = comp.out;
                comp_opb(O_S_CMP, rel & 7);
                comp.cmpend = comp.out;
                comp.type = T_INT;
            }
        } else {
            if (comp.type & T_STR)
                mismatch();
            /* a DOUBLE either side, or for a DOUBLE target: + - * of a SINGLE, / of any, not of
               constants; ^ */
            bool dbl = ltype == T_DBL || comp.type == T_DBL ||
                       (expr.dbltgt && (rel == 4 || (!expr.bothk && (rel == 3 || ltype & T_FLT ||
                                                                     comp.type == T_FLT))));
            if (dbl) {
                if (ltype != T_DBL)
                    comp_insert(ltype == T_INT ? O_I_TOD : O_F_TOD, right);
                comp_todouble();
                comp.iload = comp.iloadend = nullptr;
                if (rel == 4)
                    comp_op(O_D_POW);
                else if (rel & 0x80) {
                    comp_opb(O_D_CMP, rel & 7);
                    comp.type = T_INT;
                } else
                    comp_op(O_D_ADD + rel);
            } else if (rel == 4) { /* ^: a SINGLE, INTs' too */
                if (!(ltype & T_FLT))
                    expr_lfloat(left, right);
                comp_tofloat();
                comp_call(K_FPOW);
                comp.type = T_FLT;
            } else if (!(ltype & T_FLT) && comp.type == T_INT && rel != 3) {
                if (rel & 0x80) /* INT / INT is a FLOAT, as QBasic's */
                    expr_icmp(rel, right);
                else
                    intop(rel, right);
            } else {
                if (!(ltype & T_FLT))
                    right = expr_lfloat(left, right);
                comp_tofloat();
                if (rel & 0x80)
                    expr_fcmp(rel, right);
                else
                    fltop(rel, right);
            }
        }
        /* an operator's done: a SHORT's result checked */
        if (sh && comp.type == T_INT)
            comp_op(O_I_CHK16);
        expr.sh = sh;
        expr.shend = comp.out;
        expr.kend = expr.bothk ? comp.out : nullptr; /* of constants: one */
        expr.bothk = false;
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
    const uint8_t *start = comp.p;
    const uint8_t *q = start;
    uint32_t len = strg_span(&q, '"', '"');
    comp_opb(O_S_LD_TXT, (uint8_t)len);
    comp_dword(addr_of(start));
    comp.p = strg_skipq(q);
}

/* A FUNCTION DECLAREd only: its arguments, Function not defined when run; its result's id. */
static uint16_t undeclared(void)
{
    uint16_t id = comp_var(1, 0);
    if (*comp.p == '(') {
        comp.p++;
        ctrl_dropargs(true);
        comp_close();
    }
    ctrl_undef(18);
    return id;
}

/* A variable's value, its id's: X.LD.V, or the subscripts and X.LD.A1 id, X.LD.A id n. */
static void loadvar(uint16_t id)
{
    uint8_t info = symb_var(id)->info;
    if (info & VI_ARR) {
        uint8_t n = expr_subs();
        uint8_t k = info & 7;
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
    uint8_t k = info & 0x43;
    if (k == 1) {
        comp.iload = comp.out;
        comp_opid(O_I_LD_V, id);
        comp.iloadend = comp.out;
        comp.type = T_INT;
        shortid(id);
    } else if (k > 1) {
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
        uint8_t len = comp.p[1];
        const Name *n = symb_name(load16(comp.p + len + 2));
        if (n->flags & (NM_FUNC | NM_FN)) {
            if (!n->proc) {
                loadvar(undeclared());
                return;
            }
            if (comp.p[len + 4] == '(' || n->proc != comp.proc) {
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
    comp_target(comp_name());
    if (comp.tgtinfo & VI_ARR) {
        uint16_t tgt = comp.tgt;
        uint8_t info = comp.tgtinfo, subs = comp.tgtsubs;
        uint8_t n = expr_subs();
        comp.tgt = tgt;
        comp.tgtinfo = info;
        comp.tgtsubs = subs;
        comp.tgtsubs = n;
    }
}

/* Variable id's place: expr.vakind, VA_* << 24 + its bytes; a string's Desc VA_DESC + its
   STRING * n. */
static void vakind(uint16_t id)
{
    const Var *v = symb_var(id);
    uint8_t info = v->info;
    uint32_t va = VA_RAW << 24 | 8;
    if (!(info & SK_DBL)) {
        va = VA_RAW << 24 | 4;
        switch (info & 3) {
        case 1: /* an INT: 2 bytes but a LONG's */
            if (v->type != ET_I4)
                va = VA_I2 << 24 | 4;
            break;
        case 2:
            va = scop_fix(v->name, info & VI_ARR) | VA_DESC << 24;
            break;
        }
    }
    expr.vakind = va;
}

/* cm_addr: a variable's address: a record's or a field's code, else its slot's, its Desc's. */
static void varaddr(void)
{
    if (comp_cur() != M_VAR)
        synerr();
    if (ctrl_recref()) { /* a field's kind, its bytes */
        uint8_t et = ctrl.ref.et;
        uint32_t va = et >= ET_REC || et == ET_SD ? ctrl.ref.fix : decl_etsize[et];
        va |= VA_RAW << 24;
        if (et == ET_I2)
            va |= VA_I2 << 24;
        expr.vakind = va;
        if (ctrl.ref.off) {
            comp_int(ctrl.ref.off);
            comp_op(O_I_ADD);
        }
        return;
    }
    uint16_t id = comp_var(0, 0);
    vakind(id);
    if (symb_var(id)->info & VI_ARR) { /* an element: RA.ADDR's */
        uint8_t n = expr_subs();
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
static void xarg(char k)
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

/* QBasic's other functions, by xfns: f(args), each converted to its kind; the count after them
   when it varies; then CALL k, or none. */
static void xfunc(void)
{
    uint16_t tok = *comp.p;
    if (tok == TK_X)
        tok = 0x100 + *++comp.p;
    comp.p++;
    const XFn *f = xfns;
    while (f < xfns + countof(xfns) && f->tok != tok)
        f++;
    if (f == xfns + countof(xfns))
        synerr();
    if (!f->max)
        goto call; /* no ( ) */
    if (!(f->min & 0x7F) && comp_cur() != '(') { /* all optional: ( ) too */
        comp_int(0);
        goto call;
    }
    comp_open();
    uint32_t n = 0, i = 0; /* the arguments, the kind's index */
    for (;;) {
        char k = f->arg[i];
        if (k == 'R') { /* an array, n or n(): its id */
            if (comp_cur() != M_VAR)
                synerr();
            uint16_t id = comp_var(2, 0);
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
            if (!n && f->min & 0x80 && comp.type == T_STR && k != 'S') /* a leading optional
                                                                          left out */
                k = f->arg[++i];
            xarg(k);
        }
        n++;
        i++;
        if (i >= f->max || !comp_optcomma())
            break;
    }
    if (i < (f->min & 0x7F))
        synerr();
    comp_close();
    if ((f->min & 0x7F) != f->max)
        comp_int(n);
call:
    if (f->k)
        comp_call(f->k);
    uint8_t t = f->res;
    if (expr.fixdbl) {
        expr.fixdbl = false;
        t = T_DBL;
    }
    comp.type = t;
    comp.iload = comp.iloadend = nullptr;
}

/* Function f's (TK_SGN on) argument converted; comp.type its result. */
static void farg(uint8_t f)
{
    uint8_t sig = fnsig[f];
    switch (sig & 0x0F) {
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
    uint8_t res = sig & 0xF0;
    comp.type = res == FR_S ? T_STR : res;
}

/* LEN(r): a record's, a field's, a number's size; false: LEN's own call, nothing compiled. */
static bool lensize(void)
{
    const uint8_t *p = comp.p;
    uint8_t *out = comp.out;
    comp.p++;
    if (comp_cur() == '(' && comp_curn() == M_VAR) {
        if (ctrl_recref()) {
            if (comp_cur() == ')') {
                comp.p++;
                uint8_t et = ctrl.ref.et;
                comp_int(et >= ET_REC || et == ET_SD ? ctrl.ref.fix : decl_etsize[et]);
                return true;
            }
        } else { /* a scalar: % 2, & ! 4, # 8 */
            uint16_t id = comp_var(0, 0);
            if (!(symb_var(id)->info & VI_ARR)) {
                vakind(id);
                uint32_t va = expr.vakind;
                if (comp_cur() == ')' && va >> 24 != VA_DESC) {
                    comp.p++;
                    comp_int(va >> 24 == VA_I2 ? 2 : (uint8_t)va);
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
static void func(uint8_t f)
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
    if (comp.type == T_INT && dfnk[f] == 0x80) /* STR$ of an INT: through a DOUBLE, every digit */
        comp_todouble();
    else if (expr.dbltgt && !(comp.type & (T_STR | T_DBL)) && dfnk[f] < 0x80)
        comp_todouble(); /* for a DOUBLE: of an INT or a SINGLE a DOUBLE's, as BC */
    if (comp.type == T_DBL && dfnk[f] != 0xFF) {
        if (dfnk[f] == 0x80) {
            comp_call(K_DSTR);
            comp.type = T_STR;
            return;
        }
        comp_opb(O_D_FN, dfnk[f]);
        comp.type = dfnk[f] == DF_SGN ? T_INT : T_DBL;
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

/* cm_un: an operand: a variable, an element, a literal, ( expr ), - NOT +, a function. */
static void unary(void)
{
    expr.shend = nullptr; /* not a SHORT's unless it says */
    for (;;) {
        uint8_t c = comp_cur();
        switch (c) {
        case M_VAR:
            expr_load();
            return;
        case M_INT: {
            uint32_t v = load32(comp.p + 1);
            comp.p += 5;
            comp_int(v);
            if (v <= 32767) {
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
            prec(c == TK_MINUS ? 0x7D : 0x5A);
            bool k = expr.kend == comp.out; /* a constant's, so the result */
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
            double d = dubl_in(&comp.p, &kind);
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
        if (c == TK_X) { /* VARPTR, VARSEG, SSEG, SADD (v) */
            uint8_t a = comp.p[1], k = 0xFF;
            if (a == TK1_VARPTR)
                k = 0;
            else if (a == TK1_VARSEG || a == TK1_SSEG)
                k = K_VSEG;
            else if (a == TK1_SADD)
                k = K_SADD;
            if (k != 0xFF) {
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
        }
        uint8_t f = c - TK_SGN;
        if (f > TK_MID - TK_SGN)
            xfunc();
        else
            func(f);
        return;
    }
}
