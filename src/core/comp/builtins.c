/* QBasic's built-in functions: the tables the parser checks and the code generator reads.
 *
 * # How it works
 * Functions SGN .. CHR$ are indexed from TK_SGN: fns_sig gives each one's argument conversion
 * (FA_*) and result (FR_*), fns_dfnk its DOUBLE form (a DF_* or DFN_*). The other functions are
 * fns_x, one XFn each: token, CALL, argument counts, result and each argument's kind. fns_type
 * gives a result's type.
 *
 * # asm structure (retired/asm32/comp/expr.asm)
 * | asm | here | |
 * |---|---|---|
 * | g_dfnk | fns_dfnk | MIRROR |
 * | g_fnarg, g_fnsig | fns_sig | D1 |
 * | g_xfn, XF_* | fns_x, XFn, XF_* | MIRROR |
 *
 * - D1. A function's argument and result are one byte: FA_* + FR_*.
 */
#include "comp/builtins.h"
#include "vm/syscall.h"
#include "comp/dfa.h"
#include "num/double.h"

#define FN(tk) [TK_##tk - TK_SGN]
const uint8_t fns_dfnk[20] = {
    FN(SGN) = DF_SGN,   FN(INT) = DF_INT,   FN(ABS) = DF_ABS,    FN(USR) = DFN_NONE,
    FN(FRE) = DFN_NONE, FN(POS) = DFN_NONE, FN(SQR) = DF_SQR,    FN(RND) = DFN_NONE,
    FN(LOG) = DF_LOG,   FN(EXP) = DF_EXP,   FN(COS) = DF_COS,    FN(SIN) = DF_SIN,
    FN(TAN) = DF_TAN,   FN(ATN) = DF_ATN,   FN(PEEK) = DFN_NONE, FN(LEN) = DFN_NONE,
    FN(STRS) = DFN_STR, FN(VAL) = DFN_NONE, FN(ASC) = DFN_NONE,  FN(CHRS) = DFN_NONE,
};

const uint8_t fns_sig[20] = {
    FN(SGN) = FA_F + FR_I,  FN(INT) = FA_F + FR_F, FN(ABS) = FA_F + FR_F,  FN(USR) = FA_F + FR_F,
    FN(FRE) = FA_Y + FR_F,  FN(POS) = FA_N + FR_I, FN(SQR) = FA_F + FR_F,  FN(RND) = FA_F + FR_F,
    FN(LOG) = FA_F + FR_F,  FN(EXP) = FA_F + FR_F, FN(COS) = FA_F + FR_F,  FN(SIN) = FA_F + FR_F,
    FN(TAN) = FA_F + FR_F,  FN(ATN) = FA_F + FR_F, FN(PEEK) = FA_I + FR_I, FN(LEN) = FA_S + FR_I,
    FN(STRS) = FA_F + FR_S, FN(VAL) = FA_S + FR_F, FN(ASC) = FA_S + FR_I,  FN(CHRS) = FA_B + FR_S,
};
#undef FN

