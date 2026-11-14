/* The editor's colours: themes of attributes and palettes (src/ui/theme.asm). A theme is NSLOT
   attributes, one per slot (SL_*), and the 16 text colours' RGB for the DAC (pscr_palette): a
   theme can change what a colour looks like. The editor's page shows the theme's colours, the
   program's page VGA's, theme 0's. */
#ifndef UI_THEME_H
#define UI_THEME_H
#include "ui/ui.h"

enum {
    NTHEME = 9,
    TH_SZ = NSLOT + 48, /* a theme: its attributes, then 16 colours' R G B */
};

typedef struct {
    uint8_t cur;          /* g_thn: the theme shown, 0 QBasic's */
    uint8_t attr[NSLOT];  /* g_tha: its attributes, by slot */
} Them;
extern Them them;

/* th_set: theme n, shown (its attributes, its colours). */
void them_set(uint8_t n);
/* th_qb: Options, Theme, QBasic Colors (theme 0). */
void them_qbasic(void);
/* th_dark: One Dark (theme 1). */
void them_dark(void);
/* th_pick: the menu's theme action (ACT_TH2 .. : themes 2 ..). */
void them_pick(uint8_t action);
/* th_in: the theme's colours: the editor's page shown. */
void them_in(void);
/* th_out: VGA's colours: the program's page shown. */
void them_out(void);
/* th_ah: slot's attribute in the theme. */
static inline uint8_t them_attr(uint8_t slot) { return them.attr[slot]; }
#endif
