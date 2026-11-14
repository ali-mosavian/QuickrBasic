/* The editor's screen (src/ui/draw.asm). */
#include "ui/draw.h"
#include "session.h"
#include "plat/pkey.h"
#include "ui/debugpane.h"
#include "ui/edit.h"
#include "ui/menu.h"
#include "ui/screen.h"
#include "ui/highlight.h"
#include "ui/text.h"
#include "ui/debugui.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "ui/window.h"
#include "plat/pscr.h"

/* The 0-ended s to d, its 0 too. */
static void copy_str(char *const d, const char *const s) { memcpy(d, s, strlen(s) + 1); }

enum : uint8_t {
    STATUS_BAR_COL = 62, /* the bar before ^Q, the locks, the position */
    STATUS_PREFIX_COL = 64,
    STATUS_CAPS_COL = 67,
    STATUS_NUM_COL = 68,
    STATUS_POS_COL = 70,
};

static const char u_imm[] = "Immediate";
static const char u_f1[] = "F1=Help \xB3 ", u_f1s[] = "F1=Help   ";

static bool inactive; /* dr_ina: drawing an edit window without the focus */
static uint8_t sel;   /* dr_sk: edit_sel's for the window drawn */
static char ttl[128]; /* ui_ttl: the window's title */

void draw_all(Session *const S)
{
    menu_bar();
    wind_draw(S);
    dpan_tip(S);
    draw_status();
    /* the cursor in the window that has it, its shape the mode's */
    const uint16_t shape = uimn.ed.insert ? CURSOR_INSERT : CURSOR_OVERWRITE;
    scrn_shape(uimn.focus == FOC_PANE ? PSCR_HIDDEN : shape); /* the pane: its row lit, no cursor */
    if (uimn.focus == FOC_EDIT) {
        edit_ensure();
        scrn_cursor((uint8_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0),
                    (uint8_t)(uimn.edline.col - uimn.ed.left + 1 + uimn.ed.x));
    } else if (uimn.focus == FOC_IMM) {
        const uint16_t col = uimn.immline.col < ED_COLS - 1 ? uimn.immline.col : ED_COLS - 1;
        scrn_cursor((uint8_t)(uimn.imm.cur - uimn.imm.top + uimn.imm.row0), (uint8_t)(col + 1));
    }
}

/* The edit window drawn has the focus. */
static bool has_focus(void) { return uimn.focus == FOC_EDIT && !inactive; }

void draw_title(const uint8_t row, const uint8_t col, const uint8_t width, const uint8_t left,
                const uint8_t right, const char *const title, const bool lit, const bool arrow)
{
    uint8_t c = scrn_fill(row, col, SL_ED, left, 1);
    c = scrn_fill(row, c, SL_ED, GLYPH_H, (uint16_t)(width - 2));
    c = scrn_fill(row, c, SL_ED, right, 1);
    if (arrow) {
        c -= 5;
        c = scrn_fill(row, c, SL_ED, GLYPH_V_LEFT, 1);
        c = scrn_fill(row, c, SL_TTL, GLYPH_UP, 1);
        scrn_fill(row, c, SL_ED, GLYPH_V_RIGHT, 1);
    }
    const uint8_t slot = lit ? SL_TTL : SL_ED; /* at 1 + (the width - its length - 5) / 2 */
    c = (uint8_t)((uint8_t)(width - 5 - scrn_slen(title)) >> 1) + 1 + col;
    c = scrn_fill(row, c, slot, ' ', 1);
    c = scrn_str(row, c, title, slot, slot);
    scrn_fill(row, c, slot, ' ', 1);
}

/* The selection on screen row row, the edit window's line line, its text from column col. */
static void sel_row(const uint8_t row, const uint8_t col, const uint32_t line)
{
    const UiEdit *const e = &uimn.ed;
    if (sel == SEL_LINES) {
        if (line >= e->sel_a && line <= e->sel_b)
            scrn_attr(row, col, e->width, SL_SEL);
        return;
    }
    if (sel != SEL_PIECE || line != e->line)
        return;
    const uint16_t a0 = (uint16_t)e->sel_a, b0 = (uint16_t)e->sel_b; /* clipped to the window */
    if (b0 <= e->left)
        return;
    const uint16_t a = a0 > e->left ? a0 - e->left : 0;
    const uint16_t b1 = b0 - e->left;
    const uint16_t b = b1 < e->width ? b1 : e->width;
    if (a < b)
        scrn_attr(row, col + a, b - a, SL_SEL);
}

