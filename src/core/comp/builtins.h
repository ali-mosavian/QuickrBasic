/* The built-in functions' tables's interface. How it works: builtins.c. */
#ifndef COMP_BUILTINS_H
#define COMP_BUILTINS_H
#include "defs.h"

/* SGN .. CHR$ of a DOUBLE: D.FN's operand, DFN_STR STR$'s, DFN_NONE none. */
enum : uint8_t { DFN_STR = 0x80, DFN_NONE = 0xFF };
/* SGN .. CHR$ (TK_SGN on): a DOUBLE's DF_* or DFN_*. */
extern const uint8_t fns_dfnk[20];

/* Per function, TK_SGN on: its argument's conversion (FA_*), its result (FR_*). */
enum { FA_F, FA_A, FA_S, FA_Y, FA_B, FA_N, FA_I, FA_MASK = 0x0F };
enum { FR_I = T_INT, FR_F = T_FLT, FR_S = 0xB0, FR_MASK = 0xF0 };
extern const uint8_t fns_sig[20];

/* QBasic's other functions: the token (100h + n for TK_X n), the CALL (0 none), the arguments at
   least (| XF_LEAD: a leading one optional), at most (0: no ( ); least 0: ( ) optional), the
   result, each argument's kind: I an INT, U a BYTE's, W a SHORT's 16 bits, F a FLOAT, D a DOUBLE, S a string,
   X a string or a number, G FIX's, H a file number, R an array; or the runtime's service sc
   (vm/syscalls.def) in the CALL's place. */
typedef struct {
    uint16_t tok;
    uint8_t k, min, max, res;
    char arg[3];
    uint8_t sc;
} XFn;
enum : uint8_t { XF_LEAD = 0x80, XF_MIN = 0x7F };
extern const XFn fns_x[];
extern const uint32_t fns_nx;

/* tok (100h + n for TK_X n) one of fns_x: a function. */
bool fns_isx(uint16_t tok);

/* SGN .. MID$'s result (f TK_SGN on), its argument's type at, for a DOUBLE target or not. */
uint8_t fns_type(uint8_t f, uint8_t at, bool dbl);
#endif
