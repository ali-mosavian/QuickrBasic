/* The editor's screen: the windows, their titles and scroll bars, the status bar and the cursor.
 *
 * # How it works
 * Row 0 is the menu bar, the windows' rows are window.h's, row 24 is the status bar. draw_all draws
 * the lot; draw_edit, draw_imm and draw_status each part.
 *
 * # asm structure (retired/asm32/ui/draw.asm)
 * | asm | here | |
 * |---|---|---|
 * | dr_all, dr_edit, dr_imm, dr_status | draw_all, draw_edit, draw_imm, draw_status | MIRROR; D1 |
 * | dr_skeys | draw_keys | MIRROR; D2 |
 * | dr_ttl | draw_title | MIRROR |
 * | g_ttlx, g_ttlw | draw_title's col, width | D2 |
 * | dr_hasfoc, dr_row, dr_vbar, dr_barx, dr_hbar, dr_cursor, ui_decn, ui_title | statics | MIRROR |
 * | g_drls, g_drln, lp.dr_sk, lp.dr_ln, lp.ui_pos, bf.ui_ttl | statics | MIRROR |
 * | u_imm, u_f1, u_f1s | statics | MOVED from uimain |
 *
 * - D1. dr_edit's flag (lp.dr_ina) is draw_edit's parameter.
 * - D2. draw_keys returns the StatusKeys; the title's column and width are draw_title's parameters,
 *   not globals.
 */
#include "ui/draw.h"
#include "session.h"
#include "plat/pkey.h"
#include "ui/complete.h"
#include "ui/debugpane.h"
#include "ui/edit.h"
#include "ui/menu.h"
#include "ui/screen.h"
#include "ui/help.h"
#include "ui/highlight.h"
#include "ui/text.h"
#include "ui/visible.h"
#include "ui/debugui.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "ui/window.h"
#include "plat/pscr.h"
#include "util/grow.h"

/* The 0-ended s to d, its 0 too. */

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
static char *ttl;     /* ui_ttl: the window's title, grown to fit */
static uint32_t ttl_cap;

void draw_all(Session *const S)
{
    tree_draw(&uimn.tree.screen, S);
    Widget *const f = tree_focused(&uimn.tree.screen);

    if (f->ops && f->ops->cursor)
        f->ops->cursor(f);
    else
        scrn_shape(PSCR_HIDDEN); /* the pane: its row lit; a menu */
}

/* The cursor's shape: the edit mode's. */
static void shape(void) { scrn_shape(uimn.ed.insert ? CURSOR_INSERT : CURSOR_OVERWRITE); }

void draw_edit_cursor(Widget *const w)
{
    shape();
    edit_ensure();
#if AC
    cmpl_draw_ghost();
#endif
    scrn_cursor(
        (uint8_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0),
        (uint8_t)(uimn.edline.col - uimn.ed.left + 1 + uimn.ed.x)
    );
}

void draw_imm_cursor(Widget *const w)
{
    shape();
    const uint16_t col = uimn.immline.col < ED_COLS - 1 ? uimn.immline.col : ED_COLS - 1;
    scrn_cursor((uint8_t)(uimn.imm.cur - uimn.imm.top + uimn.imm.row0), (uint8_t)(col + 1));
}

/* The edit window drawn has the focus. */
static bool has_focus(void) { return uimn.tree.workspace.focus == FOC_EDIT && !inactive; }

void draw_title_in(
    const uint8_t base,
    const uint8_t row,
    const uint8_t col,
    const uint8_t width,
    const uint8_t left,
    const uint8_t right,
    const char *const title,
    const bool lit,
    const uint8_t arrow,
    const uint8_t lit_slot
)
{
    uint8_t c = scrn_fill(row, col, base, left, 1);
    c = scrn_fill(row, c, base, GLYPH_H, (uint16_t)(width - 2));
    c = scrn_fill(row, c, base, right, 1);

    if (arrow) {
        c -= 5;
        c = scrn_fill(row, c, base, GLYPH_V_LEFT, 1);
        c = scrn_fill(row, c, SL_TTL, arrow, 1);
        scrn_fill(row, c, base, GLYPH_V_RIGHT, 1);
    }

    const uint8_t slot = lit ? lit_slot : base; /* at 1 + (the width - its length - 5) / 2 */
    const uint16_t len = scrn_slen(title), room = width > 7 ? width - 7 : 0;
    const uint16_t n = len < room ? len : room; /* a longer one cut, clear of the arrow */
    c = (uint8_t)((width - 5 - n) >> 1) + 1 + col;
    c = scrn_fill(row, c, slot, ' ', 1);
    c = n == len ? scrn_str(row, c, title, slot, slot)
                 : scrn_strn(row, c, (const uint8_t *)title, n, slot);
    scrn_fill(row, c, slot, ' ', 1);
}

void draw_title(
    const uint8_t row,
    const uint8_t col,
    const uint8_t width,
    const uint8_t left,
    const uint8_t right,
    const char *const title,
    const bool lit,
    const uint8_t arrow
)
{
    draw_title_in(SL_ED, row, col, width, left, right, title, lit, arrow, SL_TTL);
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

    VislLine v = {.text = uimn.edline.text, .len = uimn.edline.len}; /* the active window's */

    if (inactive || line != e->line)
        v = visl_get(S, line);

    const uint16_t len = (uint16_t)v.len;
    const uint16_t from = len > e->left ? len - e->left : 0; /* the part from left */
    const uint16_t n = from < e->width ? from : e->width;
    const uint8_t c = scrn_strn(row, col, v.text + e->left, n, SL_ED);
    scrn_fill(row, c, SL_ED, ' ', e->width - n);

    if (v.cols)
        synt_paint(row, col, v.cols, len);
    else
        synt_row(S, row, col, v.text, len);

    udbg_row(S, row, col, line);
    sel_row(row, col, line);
}

