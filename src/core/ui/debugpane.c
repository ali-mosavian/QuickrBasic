/* The debug pane: Watch, Locals and Call stack, right of the code at a stop.
 *
 * # How it works
 * At a stop the pane takes columns uimn.dp_col on, the edit windows' rows; a drag of its border
 * moves that (wind_drag). uimn.pane_on is ui.h's; the rest is static.
 *
 * Its windows show lists: the call stack's frames (debugstack.c), the selected frame's Locals
 * (debuglocals.c) and the Watch (debugwatch.c), each window a list's rows, one selected.
 *
 * The tooltip shows the Locals row of the variable under the mouse (uimn.tip_mouse) or the cursor.
 *
 * Each window is a widget, the pane's child (dpan_build), and the tooltip the last, drawn on top.
 * The pane's focus is the window's; dpan_layout sets their rects.
 *
 * # asm structure (retired/asm32/ui/dpane.asm)
 * | asm | here | |
 * |---|---|---|
 * | dp_open, dp_close | dpan_open, dpan_close | MIRROR |
 * | dp_draw, dp_key, dp_mouse | the pane's widgets (dpan_build): child_draw, child_key, pane_key, child_mouse | D1 |
 * | dp_hover | dpan_hover | MIRROR |
 * | dp_tip | dpan_tip | MIRROR |
 * | dp_frm1, dp_frames | debugstack.c | MOVED |
 * | dp_vars, dp_var1, dp_vtext, dp_val, dp_ispar, dp_match, dp_seen, dp_find | debuglocals.c | MOVED |
 * | dp_weval, dp_wval, dp_wdel | debugwatch.c | MOVED |
 * | dp_cpy, dp_dec, dp_rowcut | debugvalue.c | MOVED |
 * | every other dp_* | statics | MIRROR |
 * | g_ui.dp.dp_on, tt_ms | uimn.pane_on, uimn.tip_mouse (ui.h) | MOVED |
 * | g_ui.dp.* else, TDPANE, g_dp*, u_dp*, dg_dpw | statics | MIRROR |
 * | DP_COL, DP_W, DP_TW | ui.h | MOVED |
 *
 * - D1. By widget: each window draws, takes keys and clicks itself; Tab, Shift+Tab and Esc are
 *   the pane's.
 */
#include "ui/debugpane.h"
#include "session.h"
#include "ui/debuglocals.h"
#include "ui/debugstack.h"
#include "ui/debugwatch.h"
#include "ui/dialog.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/mouse.h"
#include "ui/screen.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "ui/window.h"

enum {
    NDP = 3, /* its windows: Watch, Locals, Call stack */
    DP_WAT = 0,
    DP_LOC = 1,
    DP_STK = 2,
};

/* A window's list: its title, its rows, row i's text. */
typedef struct {
    const char *title;
    uint8_t (*count)(void);
    const char *(*row)(uint8_t i);
} PaneList;

static const char *frame_row(const uint8_t i) { return dstk_at(i)->text; }

static const PaneList lists[NDP] = {
    [DP_WAT] = {"Watch", dwat_count, dwat_row},
    [DP_LOC] = {"Locals", dloc_count, dloc_row},
    [DP_STK] = {"Call stack", dstk_count, frame_row},
};

static struct {
    uint8_t frame;              /* the frame selected, 0 the stop's */
    uint8_t sel[NDP], top[NDP]; /* each window's row selected, its first row shown */
    uint8_t tip_row, tip_col;
} dp;

static uint8_t fld_text[DLBUF];
static LineBuf fld = {.text = fld_text, .cap = DLBUF}; /* Ins's expression */

static DlogCtl add_ctl[] = {
    {.kind = CT_FLD,
     .row = 1,
     .col = 14,
     .cols = 34,
     .rows = 1,
     .field = &fld,
     .label = "Expression:",
     .lrow = 2,
     .lcol = 2},
    {.kind = CT_BTN, .row = 6, .col = 12, .cols = 6, .text = uimn_ok},
    {.kind = CT_BTN, .row = 6, .col = 30, .cols = 10, .text = uimn_cancel},
};
static Dlog add_dlog = {
    .row = 8,
    .col = 14,
    .rows = 8,
    .cols = 52,
    .title = "Add Watch",
    .n = 3,
    .focus = 0,
    .sep = 5,
    .def = 1,
    .ctl = add_ctl,
};

static const char sfx_char[] = "%&!#$"; /* a name's suffix */

/* Window w's widget: the pane's child w (dpan_build). */
static Widget *win(const uint8_t w) { return uimn.tree.pane.children[w]; }

/* The pane's columns, from the line it shares with the edit windows, and a row's characters
   within its borders. */
