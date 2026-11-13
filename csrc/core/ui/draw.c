/* The editor's screen (src/ui/draw.asm). */
#include "ui/draw.h"
#include "ui/dpan.h"
#include "ui/edit.h"
#include "ui/menu.h"
#include "ui/scrn.h"
#include "ui/synt.h"
#include "ui/text.h"
#include "ui/udbg.h"
#include "ui/uimn.h"
#include "ui/view.h"
#include "ui/wind.h"
#include "plat/pscr.h"

/* The 0-ended s to d, its 0 too. */
static void copy_str(char *d, const char *s) { memcpy(d, s, strlen(s) + 1); }

static const char u_imm[] = "Immediate";
static const char u_f1[] = "F1=Help \xB3 ", u_f1s[] = "F1=Help   ";

static bool inactive; /* dr_ina: drawing an edit window without the focus */
static uint8_t sel;   /* dr_sk: edit_sel's for the window drawn */
static char ttl[128]; /* ui_ttl: the window's title */

void draw_all(void)
{
    menu_bar();
    wind_draw();
    dpan_tip();
    draw_status();
    /* the cursor in the window that has it, its shape the mode's */
    uint16_t shape = uimn.ed.insert ? 0x0607 : 0x0407;
    if (uimn.focus == FOC_PANE) /* the pane: its row lit, no cursor */
        shape = PSCR_HIDDEN;
    scrn_shape(shape);
    if (uimn.focus == FOC_EDIT) {
        edit_ensure();
        scrn_cursor((uint8_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0),
                    (uint8_t)(uimn.edline.col - uimn.ed.left + 1 + uimn.ed.x));
    } else if (uimn.focus == FOC_IMM) {
        uint16_t col = uimn.immline.col < ED_COLS - 1 ? uimn.immline.col : ED_COLS - 1;
        scrn_cursor((uint8_t)(uimn.imm.cur - uimn.imm.top + uimn.imm.row0), (uint8_t)(col + 1));
    }
}

/* The edit window drawn has the focus. */
static bool has_focus(void) { return uimn.focus == FOC_EDIT && !inactive; }

void draw_title(uint8_t row, uint8_t col, uint8_t width, uint8_t left, uint8_t right,
                const char *title, bool lit, bool arrow)
{
    uint8_t c = scrn_fill(row, col, SL_ED, left, 1);
    c = scrn_fill(row, c, SL_ED, 0xC4, (uint16_t)(width - 2));
    c = scrn_fill(row, c, SL_ED, right, 1);
    if (arrow) {
        c -= 5;
        c = scrn_fill(row, c, SL_ED, 0xB4, 1);
        c = scrn_fill(row, c, SL_TTL, 0x18, 1);
        scrn_fill(row, c, SL_ED, 0xC3, 1);
    }
    uint8_t slot = lit ? SL_TTL : SL_ED; /* at 1 + (the width - its length - 5) / 2 */
    c = (uint8_t)((uint8_t)(width - 5 - scrn_slen(title)) >> 1) + 1 + col;
    c = scrn_fill(row, c, slot, ' ', 1);
    c = scrn_str(row, c, title, slot, slot);
    scrn_fill(row, c, slot, ' ', 1);
}

/* Text row r of the edit window. */
static void draw_row(uint16_t r)
{
    const UiEdit *e = &uimn.ed;
    uint8_t row = (uint8_t)(r + e->row0), col = e->x + 1;
    uint32_t line = r + e->top;
    if (line >= uimn.text.n) {
        scrn_fill(row, col, SL_ED, ' ', e->width);
        return;
    }
    const uint8_t *src = uimn.edline.text; /* uimn.edline is the active window's */
    uint16_t len = uimn.edline.len;
    if (inactive || line != e->line) {
        len = (uint16_t)text_get(line, uimn.line);
        src = uimn.line;
    }
    uint16_t n = len > e->left ? len - e->left : 0; /* the part from left */
    if (n > e->width)
        n = e->width;
    uint8_t c = scrn_strn(row, col, src + e->left, n, SL_ED);
    scrn_fill(row, c, SL_ED, ' ', e->width - n);
    synt_row(row, col, src, len);
    udbg_row(row, col, line);
    if (sel == SEL_LINES) { /* the selection */
        if (line >= e->sel_a && line <= e->sel_b)
            scrn_attr(row, col, e->width, SL_SEL);
    } else if (sel == SEL_PIECE && line == e->line) {
        uint16_t a = (uint16_t)e->sel_a, b = (uint16_t)e->sel_b; /* clipped to the window */
        a = a > e->left ? a - e->left : 0;
        if (b <= e->left)
            return;
        b -= e->left;
        if (b > e->width)
            b = e->width;
        if (a < b)
            scrn_attr(row, col + a, b - a, SL_SEL);
    }
}

/* The scroll bar's column, the frame's last. */
static uint8_t bar_col(void) { return (uint8_t)(uimn.ed.x + uimn.ed.width + 1); }

/* The scroll bar: arrows, the track, the thumb at the cursor's line. */
static void draw_vbar(void)
{
    const UiEdit *e = &uimn.ed;
    if (e->rows < 3)
        return;
    uint8_t row = (uint8_t)e->row0, col = bar_col();
    scrn_fill(row, col, SL_BAR, 0x18, 1);
    for (uint16_t i = 0; i < e->rows - 2; i++)
        scrn_fill(++row, col, SL_BAR, 0xB0, 1);
    scrn_fill(++row, col, SL_BAR, 0x19, 1);
    uint32_t thumb = e->line * (uint32_t)(e->rows - 3) / uimn.text.n;
    scrn_fill((uint8_t)(thumb + e->row0 + 1), col, SL_THB, ' ', 1);
}