/* Edit window e's scroll bar: arrows, the track, the thumb at the cursor's line. */
static void draw_vbar(const UiEdit *const e)
{
    if (e->rows < 3)
        return;

    const uint8_t row = (uint8_t)e->row0, col = (uint8_t)(e->x + e->width + 1);
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
    const uint32_t end = (uint16_t)(e->left + e->width);   /* the columns to the window's end, */
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
    const char *const file = uimn.file[0] ? uimn.file : uimn_untitled;    /* its name or Untitled */
    const char *const proc = uimn.ed.view ? view_name(uimn.ed.view) : ""; /* ":" its procedure */
    const uint32_t flen = (uint32_t)strlen(file), plen = (uint32_t)strlen(proc);
    const bool room = GROW(ttl, ttl_cap, flen + 1 + plen + 1); /* none: the file's name alone */

    if (room) {
        memcpy(ttl, file, flen);
        ttl[flen] = ':';
        memcpy(ttl + flen + (plen ? 1 : 0), proc, plen + 1);
    }

    const bool top = row == 1; /* the top window's corners, else joins */
    draw_title(
        row,
        e->x,
        width,
        top ? GLYPH_TOP_LEFT : GLYPH_V_RIGHT,
        top ? GLYPH_TOP_RIGHT : GLYPH_V_LEFT,
        room ? ttl : file,
        has_focus(),
        uimn.pane_on     ? 0
        : uimn.win.maxed ? GLYPH_UPDOWN
                         : GLYPH_UP /* none by the pane */
    );
    sel = inactive ? SEL_NONE : edit_sel();

    for (uint16_t r = 0; r < e->rows; r++)
        draw_row(S, r);

    const uint8_t last = (uint8_t)(e->row0 + e->rows);

    if (uimn.pane_on) /* by the pane: no bars, the border on down; the pane's line its place */
        scrn_fill(last, scrn_fill(last, e->x, SL_ED, GLYPH_V, 1), SL_ED, ' ', e->width);
    else if (has_focus()) { /* the bars: the window with the focus */
        draw_vbar(e);
        draw_hbar();
    } else
        scrn_fill(last, e->x + 1, SL_ED, ' ', e->width);

    inactive = false;
}

void draw_seam(const UiEdit *const e, const bool bar)
{
    const uint8_t col = (uint8_t)(e->x + e->width + 1);
    scrn_fill((uint8_t)(e->row0 - 1), col, SL_ED, GLYPH_T_DOWN, 1);

    for (uint16_t r = 0; r <= e->rows; r++) /* its rows and the bottom one */
        scrn_fill((uint8_t)(e->row0 + r), col, SL_ED, GLYPH_V, 1);
    if (bar)
        draw_vbar(e);
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
    draw_title(
        (uint8_t)(im->row0 - 1),
        0,
        PSCR_COLS,
        GLYPH_V_RIGHT,
        GLYPH_V_LEFT,
        u_imm,
        uimn.tree.workspace.focus == FOC_IMM,
        0
    );

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
    if (uimn.tree.workspace.focus == FOC_IMM)
        return &uimn_keys_imm;
    if (uimn.tree.workspace.focus == FOC_HELP)
        return &help_keys;

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
        scrn_str(status_row(), 1, draw_keys()->text, SL_SMSG, SL_SMSG);
        return true;
    }

    const char *const f1 = help == uimn_menu_help || help == uimn_dlog_help ? u_f1s : u_f1;
    const uint8_t c = scrn_str(status_row(), 1, f1, SL_SMSG, SL_SMSG); /* F1=Help and a bar */
    return scrn_str(status_row(), c, help, SL_SMSG, SL_SMSG) <= STATUS_BAR_COL;
}

/* ^Q pending, Caps Lock, Num Lock. */
static void status_flags(void)
{
    if (uimn.ed.prefix) {
        scrn_fill(status_row(), STATUS_PREFIX_COL, SL_STAT, '^', 1);
        scrn_fill(status_row(), STATUS_PREFIX_COL + 1, SL_STAT, 'Q', 1);
    } else
        scrn_fill(status_row(), STATUS_PREFIX_COL, SL_STAT, ' ', 2);

    scrn_fill(status_row(), STATUS_CAPS_COL, SL_STAT, uimn.keyflags & PKEY_CAPS ? 'C' : ' ', 1);
    scrn_fill(status_row(), STATUS_NUM_COL, SL_STAT, uimn.keyflags & PKEY_NUM ? 'N' : ' ', 1);
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
    scrn_str(status_row(), wide ? STATUS_POS_COL - 2 : STATUS_POS_COL, pos, SL_STAT, SL_STAT);
}

enum : uint8_t { STATUS_HELP_BAR_COL = 72 }; /* where the bar is while the help window has the focus */

void draw_status(void)
{
    scrn_fill(status_row(), 0, SL_STAT, ' ', PSCR_COLS);

    if (uimn.tree.workspace.focus == FOC_HELP && !uimn.help) { /* QBasic's: the longer keys, the bar moved, no position */
        scrn_str(status_row(), 1, draw_keys()->text, SL_SMSG, SL_SMSG);
        scrn_fill(status_row(), STATUS_HELP_BAR_COL, SL_STAT, GLYPH_V, 1);
        return;
    }

    if (!status_left())
        return;

    scrn_fill(status_row(), STATUS_BAR_COL, SL_STAT, GLYPH_V, 1);
    status_flags();
    status_pos();
}
