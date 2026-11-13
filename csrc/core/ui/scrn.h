/* The editor's screen: cells, strings, boxes, shadows (src/ui/scr.asm). Drawing goes to a copy of
   page 0 (the asm's g_scrb, static here); scrn_flip shows it. A place is row, col; slot an
   attribute slot (SL_*), made the theme's attribute (them_attr) as it is drawn. A string's '&'
   marks its hotkey, drawn in slot hot; a plain one has hot == slot. A run goes on into the next
   row past column 79. */
#ifndef UI_SCRN_H
#define UI_SCRN_H
#include "ui/ui.h"

/* sc_fill: n cells of ch in slot from row, col; the column past them. */
uint8_t scrn_fill(uint8_t row, uint8_t col, uint8_t slot, uint8_t ch, uint16_t n);
/* sc_str: the 0-ended s in slot, its '&' hotkey in hot; the column past it. */
uint8_t scrn_str(uint8_t row, uint8_t col, const char *s, uint8_t slot, uint8_t hot);
/* sc_strn: n characters of s in slot, '&' not special; the column past them. */
uint8_t scrn_strn(uint8_t row, uint8_t col, const uint8_t *s, uint16_t n, uint8_t slot);
/* sc_slen: s's length, its '&' not counted. */
uint16_t scrn_slen(const char *s);
/* sc_box: a single-line frame of rows by cols at row, col, filled with spaces, in slot; title
   centred in it, nullptr none. */
void scrn_box(uint8_t row, uint8_t col, uint8_t rows, uint8_t cols, uint8_t slot,
              const char *title);
/* sc_sep: a separator ├──┤ cols wide at row, col, in slot. */
void scrn_sep(uint8_t row, uint8_t col, uint8_t cols, uint8_t slot);
/* sc_attr: n cells from row, col take slot's attribute; their characters stay. */
void scrn_attr(uint8_t row, uint8_t col, uint16_t n, uint8_t slot);
/* sc_shadow: the shadow (SL_SHD) of a box of rows by cols at row, col: two columns right, a row
   below, clipped to the screen. */
void scrn_shadow(uint8_t row, uint8_t col, uint8_t rows, uint8_t cols);
/* sc_cur: page 0's cursor at row, col. */
void scrn_cursor(uint8_t row, uint8_t col);
/* sc_shape: the cursor's start << 8 | end line; PSCR_HIDDEN (2000h) hides it. */
void scrn_shape(uint16_t lines);
/* sc_flip: the drawn screen to page 0, in the vertical retrace. */
void scrn_flip(void);
#endif