static uint8_t width(void) { return (uint8_t)(PSCR_COLS - uimn.dp_col + 1); }
static uint8_t text_width(void) { return (uint8_t)(width() - 2); }

/* dp_geo: each window's title row and rows: the edit windows' rows, three tenths to Watch and to
   Call stack, the rest to Locals. */
void dpan_layout(void)
{
    const uint8_t all = win_rows() - uimn.win.win[W_IMM].height;
    const uint8_t q = (uint8_t)(all * 3 / 10);
    const uint8_t loc = all - 2 * q;
    const uint8_t col = uimn.dp_col - 1;
    win(DP_WAT)->rect = (Rect){1, col, q, width()};

    win(DP_LOC)->rect = (Rect){q + 1, col, loc, width()};

    win(DP_STK)->rect = (Rect){q + 1 + loc, col, q, width()};
}

/* dp_item: row i of window w, nullptr none. */
static const char *item(const uint8_t w, const uint32_t i)
{
    return i < lists[w].count() ? lists[w].row((uint8_t)i) : nullptr;
}

static bool has_focus(const uint8_t w)
{
    return uimn.tree.workspace.focus == FOC_PANE && uimn.tree.pane.focus == w;
}

/* Screen row row the active edit window's place in its text: the line's square. */
static bool marks(const uint8_t row)
{
    const UiEdit *const e = &uimn.ed;

    if (!e->rows || row < e->row0 || row >= e->row0 + e->rows)
        return false;

    const uint32_t last = uimn.text.n > 1 ? uimn.text.n - 1 : 1;
    return row == e->row0 + e->line * (uint32_t)(e->rows - 1) / last;
}

/* dp_line: row r of window w: the line it shares, the square at the code's place on it; its
   border; its item. */
static void line1(const uint8_t w, const uint8_t r)
{
    const uint8_t row = win(w)->rect.row + 1 + r;
    const uint32_t i = dp.top[w] + r;
    const uint8_t slot = has_focus(w) && (uint8_t)i == dp.sel[w] ? SL_SEL : SL_ED;
    const bool cur = w == DP_STK && (uint8_t)i == dp.frame && (uint8_t)i < dstk_count();
    const uint8_t tw = text_width();
    uint8_t col = scrn_fill(row, win(w)->rect.col, SL_ED, marks(row) ? GLYPH_SQUARE : GLYPH_V, 1);
    col = scrn_fill(row, col, slot, cur ? GLYPH_POINT_RIGHT : ' ', 1);
    scrn_fill(row, col, slot, ' ', tw - 1);
    const char *const s = item(w, i);

    if (s) {
        uint16_t n = 0;

        while (s[n] && n < tw - 1)
            n++;

        scrn_strn(row, col, (const uint8_t *)s, n, slot);
    }

    scrn_fill(row, PSCR_COLS - 1, SL_ED, GLYPH_V, 1);
}

/* dp_draw: window x, its title and rows. */
static void child_draw(Widget *const x, Session *const S)
{
    dpan_layout();
    const uint8_t w = tree_index(x);
    const bool top = !w; /* the top window's corners, else joins */

    draw_title(
        x->rect.row,
        x->rect.col,
        width(),
        top ? GLYPH_T_DOWN : GLYPH_V_RIGHT,
        top ? GLYPH_TOP_RIGHT : GLYPH_V_LEFT,
        lists[w].title,
        has_focus(w),
        0
    );

    for (uint8_t r = 0; r + 1 < x->rect.rows; r++)
        line1(w, r);
}

/* The stop's procedure: the Watch's scope. */
static uint32_t stop_proc(void) { return dstk_count() ? dstk_at(0)->at.proc : 0; }

/* Locals of frame dp.frame. */
static void read_locals(Session *const S)
{
    if (dstk_count())
        dloc_read(S, &dstk_at(dp.frame)->at, dstk_newest(dp.frame));
}

void dpan_open(Session *const S)
{
    uimn.pane_on = true;
    dloc_stop();
    uimn.tree.pane.focus = DP_WAT;
    dp.frame = 0;
    memset(dp.sel, 0, sizeof dp.sel);
    memset(dp.top, 0, sizeof dp.top);
    dstk_read(S);
    read_locals(S);
    dwat_eval(S, stop_proc());
    wind_imm_hide();
    wind_layout();
}

void dpan_close(void)
{
    if (!uimn.pane_on)
        return;

    uimn.pane_on = false;

    if (uimn.tree.workspace.focus == FOC_PANE)
        uimn.tree.workspace.focus = FOC_EDIT;

    wind_imm_show();
    wind_layout();
}