/* The sideways bar under the text, its thumb at left. */
static void draw_hbar(void)
{
    const UiEdit *e = &uimn.ed;
    uint8_t row = (uint8_t)(e->row0 + e->rows);
    uint8_t c = scrn_fill(row, e->x, SL_ED, 0xB3, 1);
    c = scrn_fill(row, c, SL_BAR, 0x1B, 1);
    c = scrn_fill(row, c, SL_BAR, 0xB0, e->width - 2);
    c = scrn_fill(row, c, SL_BAR, 0x1A, 1);
    scrn_fill(row, c, SL_ED, 0xB3, 1);
    uint32_t span = (uint16_t)(e->left + e->width); /* of the columns to the window's end, */
    if (span < 255)                                 /* 255 at least */
        span = 255;
    uint32_t thumb = (uint32_t)e->left * (uint16_t)(e->width - 3) / span;
    scrn_fill(row, (uint8_t)((uint8_t)thumb + 2 + e->x), SL_THB, ' ', 1);
}

void draw_edit(bool ina)
{
    inactive = ina;
    const UiEdit *e = &uimn.ed;
    uint8_t row = (uint8_t)(e->row0 - 1), width = (uint8_t)(e->width + 2);
    scrn_box(row, e->x, (uint8_t)(e->rows + 2), width, SL_ED, nullptr);
    copy_str(ttl, uimn.file[0] ? uimn.file : uimn_untitled); /* the file's name or Untitled */
    view_title(ttl);
    bool top = row == 1; /* the top window's corners, else joins */
    draw_title(row, e->x, width, top ? 0xDA : 0xC3, top ? 0xBF : 0xB4, ttl, has_focus(),
               true);
    sel = inactive ? SEL_NONE : edit_sel();
    for (uint16_t r = 0; r < e->rows; r++)
        draw_row(r);
    if (has_focus()) { /* the bars: the window with the focus */
        draw_vbar();
        draw_hbar();
    } else
        scrn_fill((uint8_t)(e->row0 + e->rows), e->x + 1, SL_ED, ' ', e->width);
    inactive = false;
}

void draw_imm(void)
{
    UiImm *im = &uimn.imm;
    draw_title((uint8_t)(im->row0 - 1), 0, 80, 0xC3, 0xB4, u_imm, uimn.focus == FOC_IMM,
               false);
    if (im->cur < im->top) /* the window shows top and the next */
        im->top = im->cur;
    if (im->cur > im->top + 1)
        im->top = im->cur - 1;
    uint16_t r = 0;
    do {
        uint8_t row = (uint8_t)(r + im->row0);
        scrn_fill(row, 0, SL_ED, 0xB3, 1);
        uint16_t i = im->top + r, len = 0;
        const uint8_t *src = nullptr;
        if (i < im->n) {
            src = uimn.immline.text;
            len = uimn.immline.len;
            if (i != im->cur) {
                src = im->hist[i] + 1;
                len = im->hist[i][0];
            }
        }
        if (len > ED_COLS)
            len = ED_COLS;
        uint8_t c = scrn_strn(row, 1, src, len, SL_ED);
        c = scrn_fill(row, c, SL_ED, ' ', ED_COLS - len);
        scrn_fill(row, c, SL_ED, 0xB3, 1);
    } while (++r < im->rows);
}

const StatusKeys *draw_keys(void)
{
    if (uimn.focus == FOC_IMM)
        return &uimn_keys_imm;
    return uimn_stopped() ? &uimn_keys_run : &uimn_keys_edit;
}

/* v in n decimal digits at dst, zeros before; past them. */
static char *decimal(char *dst, uint32_t v, int n)
{
    for (int i = n - 1; i >= 0; i--, v /= 10)
        dst[i] = (char)('0' + v % 10);
    return dst + n;
}

void draw_status(void)
{
    scrn_fill(24, 0, SL_STAT, ' ', 80);
    const char *help = uimn.help;
    if (!help)
        scrn_str(24, 1, draw_keys()->text, SL_STAT, SL_STAT);
    else {
        const char *f1 = help == uimn_menu_help || help == uimn_dlog_help ? u_f1s : u_f1;
        uint8_t c = scrn_str(24, 1, f1, SL_STAT, SL_STAT); /* F1=Help and a bar */
        if (scrn_str(24, c, help, SL_STAT, SL_STAT) > 62)
            return;
    }
    scrn_fill(24, 62, SL_STAT, 0xB3, 1);
    if (uimn.ed.prefix) { /* ^Q pending */
        scrn_fill(24, 64, SL_STAT, '^', 1);
        scrn_fill(24, 65, SL_STAT, 'Q', 1);
    } else
        scrn_fill(24, 64, SL_STAT, ' ', 2);
    scrn_fill(24, 67, SL_STAT, uimn.keyflags & 0x40 ? 'C' : ' ', 1); /* Caps Lock, Num Lock */
    scrn_fill(24, 68, SL_STAT, uimn.keyflags & 0x20 ? 'N' : ' ', 1);
    char pos[12];
    char *p = decimal(pos, uimn.ed.line + 1, 5);
    *p++ = ':';
    uint32_t col = uimn.edline.col + 1u;
    uint8_t at = 70;
    int digits = 3;
    if ((uint16_t)col >= 1000) { /* past 999: 5 digits, 2 columns left */
        digits = 5;
        at = 68;
    }
    *decimal(p, col, digits) = 0;
    scrn_str(24, at, pos, SL_STAT, SL_STAT);
}