const XFn fns_x[] = {
    {0x100 + TK1_CINT, 0, 1, 1, T_INT, "I", SC_NONE},
    {0x100 + TK1_CLNG, 0, 1, 1, T_INT, "I", SC_NONE},
    {0x100 + TK1_CSNG, 0, 1, 1, T_FLT, "F", SC_NONE},
    {0x100 + TK1_CDBL, 0, 1, 1, T_DBL, "D", SC_NONE},
    {TK_FIX, 0, 1, 1, T_FLT, "G", SC_NONE},
    {TK_INSTR, K_INSTR, XF_LEAD | 2, 3, T_INT, {'I', 'S', 'S'}, SC_NONE},
    {TK_LBOUND, K_LBND, 1, 2, T_INT, "RI", SC_NONE},
    {TK_UBOUND, K_UBND, 1, 2, T_INT, "RI", SC_NONE},
    {0x100 + TK1_SPACES, K_SPACE, 1, 1, T_STR, "I", SC_NONE},
    {0x100 + TK1_STRINGS, K_STRG, 2, 2, T_STR, "IX", SC_NONE},
    {0x100 + TK1_LTRIMS, K_LTRIM, 1, 1, T_STR, "S", SC_NONE},
    {0x100 + TK1_RTRIMS, K_RTRIM, 1, 1, T_STR, "S", SC_NONE},
    {0x100 + TK1_UCASES, K_UCASE, 1, 1, T_STR, "S", SC_NONE},
    {0x100 + TK1_LCASES, K_LCASE, 1, 1, T_STR, "S", SC_NONE},
    {0x100 + TK1_HEXS, K_HEX, 1, 1, T_STR, "W", SC_NONE},
    {0x100 + TK1_OCTS, K_OCT, 1, 1, T_STR, "W", SC_NONE},
    {0x100 + TK1_ERR, K_ERR, 0, 0, T_INT, "", SC_NONE},
    {0x100 + TK1_ERL, K_ERL, 0, 0, T_INT, "", SC_NONE},
    {0x100 + TK1_TIMER, K_TIMER, 0, 0, T_FLT, "", SC_NONE},
    {0x100 + TK1_DATES, K_DATE, 0, 0, T_STR, "", SC_NONE},
    {0x100 + TK1_TIMES, K_TIME, 0, 0, T_STR, "", SC_NONE},
    {TK_INKEYS, K_INKEY, 0, 0, T_STR, "", SC_NONE},
    {0x100 + TK1_COMMANDS, K_CMD, 0, 0, T_STR, "", SC_NONE},
    {0x100 + TK1_ENVIRONS, K_ENVS, 1, 1, T_STR, "X", SC_NONE},
    {0x100 + TK1_SETMEM, K_SETMEM, 1, 1, T_INT, "I", SC_NONE},
    {0x100 + TK1_ERDEV, K_ERDEV, 0, 0, T_INT, "", SC_NONE},
    {0x100 + TK1_ERDEVS, K_ERDEVS, 0, 0, T_STR, "", SC_NONE},
    {0x100 + TK1_MKIS, K_MKI, 1, 1, T_STR, "I", SC_NONE},
    {0x100 + TK1_MKLS, K_MKL, 1, 1, T_STR, "I", SC_NONE},
    {0x100 + TK1_MKSS, K_MKS, 1, 1, T_STR, "F", SC_NONE},
    {0x100 + TK1_MKDS, K_MKD, 1, 1, T_STR, "D", SC_NONE},
    {0x100 + TK1_MKSMBFS, K_MKSM, 1, 1, T_STR, "F", SC_NONE},
    {0x100 + TK1_MKDMBFS, K_MKDM, 1, 1, T_STR, "D", SC_NONE},
    {0x100 + TK1_CVI, K_CVI, 1, 1, T_INT, "S", SC_NONE},
    {0x100 + TK1_CVL, K_CVL, 1, 1, T_INT, "S", SC_NONE},
    {0x100 + TK1_CVS, K_CVS, 1, 1, T_FLT, "S", SC_NONE},
    {0x100 + TK1_CVD, K_CVD, 1, 1, T_DBL, "S", SC_NONE},
    {0x100 + TK1_CVSMBF, K_CVSM, 1, 1, T_FLT, "S", SC_NONE},
    {0x100 + TK1_CVDMBF, K_CVDM, 1, 1, T_DBL, "S", SC_NONE},
    {0x100 + TK1_EOF, K_EOF, 1, 1, T_INT, "H", SC_NONE},
    {0x100 + TK1_LOF, K_LOF, 1, 1, T_INT, "H", SC_NONE},
    {0x100 + TK1_LOC, K_LOC, 1, 1, T_INT, "H", SC_NONE},
    {0x100 + TK1_SEEK, K_SEEKF, 1, 1, T_INT, "H", SC_NONE},
    {0x100 + TK1_FREEFILE, K_FREEF, 0, 0, T_INT, "", SC_NONE},
    {0x100 + TK1_INP, K_INP, 1, 1, T_INT, "I", SC_NONE},
    {0x100 + TK1_INPUTS, K_INPS, 1, 2, T_STR, "IH", SC_NONE},
    {0x100 + TK1_FILEATTR, K_FATTR, 2, 2, T_INT, "HI", SC_NONE},
    {0x100 + TK1_IOCTLS, K_IOCTLS, 1, 1, T_STR, "H", SC_NONE},
    {0x100 + TK1_LPOS, K_LPOS, 1, 1, T_INT, "I", SC_NONE},
    {0x100 + TK1_POINT, 0, 2, 2, T_INT, "FF", SC_POINT},
    {0x100 + TK1_PMAP, 0, 2, 2, T_FLT, "FI", SC_PMAP},
    {0x100 + TK1_CSRLIN, 0, 0, 0, T_INT, "", SC_CSRLIN},
    {0x100 + TK1_PLAY, 0, 1, 1, T_INT, "I", SC_PLAYN},
    {0x100 + TK1_STICK, 0, 1, 1, T_INT, "I", SC_STICK},
    {0x100 + TK1_STRIG, 0, 1, 1, T_INT, "I", SC_STRIG},
    {0x100 + TK1_PEN, 0, 1, 1, T_INT, "I", SC_PEN},
    {0x100 + TK1_SCREEN, 0, 2, 3, T_INT, {'I', 'I', 'I'}, SC_SCRN},
};

const uint32_t fns_nx = sizeof fns_x / sizeof fns_x[0];

bool fns_isx(const uint16_t tok)
{
    for (uint32_t i = 0; i < fns_nx; i++)
        if (fns_x[i].tok == tok)
            return true;

    return false;
}

/* SGN .. MID$'s result, its argument's type at: as func compiles it. */
uint8_t fns_type(const uint8_t f, const uint8_t at, const bool dbl)
{
    if (f >= TK_MID - TK_SGN - 2)
        return T_STR;

    const uint8_t df = fns_dfnk[f];
    const bool todbl =
        (at == T_INT && df == DFN_STR) || (dbl && !(at & (T_STR | T_DBL)) && df < DFN_STR);

    if ((at == T_DBL || todbl) && df != DFN_NONE)
        return df == DFN_STR ? T_STR : df == DF_SGN ? T_INT : T_DBL;
    if (f == TK_VAL - TK_SGN)
        return T_DBL;

    const uint8_t res = fns_sig[f] & FR_MASK;
    return res == FR_S ? T_STR : res;
}