/* dp_vis: window w's first row shown: its selected row in view. */
static void visible(const uint8_t w)
{
    dpan_layout();
    const uint8_t s = dp.sel[w];

    if (s < dp.top[w]) {
        dp.top[w] = s;
        return;
    }

    const uint8_t rows = win(w)->rect.rows - 1;

    if (!rows || (uint8_t)(s - dp.top[w]) < rows)
        return;

    dp.top[w] = s - rows + 1;
}

/* dp_go: frame i selected: its Locals, the cursor at the start of its line. */
static void go(Session *const S, const uint8_t i)
{
    dp.frame = i;
    read_locals(S);
    const int32_t line = dstk_at(i)->line;

    if (line == -1)
        return;

    edit_commit(S);
    view_goline(S, (uint32_t)line);
    uimn.ed.sel_on = false;
    edit_load(S, (uint32_t)line - uimn.text.first);
    uimn.edline.col = 0;
    edit_ensure();
}

/* dp_wadd: Ins in Watch: an expression asked, added, evaluated. */
static void watch_add(Session *const S)
{
    add_dlog.focus = 0;
    fld.len = 0;
    fld.col = 0;

    if (dlog_run(S, &add_dlog) != 1 || !fld.len)
        return;
    if (!dwat_add(fld.text, fld.len))
        return;

    dp.sel[DP_WAT] = dwat_count() - 1;
    dwat_eval(S, stop_proc());
    visible(DP_WAT);
}

/* dp_wdel: Del in Watch: the selected watch dropped. */
static void watch_del(void)
{
    const uint8_t i = dp.sel[DP_WAT];

    if (i >= dwat_count())
        return;

    dwat_del(i);

    if (i >= dwat_count() && i > 0)
        dp.sel[DP_WAT] = i - 1;
}

/* Up or Down in window w: its row selected moved by one, kept in view. */
static void sel_step(const uint8_t w, const uint8_t scan)
{
    const bool up = scan == SCAN_UP;
    const int32_t s = dp.sel[w] + (up ? -1 : 1);

    if (up ? s < 0 : s >= lists[w].count())
        return;

    dp.sel[w] = (uint8_t)s;
    visible(w);
}

/* A key with no character (an extended one) in window w. */
static bool scan_key(Session *const S, const uint8_t w, const uint8_t scan)
{
    if (w == DP_WAT && scan == SCAN_INS)
        watch_add(S);
    else if (w == DP_WAT && scan == SCAN_DEL)
        watch_del();
    else if (scan == SCAN_UP || scan == SCAN_DOWN)
        sel_step(w, scan);
    else
        return false;

    return true;
}

/* Window x's keys: Up, Down a row; Enter a frame; Ins, Del in Watch a watch. */
static bool child_key(Widget *const x, Session *const S, const uint16_t key)
{
    const uint8_t w = tree_index(x);
    const uint8_t c = (uint8_t)key;

    if (c == CH_ENTER) {
        if (w == DP_STK && dp.sel[w] < dstk_count())
            go(S, dp.sel[w]);

        return true;
    }

    return !c && key != KEY_SHIFT_TAB && scan_key(S, w, (uint8_t)(key >> 8));
}

/* The pane's: Tab, Shift+Tab its windows; Esc back to the edit window. */
static bool pane_key(Widget *const x, Session *const S, const uint16_t key)
{
    const uint8_t w = x->focus;
    const uint8_t c = (uint8_t)key;

    if (c == CH_TAB)
        x->focus = w + 1 < NDP ? w + 1 : 0;
    else if (key == KEY_SHIFT_TAB)
        x->focus = w ? w - 1 : NDP - 1;
    else if (c == CH_ESC)
        wind_focus(S, wind_active());
    else
        return false;

    return true;
}

/* A press in window x at row: the pane's focus, x's; its row selected, a frame's row the frame. A
   drag: nothing; a move is the screen's. */
static bool child_mouse(Widget *const x, Session *const S, const uint8_t row, const uint8_t col)
{
    const uint8_t ev = uimn.mouse.event;

    if (ev != MS_PRESS)
        return ev == MS_DRAG;

    wind_focus(S, W_DBG);
    dpan_layout();
    const uint8_t w = tree_index(x);
    uimn.tree.pane.focus = w;
    const uint8_t r = row - x->rect.row;

    if (!r)
        return true;

    const uint8_t i = (uint8_t)(r - 1 + dp.top[w]);

    if (i >= lists[w].count())
        return true;

    dp.sel[w] = i;

    if (w == DP_STK)
        go(S, i);

    return true;
}

static void tip_draw(Widget *const x, Session *const S) { dpan_tip(S); }

static const WidgetOps child_ops = {.draw = child_draw, .key = child_key, .mouse = child_mouse};
static const WidgetOps tip_ops = {.draw = tip_draw};
static const WidgetOps pane_ops = {.key = pane_key};

