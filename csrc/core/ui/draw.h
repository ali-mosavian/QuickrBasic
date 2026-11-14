/* The editor's screen: the windows, their titles and scroll bars, the status bar, the cursor
   (src/ui/draw.asm). Row 0 the menu bar; the windows' rows (window.h); row 24 the status bar. */
#ifndef UI_DRAW_H
#define UI_DRAW_H
#include "ui/ui.h"

/* dr_all: the whole screen (menu_bar, wind_draw, dpan_tip, draw_status) and the cursor in the
   focused window, its shape the mode's (hidden in the pane). */
void draw_all(Session *S);
/* dr_edit: the edit window in uimn.ed: its frame and title, its lines (synt_row, udbg_row), the
   selection, its scroll bars when it has the focus. inactive (dr_ina): the window drawn is not
   the active one (wind_draw swapped its state in): no selection, no bars, uimn.edline not its. */
void draw_edit(Session *S, bool inactive);
/* dr_imm: the immediate window: its title, its lines; im_top kept so that im_i shows. */
void draw_imm(void);
/* dr_status: the status bar: the keys (draw_keys) or uimn.help after "F1=Help"; the cursor's
   line and column from column 62 when they fit, ^Q pending, Caps and Num Lock, as QBasic's. */
void draw_status(void);
/* dr_skeys: the status bar's keys for the state: the immediate window's, else Continue's (a stop
   to go on from: uimn_stopped), else the editor's. */
const StatusKeys *draw_keys(void);
/* dr_ttl: a window's title row: from col, width wide; left, right its corners; title centred,
   lit (SL_TTL) when lit; arrow: the maximise arrow before the right corner. ttlx, ttlw were the
   asm's globals for col, width. */
void draw_title(uint8_t row, uint8_t col, uint8_t width, uint8_t left, uint8_t right,
                const char *title, bool lit, bool arrow);
#endif