/* Text row r of the edit window. */
static void draw_row(Session *const S, const uint16_t r)
{
    const UiEdit *const e = &uimn.ed;
    const uint8_t row = (uint8_t)(r + e->row0), col = e->x + 1;
    const uint32_t line = r + e->top;
    if (line >= uimn.text.n) {
        scrn_fill(row, col, SL_ED, ' ', e->width);
        return;
    }
    const uint8_t *src = uimn.edline.text; /* uimn.edline is the active window's */
    uint16_t len = uimn.edline.len;
    if (inactive || line != e->line) {
        len = (uint16_t)text_get(S, line, uimn.line);
        src = uimn.line;
    }
    const uint16_t from = len > e->left ? len - e->left : 0; /* the part from left */
    const uint16_t n = from < e->width ? from : e->width;
    const uint8_t c = scrn_strn(row, col, src + e->left, n, SL_ED);
    scrn_fill(row, c, SL_ED, ' ', e->width - n);
    synt_row(S, row, col, src, len);
    udbg_row(S, row, col, line);
    sel_row(row, col, line);
}

/* The scroll bar's column, the frame's last. */
static uint8_t bar_col(void) { return (uint8_t)(uimn.ed.x + uimn.ed.width + 1); }

/* The scroll bar: arrows, the track, the thumb at the cursor's line. */
static void draw_vbar(void)
{
    const UiEdit *const e = &uimn.ed;
    if (e->rows < 3)
        return;
    const uint8_t row = (uint8_t)e->row0, col = bar_col();
    scrn_fill(row, col, SL_BAR, GLYPH_UP, 1);
    for (uint16_t i = 1; i < e->rows - 1; i++)
        scrn_fill((uint8_t)(row + i), col, SL_BAR, GLYPH_SHADE, 1);
    scrn_fill((uint8_t)(row + e->rows - 1), col, SL_BAR, GLYPH_DOWN, 1);
    const uint32_t thumb = e->line * (uint32_t)(e->rows - 3) / uimn.text.n;
    scrn_fill((uint8_t)(thumb + e->row0 + 1), col, SL_THB, ' ', 1);
}

/* The sideways bar under the text, its thumb at left. */
static void draw_hbar(void)
{
    enum { SPAN_MIN = 255 };
    const UiEdit *const e = &uimn.ed;
    const uint8_t row = (uint8_t)(e->row0 + e->rows);
    uint8_t c = scrn_fill(row, e->x, SL_ED, GLYPH_V, 1);
    c = scrn_fill(row, c, SL_BAR, GLYPH_LEFT, 1);
    c = scrn_fill(row, c, SL_BAR, GLYPH_SHADE, e->width - 2);
    c = scrn_fill(row, c, SL_BAR, GLYPH_RIGHT, 1);
    scrn_fill(row, c, SL_ED, GLYPH_V, 1);
    const uint32_t end = (uint16_t)(e->left + e->width); /* the columns to the window's end, */
    const uint32_t span = end < SPAN_MIN ? SPAN_MIN : end; /* SPAN_MIN at least */
    const uint32_t thumb = (uint32_t)e->left * (uint16_t)(e->width - 3) / span;
    scrn_fill(row, (uint8_t)((uint8_t)thumb + 2 + e->x), SL_THB, ' ', 1);
}

void draw_edit(Session *const S, const bool ina)
{
    inactive = ina;
    const UiEdit *const e = &uimn.ed;
    const uint8_t row = (uint8_t)(e->row0 - 1), width = (uint8_t)(e->width + 2);
    scrn_box(row, e->x, (uint8_t)(e->rows + 2), width, SL_ED, nullptr);
    copy_str(ttl, uimn.file[0] ? uimn.file : uimn_untitled); /* the file's name or Untitled */
    view_title(ttl);
    const bool top = row == 1; /* the top window's corners, else joins */
    draw_title(row, e->x, width, top ? GLYPH_TOP_LEFT : GLYPH_V_RIGHT,
               top ? GLYPH_TOP_RIGHT : GLYPH_V_LEFT, ttl, has_focus(), true);
    sel = inactive ? SEL_NONE : edit_sel();
    for (uint16_t r = 0; r < e->rows; r++)
        draw_row(S, r);
    if (has_focus()) { /* the bars: the window with the focus */
        draw_vbar();
        draw_hbar();
    } else
        scrn_fill((uint8_t)(e->row0 + e->rows), e->x + 1, SL_ED, ' ', e->width);
    inactive = false;
}