static_assert(countof(uimn.tree.dp) == DPAN_KIDS);

void dpan_build(Widget *const pane, Widget kids[static DPAN_KIDS])
{
    pane->ops = &pane_ops;

    for (uint32_t i = 0; i < DPAN_KIDS; i++) {
        kids[i] = (Widget){.ops = i < NDP ? &child_ops : &tip_ops, .visible = true};

        tree_add(pane, &kids[i]);
    }
}

void dpan_hover(const uint8_t row, const uint8_t col)
{
    uimn.tip_mouse = true;
    dp.tip_row = row;
    dp.tip_col = col;
}

static bool is_suffix(const uint8_t c) { return c && strchr(sfx_char, c); }

/* The name at column col of s, len long, not in a string or after a '. *start its
   first column, *n its length (its suffix left out), *sfx its suffix, 0 none. */
static bool word(
    const uint8_t *const s,
    const uint32_t len,
    uint32_t col,
    uint32_t *const start,
    uint32_t *const n,
    uint8_t *const sfx
)
{
    if (col >= len)
        return false;
    if (is_suffix(s[col])) { /* on a suffix: the name before it */
        if (!col)
            return false;

        col--;
    }

    if (!is_name_char(s[col]))
        return false;

    uint32_t b = col;

    while (b && is_name_char(s[b - 1]))
        b--;
    if (!is_letter(s[b]))
        return false;

    uint32_t e = col;

    do
        e++;
    while (e < len && is_name_char(s[e]));

    *start = b;
    *n = e - b;
    *sfx = e < len && is_suffix(s[e]) ? s[e] : 0;
    bool quoted = false;

    for (uint32_t i = 0; i < b; i++) {
        if (s[i] == '"')
            quoted = !quoted;
        else if (s[i] == '\'' && !quoted)
            return false;
    }

    return true;
}

/* The tooltip's text line: the cursor's, else the mouse's in the active window; *text, *len,
   *col in it, *line its number. False: none. */
static bool tip_at(
    Session *const S,
    const uint8_t **const text,
    uint32_t *const len,
    uint32_t *const col,
    uint32_t *const line
)
{
    const UiEdit *const ed = &uimn.ed;

    if (!uimn.tip_mouse) {
        if (uimn.tree.workspace.focus != FOC_EDIT)
            return false;

        *col = uimn.edline.col;
        *text = uimn.edline.text;
        *len = uimn.edline.len;
        *line = ed->line;
        return true;
    }

    uint8_t w;

    if (!wind_at(dp.tip_row, dp.tip_col, &w) || w != wind_active())
        return false;
    if (dp.tip_row < ed->row0 || dp.tip_row - ed->row0 >= ed->rows)
        return false;

    const uint32_t l = dp.tip_row - ed->row0 + ed->top;

    if (l >= uimn.text.n || dp.tip_col <= ed->x)
        return false;

    const uint32_t c = dp.tip_col - ed->x - 1u;

    if (c >= ed->width)
        return false;

    *col = c + ed->left;
    *line = (uint16_t)l;
    *len = text_get(S, *line, &uimn.line);
    *text = uimn.line.text;
    return true;
}

/* Tooltip s for the name at column start of line line: under it, else over it, in the window. */
static void tip_show(const char *const s, const uint32_t start, const uint32_t line)
{
    const UiEdit *const ed = &uimn.ed;
    uint16_t r = (uint16_t)(line - ed->top + ed->row0 + 1);

    if (r >= ed->row0 + ed->rows)
        r -= 2;

    const uint32_t sn = strlen(s);
    const uint32_t edge = ed->x + 1u + ed->width; /* past the window */
    const uint32_t c0 = ed->x + 1u + (start > ed->left ? start - ed->left : 0);
    const uint32_t c = c0 + sn + 2 > edge ? edge - sn - 2 : c0;
    const uint8_t row = (uint8_t)r;
    uint8_t x = scrn_fill(row, (uint8_t)c, SL_BAR, ' ', 1);
    x = scrn_strn(row, x, (const uint8_t *)s, (uint16_t)sn, SL_BAR);
    scrn_fill(row, x, SL_BAR, ' ', 1);
}

void dpan_tip(Session *const S)
{
    if (!uimn.pane_on || !uimn_stopped() || !dstk_count())
        return;
    if (dstk_at(dp.frame)->view != uimn.ed.view) /* in the frame's view */
        return;

    const uint8_t *text;
    uint32_t len, col, line;

    if (!tip_at(S, &text, &len, &col, &line))
        return;

    uint32_t start, n;
    uint8_t sfx;

    if (!word(text, len, col, &start, &n, &sfx))
        return;

    const char *const s = dloc_find(S, text + start, n, sfx);

    if (s)
        tip_show(s, start, line);
}
