/* The text screen: 80 by 25 cells on pages, the cursor, the video mode, the 16 text colours.
 *
 * # How it works
 * A cell is text memory's: the character in the low byte, the attribute in the high. The editor
 * draws page 0 from its own copy (ui/scrn) and leaves page 1 to the program's output, which DOS
 * writes there while it is the page shown.
 *
 * On DOS32 the cells are colour text memory (B800h + page * 1000h), the rest INT 10h, the DAC's
 * ports 3C8h/3C9h and the input status port 3DAh.
 */
#ifndef PLAT_PSCR_H
#define PLAT_PSCR_H
#include "base.h"

/* The screen's rows (pscr_rows) and columns (pscr_cols) at most; PSCR_COLS a VGA's. */
enum { PSCR_ROWS_MAX = 100, PSCR_COLS_MAX = 240, PSCR_COLS = 80 };

/* A cell as text memory holds it: the character in the low byte, the attribute in the high. */
typedef uint16_t PscrCell;

enum : uint16_t { PSCR_HIDDEN = 0x2000 }; /* pscr_shape's: no cursor */

/* 80-column colour text of rows rows (25, 43, 50; INT 10h mode 3, then the 8x8 font past 25):
   every page cleared, page 0 shown, the BIOS palette. The rows it has: fewer when the card has
   not as many (an EGA's 43 for 50). A display with a size of its own (a terminal's) has that
   instead, rows and columns. */
uint8_t pscr_text(uint8_t rows);
/* The text screen's rows and columns now. */
uint8_t pscr_rows(void);
uint8_t pscr_cols(void);
/* The display has rows by cols: true unless it has a size of its own, and it changed. */
bool pscr_fits(uint8_t rows, uint8_t cols);
/* The page shown (INT 10h AH=5). */
void pscr_page(uint8_t page);

/* n cells from cells to page's row, col on, row by row (no clipping at a row's end). */
void pscr_put(uint8_t page, uint8_t row, uint8_t col, const PscrCell *cells, uint32_t n);
/* n cells of page from row, col to cells. */
void pscr_get(uint8_t page, uint8_t row, uint8_t col, PscrCell *cells, uint32_t n);
/* n copies of cell on page from row, col (INT 10h AH=9 at the cursor's place). */
void pscr_fill(uint8_t page, uint8_t row, uint8_t col, PscrCell cell, uint32_t n);

/* Page's cursor to row, col (INT 10h AH=2). */
void pscr_cursor(uint8_t page, uint8_t row, uint8_t col);
/* Page's cursor's place (INT 10h AH=3). */
void pscr_where(uint8_t page, uint8_t *row, uint8_t *col);
/* The cursor's shape: start line << 8 | end line (INT 10h AH=1); PSCR_HIDDEN none. */
void pscr_shape(uint16_t lines);

/* The window rows top .. bottom, columns left .. right of the page shown scrolled up by lines,
   the rows uncovered blank in attr (INT 10h AH=6); lines 0 blanks the window. */
void pscr_scroll(
    uint8_t lines,
    uint8_t top,
    uint8_t left,
    uint8_t bottom,
    uint8_t right,
    uint8_t attr
);

/* The 16 text colours' RGB, 6 bits each, colour 0's red first: each to the DAC register the
   attribute controller maps that colour to (the BIOS's: 0 1 2 3 4 5 20 7 56 .. 63). */
void pscr_palette(const uint8_t rgb[48]);

/* Until the next vertical retrace starts (one in progress is waited past): a copy to the page
   shown then does not tear. */
void pscr_retrace(void);

/* The console's bell (INT 10h AH=0Eh, BEL). */
void pscr_beep(void);
#endif
