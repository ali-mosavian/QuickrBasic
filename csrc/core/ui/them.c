/* The editor's colours (src/ui/theme.asm). */
#include "ui/them.h"
#include "ui/menu.h"
#include "plat/pscr.h"

Them them;

/* Zed's themes' attributes, over one layout of the colours: 0 bars, 1 the text's ground, 2 a
   shadow, 3 a selection, 4 errors, 5 keywords, 6 numbers, 7 text, 8 comments, 9 functions,
   10 operators, 11 types, 12 strings, 13 parameters, 14 warnings, 15 strong text. */
#define ZEDA                                                                                   \
    0x17, 0x37, 0x39, 0x07, 0x0F, 0x07, 0x3F, 0x39, 0x08, 0x4F, 0x3F, 0x07, 0x3F, 0x0F, 0x28,  \
        0x30, 0x15, 0x1C, 0x16, 0x18, 0x17, 0x19, 0x1D, 0x1B, 0x1A

/* ED SEL TTL BAR HOT STAT MSEL MHOT GRAY BPL CUR DLG DSEL DFOC SHD THB, KW STR NUM COM NAM FNM
   PAR TYP OP; then the 16 colours. */
static const uint8_t themes[NTHEME][TH_SZ] = {
    {0x17, 0x71, 0x71, 0x70, 0x7F, 0x30, 0x07, 0x0F, 0x78, 0x47, 0x1F, 0x70, 0x07, 0x7F, 0x08,
     0x00, 0x1B, 0x1E, 0x1D, 0x1A, 0x17, 0x1F, 0x1C, 0x13, 0x13, /* QBasic's, VGA's colours */
     0, 0, 0, 0, 0, 42, 0, 42, 0, 0, 42, 42, 42, 0, 0, 42, 0, 42, 42, 21, 0, 42, 42, 42,
     21, 21, 21, 21, 21, 63, 21, 63, 21, 21, 63, 63, 63, 21, 21, 63, 21, 63, 63, 63, 21, 63, 63,
     63},
    {ZEDA, /* One Dark */
     14, 16, 19, 10, 11, 12, 5, 5, 6, 17, 18, 21, 52, 28, 29, 45, 29, 51,
     47, 37, 26, 43, 44, 47, 23, 24, 27, 28, 43, 58, 27, 45, 47, 27, 45, 47,
     40, 48, 32, 52, 28, 29, 55, 48, 33, 55, 56, 57},
    {ZEDA, /* One Light */
     55, 55, 55, 62, 62, 62, 27, 27, 27, 50, 50, 50, 52, 24, 20, 41, 18, 42,
     43, 27, 9, 9, 9, 10, 40, 40, 41, 22, 30, 56, 14, 32, 45, 14, 32, 45,
     25, 39, 21, 52, 24, 19, 41, 34, 6, 3, 3, 4},
    {ZEDA, /* Ayu Mirage */
     17, 18, 20, 9, 10, 13, 4, 5, 6, 20, 21, 23, 60, 33, 30, 63, 43, 25,
     55, 47, 63, 51, 50, 48, 23, 25, 28, 63, 52, 28, 60, 39, 29, 28, 51, 63,
     53, 63, 31, 55, 47, 63, 63, 51, 28, 58, 58, 57},
    {ZEDA, /* Ayu Light */
     55, 55, 55, 63, 63, 63, 27, 27, 27, 51, 52, 52, 59, 28, 28, 62, 35, 15,
     40, 30, 51, 23, 24, 25, 42, 44, 45, 60, 43, 18, 59, 36, 25, 14, 39, 57,
     33, 44, 0, 40, 30, 51, 60, 43, 18, 9, 9, 10},
    {ZEDA, /* Gruvbox Dark Soft */
     19, 17, 16, 12, 12, 11, 6, 6, 5, 22, 20, 19, 62, 18, 13, 62, 18, 12,
     52, 33, 38, 58, 54, 44, 42, 38, 33, 46, 46, 9, 35, 48, 31, 62, 47, 11,
     46, 46, 9, 58, 54, 44, 62, 47, 11, 61, 60, 56},
    {ZEDA, /* Gruvbox Light */
     54, 50, 41, 62, 60, 49, 27, 25, 20, 50, 46, 38, 39, 0, 2, 39, 0, 1,
     35, 15, 28, 10, 10, 10, 31, 27, 25, 30, 29, 3, 16, 30, 22, 45, 29, 4,
     30, 29, 3, 10, 10, 10, 45, 29, 5, 4, 4, 4},
    {ZEDA, /* JetBrains Dark */
     9, 10, 10, 7, 7, 8, 3, 3, 4, 16, 17, 18, 62, 25, 29, 51, 35, 27,
     10, 43, 46, 47, 47, 49, 30, 31, 33, 26, 40, 53, 47, 47, 49, 41, 46, 29,
     26, 42, 28, 47, 47, 49, 60, 49, 23, 57, 57, 57},
    {ZEDA, /* JetBrains Light */
     61, 62, 62, 63, 63, 63, 30, 31, 31, 55, 56, 57, 63, 0, 0, 0, 12, 44,
     5, 20, 58, 2, 2, 2, 35, 35, 35, 0, 24, 30, 2, 2, 2, 40, 18, 2,
     1, 31, 5, 2, 2, 2, 61, 38, 4, 0, 0, 0},
};

void them_set(uint8_t n)
{
    them.cur = n;
    memcpy(them.attr, themes[n], NSLOT);
    them_in();
}

void them_qbasic(void) { them_set(0); }
void them_dark(void) { them_set(1); }
void them_pick(uint8_t action) { them_set(action - (ACT_TH2 - 2)); }

void them_in(void) { pscr_palette(themes[them.cur] + NSLOT); }
void them_out(void) { pscr_palette(themes[0] + NSLOT); }