/* Line i of the immediate window's history, *len long; nullptr none. */
static const uint8_t *imm_line(const uint16_t i, uint16_t *const len)
{
    const UiImm *const im = &uimn.imm;
    if (i >= im->n) {
        *len = 0;
        return nullptr;
    }
    if (i == im->cur) {
        *len = uimn.immline.len;
        return uimn.immline.text;
    }
    *len = im->hist[i][0];
    return im->hist[i] + 1;
}

void draw_imm(void)
{
    UiImm *const im = &uimn.imm;
    draw_title((uint8_t)(im->row0 - 1), 0, PSCR_COLS, GLYPH_V_RIGHT, GLYPH_V_LEFT, u_imm,
               uimn.focus == FOC_IMM, false);
    if (im->cur < im->top) /* the window shows top and the next */
        im->top = im->cur;
    if (im->cur > im->top + 1)
        im->top = im->cur - 1;
    uint16_t r = 0;
    do {
        const uint8_t row = (uint8_t)(r + im->row0);
        scrn_fill(row, 0, SL_ED, GLYPH_V, 1);
        uint16_t all;
        const uint8_t *const src = imm_line(im->top + r, &all);
        const uint16_t len = all < ED_COLS ? all : ED_COLS;
        uint8_t c = scrn_strn(row, 1, src, len, SL_ED);
        c = scrn_fill(row, c, SL_ED, ' ', ED_COLS - len);
        scrn_fill(row, c, SL_ED, GLYPH_V, 1);
    } while (++r < im->rows);
}

const StatusKeys *draw_keys(void)
{
    if (uimn.focus == FOC_IMM)
        return &uimn_keys_imm;
    return uimn_stopped() ? &uimn_keys_run : &uimn_keys_edit;
}

/* v in n decimal digits at dst, zeros before; past them. */
static char *decimal(char *const dst, uint32_t v, const int n)
{
    for (int i = n - 1; i >= 0; i--, v /= 10)
        dst[i] = (char)('0' + v % 10);
    return dst + n;
}

/* The status bar's left: the keys, else F1 and the help line; false: that ran past its bar. */
static bool status_left(void)
{
    const char *const help = uimn.help;
    if (!help) {
        scrn_str(STATUS_ROW, 1, draw_keys()->text, SL_STAT, SL_STAT);
        return true;
    }
    const char *const f1 = help == uimn_menu_help || help == uimn_dlog_help ? u_f1s : u_f1;
    const uint8_t c = scrn_str(STATUS_ROW, 1, f1, SL_STAT, SL_STAT); /* F1=Help and a bar */
    return scrn_str(STATUS_ROW, c, help, SL_STAT, SL_STAT) <= STATUS_BAR_COL;
}

/* ^Q pending, Caps Lock, Num Lock. */
static void status_flags(void)
{
    if (uimn.ed.prefix) {
        scrn_fill(STATUS_ROW, STATUS_PREFIX_COL, SL_STAT, '^', 1);
        scrn_fill(STATUS_ROW, STATUS_PREFIX_COL + 1, SL_STAT, 'Q', 1);
    } else
        scrn_fill(STATUS_ROW, STATUS_PREFIX_COL, SL_STAT, ' ', 2);
    scrn_fill(STATUS_ROW, STATUS_CAPS_COL, SL_STAT, uimn.keyflags & PKEY_CAPS ? 'C' : ' ', 1);
    scrn_fill(STATUS_ROW, STATUS_NUM_COL, SL_STAT, uimn.keyflags & PKEY_NUM ? 'N' : ' ', 1);
}

/* The cursor's line:column. */
static void status_pos(void)
{
    char pos[12];
    char *const p = decimal(pos, uimn.ed.line + 1, 5);
    *p = ':';
    const uint32_t col = uimn.edline.col + 1u;
    const bool wide = (uint16_t)col >= 1000; /* past 999: 5 digits, 2 columns left */
    *decimal(p + 1, col, wide ? 5 : 3) = 0;
    scrn_str(STATUS_ROW, wide ? STATUS_POS_COL - 2 : STATUS_POS_COL, pos, SL_STAT, SL_STAT);
}

void draw_status(void)
{
    scrn_fill(STATUS_ROW, 0, SL_STAT, ' ', PSCR_COLS);
    if (!status_left())
        return;
    scrn_fill(STATUS_ROW, STATUS_BAR_COL, SL_STAT, GLYPH_V, 1);
    status_flags();
    status_pos();
}
