/* The windows: their rows, focus, Split, grow, shrink and maximise.
 *
 * # How it works
 * They stack from row 1 to row 23: W_ED1, W_ED2 (Split) and W_IMM, each uimn.win.win[w].height rows
 * with its title, 0 hidden. With uimn.win.vert, ED2 is beside ED1, in its rows, each half the
 * columns.
 *
 * An edit window has a bottom row: the sideways scroll bar's with the focus. The active edit
 * window's state is uimn.ed and uimn.edline.col; the other's waits in its win[].ed and win[].col.
 *
 * With the debug pane (uimn.pane_on) the edit windows have uimn.dp_col columns, the pane the rest. A
 * window is W_ED1, W_ED2, W_IMM, or W_DBG for the pane.
 *
 * # asm structure (retired/asm32/ui/wind.asm)
 * | asm | here | |
 * |---|---|---|
 * | wn_init, wn_layout, wn_draw | wind_init, wind_layout, wind_draw | MIRROR |
 * | wn_act, wn_focus, wn_cur | wind_act, wind_focus, wind_cur | MIRROR |
 * | wn_next, wn_prev | wind_next, wind_prev | MIRROR |
 * | wn_at | wind_at | MIRROR; D1 |
 * | wn_split, wn_vsplit, wn_grow, wn_shrink, wn_max | wind_split, wind_vsplit, wind_grow, wind_shrink, wind_max | MIRROR |
 * | wn_beside, wn_ptr, wn_swap, wn_curh, wn_move, wn_shown, wn_spare, wn_min, wn_unmax, g_wnord | statics | MIRROR |
 * | TWIN | UiWin (ui.h) | MOVED |
 * | g_ui.wn.* | uimn.win (UiWins, ui.h) | MOVED |
 * | NWIN, W_HELP, W_ED1, W_ED2, W_IMM, W_DBG | same (ui.h) | MOVED |
 *
 * - D1. A flag result is a bool, true the success; the window is returned through a pointer.
 */
#include "ui/window.h"
#include "session.h"
#include "ui/debugpane.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/mouse.h"
#include "ui/view.h"

enum : uint8_t {
    ED1_INIT = 20, /* the heights at start */
    IMM_INIT = 3,
    SPLIT_MIN = 6, /* ED1's height that Split halves, at least */
};

static UiWin *win(const uint8_t w) { return &uimn.win.win[w]; }

/* Window w is ED2 beside ED1, shown. */
static bool beside(const uint8_t w) { return w == W_ED2 && uimn.win.vert && win(W_ED2)->height; }

enum : uint8_t { COLS_MIN = 10 }; /* an edit window's columns beside another, at least */

/* ED1's columns beside ED2: a drag's, within both windows' least, else half. */
static uint8_t ed1_cols(void)
{
    const uint8_t all = uimn.win.cols, want = uimn.win.ed1_cols ? uimn.win.ed1_cols : all / 2;
    return want < COLS_MIN ? COLS_MIN : want > all - COLS_MIN ? (uint8_t)(all - COLS_MIN) : want;
}

void wind_layout(void)
{
    UiWins *const ws = &uimn.win;
    ws->cols = uimn.pane_on ? uimn.dp_col : uimn.cols;
    uint8_t row = 1; /* the next window's title row */
    for (uint8_t w = 0; w < NWIN; w++) {
        UiWin *const p = win(w);

        if (beside(w)) {
            p->height = win(W_ED1)->height;
            row -= p->height; /* ED1's rows again */
        }

        const uint16_t first = row + 1; /* the first row under the title */
        Rect help;                      /* the help window has no widget */
        Rect *const r = w == W_HELP ? &help : &wind_widget(w)->rect;
        *r = (Rect){row, 0, p->height, uimn.cols};

        if (w != W_HELP)
            wind_widget(w)->visible = p->height != 0;

        if (w == W_IMM) {
            uimn.imm.row0 = first;
            uimn.imm.rows = p->height ? p->height - 1 : 0;
        } else {
            UiEdit *const e =
                w == wind_active() ? &uimn.ed : &p->ed; /* the active one's: uimn.ed */
            uint32_t cols = ws->cols;                   /* all, or a half */
            e->x = 0;

            if (ws->vert && win(W_ED2)->height) {
                cols = ed1_cols();

                if (w == W_ED2) /* on ED1's right border: one line between them */
                    e->x = (uint8_t)(cols - 1);
            }

            r->col = e->x;
            r->cols = (uint8_t)(w == W_ED2 ? ws->cols - e->x : cols); /* ED2: the rest */
            e->width = (uint16_t)(r->cols - 2);

            if (beside(w)) { /* the shared line ED1's: its scroll bar's clicks */
                r->col++;
                r->cols--;
            }

            e->row0 = first;
            e->rows = p->height >= 2 ? p->height - 2 : 0; /* the title, the bottom row */
        }

        row += p->height;
    }

    const uint8_t imm_row = uimn.imm.row0 - 1; /* its title */
    uimn.tree.editors.rect = (Rect){1, 0, imm_row - 1, ws->cols};

    uimn.tree.pane.rect =
        (Rect){1, uimn.dp_col - 1, win_rows() - win(W_IMM)->height, uimn.cols - uimn.dp_col + 1};

    uimn.tree.pane.visible = uimn.pane_on;
    dpan_layout();
}

void wind_init(void)
{
    win(W_ED1)->height = ED1_INIT; /* 25 rows'; wind_fit fits them to the screen's */
    win(W_IMM)->height = IMM_INIT;
    uimn.dp_col = DP_COL;
    uimn.tree.editors.focus = 0;
    wind_layout();
}

/* uimn.ed and edit window w's state exchanged. */
static void swap(const uint8_t w)
{
    const UiEdit t = uimn.ed;
    uimn.ed = win(w)->ed;
    win(w)->ed = t;
}

void wind_ed_draw(Widget *const x, Session *const S)
{
    const uint8_t w = wind_of(x);

    if (w == wind_active()) {
        draw_edit(S, false);
    } else {
        swap(w); /* the other edit window: its state in uimn.ed while it draws */
        view_apply();
        draw_edit(S, true);
        swap(w);
        view_apply();
    }

    if (w == W_ED2 && beside(w)) { /* drawn over ED1's border: it again, its bar if it has one */
        const bool act1 = wind_active() == W_ED1;
        draw_seam(act1 ? &uimn.ed : &win(W_ED1)->ed, act1 && uimn.tree.workspace.focus == FOC_EDIT);
    }
}

void wind_imm_draw(Widget *const x, Session *const S) { draw_imm(); }

Widget *wind_widget(const uint8_t w)
{
    if (w == W_IMM)
        return &uimn.tree.imm;

    return w == W_DBG ? &uimn.tree.pane : &uimn.tree.ed[w - W_ED1];
}

uint8_t wind_of(const Widget *const x)
{
    for (uint8_t w = W_ED1; w <= W_DBG; w++)
        if (wind_widget(w) == x)
            return w;

    return W_HELP;
}

void wind_act(Session *const S, const uint8_t w)
{
    edit_commit(S);
    uimn.tree.workspace.focus = FOC_EDIT;

    if (w == wind_active())
        return;

    win(wind_active())->col = uimn.edline.col; /* the old one's state to its ed */
    swap(wind_active());
    uimn.tree.editors.focus = w - W_ED1;
    swap(w); /* the new one's from its ed */
    view_apply();
    const uint16_t col = win(w)->col;
    edit_load(S, uimn.ed.line);
    uimn.edline.col = col;
}

void wind_focus(Session *const S, const uint8_t w)
{
    if (w != W_IMM && w != W_DBG) {
        wind_act(S, w);
        return;
    }

    edit_commit(S);
    uimn.tree.workspace.focus = w == W_IMM ? FOC_IMM : FOC_PANE;
}

uint8_t wind_cur(void)
{
    if (uimn.tree.workspace.focus == FOC_EDIT)
        return wind_active();

    return uimn.tree.workspace.focus == FOC_IMM ? W_IMM : W_DBG;
}

static const uint8_t order[] = {W_ED1, W_ED2, W_DBG, W_IMM}; /* F6's */
enum { NWORD = countof(order) };

/* The focus to the window shown d places on in order. */
static void step(Session *const S, const uint32_t d)
{
    const uint8_t cur = wind_cur();
    uint32_t i = 0;

    while (order[i] != cur)
        i++;
    for (;;) {
        i = (i + d) % NWORD;
        const uint8_t w = order[i];

        if (w == W_DBG ? uimn.pane_on : win(w)->height != 0) {
            wind_focus(S, w);
            return;
        }
    }
}

void wind_next(Session *const S) { step(S, 1); }
void wind_prev(Session *const S) { step(S, NWORD - 1); }

bool wind_at(const uint8_t row, const uint8_t col, uint8_t *const w)
{
    *w = wind_of(tree_at(&uimn.tree.workspace, row, col));
    return *w != W_HELP;
}

/* The heights before Ctrl+F10, if maximised. */
static void unmax(void)
{
    UiWins *const ws = &uimn.win;

    if (!ws->maxed)
        return;
    for (uint8_t w = 0; w < NWIN; w++)
        win(w)->height = ws->saved[w];

    ws->maxed = false;
    ws->vert = ws->vert_saved;
    wind_layout();
}

/* ED2 starts as ED1. */
static void copy_ed1(void)
{
    win(W_ED2)->ed = uimn.ed;
    win(W_ED2)->col = uimn.edline.col;
    wind_layout();
}

/* One window again, with the active one's state, which uimn.ed holds. */
static void unsplit(void)
{
    uimn.tree.editors.focus = 0;

    if (!uimn.win.vert)
        win(W_ED1)->height += win(W_ED2)->height;

    win(W_ED2)->height = 0;
    uimn.win.vert = false;
    wind_layout();
}

void wind_split(Session *const S)
{
    unmax();
    edit_commit(S);

    if (win(W_ED2)->height) {
        unsplit();
        return;
    }

    const uint8_t h = win(W_ED1)->height; /* ED2 hidden: ED1 is the active one */
    if (h < SPLIT_MIN)
        return;

    win(W_ED2)->height = h >> 1;
    win(W_ED1)->height -= h >> 1;
    copy_ed1();
}

void wind_vsplit(Session *const S)
{
    unmax();
    edit_commit(S);

    if (win(W_ED2)->height) {
        unsplit();
        return;
    }

    uimn.win.vert = true;
    win(W_ED2)->height = win(W_ED1)->height;
    copy_ed1();
}

/* The window with the focus, ED1 for ED2 beside it or the pane: whose rows change. */
static uint8_t cur_rows(void)
{
    const uint8_t w = wind_cur();
    return w == W_DBG || beside(w) ? W_ED1 : w;
}

/* Window w's least height: its title and a row, an edit window's bottom row too. */
static uint8_t least(const uint8_t w) { return w == W_IMM ? 2 : 3; }

/* The window shown next to window w, d 1 under, -1 over; false: none. */
static bool shown(const uint8_t w, const int d, uint8_t *const next)
{
    for (uint32_t i = w + d; i < NWIN; i += d)
        if (win(i)->height && !beside(i)) {
            *next = (uint8_t)i;
            return true;
        }

    return false;
}

/* A row from window from to window to. */
static void move(const uint8_t from, const uint8_t to)
{
    win(from)->height--;
    win(to)->height++;
    wind_layout();
}

void wind_grow(void)
{
    unmax();
    const uint8_t w = cur_rows(); /* w gains, a neighbour that can spare a row loses: under first */
    for (int d = 1; d >= -1; d -= 2) {
        uint8_t n;

        if (shown(w, d, &n) && win(n)->height > least(n)) {
            move(n, w);
            return;
        }
    }
}

void wind_shrink(void)
{
    unmax();
    const uint8_t w = cur_rows(); /* w loses, the neighbour under it, else over it, gains */
    if (win(w)->height <= least(w))
        return;

    uint8_t n;

    if (shown(w, 1, &n) || shown(w, -1, &n))
        move(w, n);
}

/* A drag's line: none, the debug pane's, ED1's and ED2's beside it, a window's title row. */
enum : uint8_t { SEAM_NONE, SEAM_PANE, SEAM_BESIDE, SEAM_TITLE };

/* The line under the press at row, col; *w the window whose title row it is. */
static uint8_t seam_at(const uint8_t row, const uint8_t col, uint8_t *const w)
{
    const Rect *const pane = &uimn.tree.pane.rect;

    if (uimn.pane_on && col == pane->col && row >= pane->row && row < pane->row + pane->rows)
        return SEAM_PANE;

    if (beside(W_ED2)) {
        const UiEdit *const e2 = wind_active() == W_ED2 ? &uimn.ed : &win(W_ED2)->ed;
        const bool bar = wind_active() == W_ED1 && uimn.tree.workspace.focus == FOC_EDIT &&
                         !uimn.pane_on; /* ED1's scroll bar on it: its title and foot rows */
        const bool ends = row == e2->row0 - 1 || row == e2->row0 + e2->rows;

        if (col == e2->x && row >= e2->row0 - 1 && row <= e2->row0 + e2->rows && (!bar || ends))
            return SEAM_BESIDE;
    }

    uint8_t over;

    for (uint8_t i = W_ED1; i < NWIN; i++)
        if (win(i)->height && !beside(i) && wind_widget(i)->rect.row == row &&
            shown(i, -1, &over) && over != W_HELP) {
            *w = i;
            return SEAM_TITLE;
        }

    return SEAM_NONE;
}

bool wind_drag(void)
{
    static uint8_t held, title; /* the line pressed, not let go; a title row's window */
    const UiMouse *const m = &uimn.mouse;

    if (m->event == MS_PRESS) { /* a title row's press is its window's still: a click as ever */
        held = uimn.win.maxed ? SEAM_NONE : seam_at(m->row, m->col, &title);
        return held && held != SEAM_TITLE;
    }

    if (!held)
        return false;
    if (m->event == MS_UP) {
        held = SEAM_NONE;
        return true;
    }

    if (m->event != MS_DRAG)
        return true;

    if (held == SEAM_PANE && uimn.pane_on) {
        const uint8_t to = m->col + 1; /* the line at the mouse */
        uimn.dp_col = to < DP_COLMIN                ? DP_COLMIN
                      : to > uimn.cols - DP_PANEMIN ? uimn.cols - DP_PANEMIN
                                                    : to;
    } else if (held == SEAM_BESIDE) {
        uimn.win.ed1_cols = m->col + 1;
    } else if (held == SEAM_TITLE) {
        uint8_t over;

        if (!win(title)->height || !shown(title, -1, &over))
            return true;

        const uint8_t top = wind_widget(over == W_ED2 && beside(W_ED2) ? W_ED1 : over)->rect.row;
        const int moved = m->row - wind_widget(title)->rect.row; /* down: the one over it grows */
        const int a = win(over)->height + moved, b = win(title)->height - moved;

        if (m->row <= top || a < least(over) || b < least(title))
            return true;

        win(over)->height = (uint8_t)a;
        win(title)->height = (uint8_t)b;
    }

    wind_layout();
    return true;
}

void wind_fit(void)
{
    unmax();
    uint8_t sum = 0;

    for (uint8_t w = 0; w < NWIN; w++)
        if (!beside(w))
            sum += win(w)->height;

    const int delta = win_rows() - sum; /* to ED1, else the heights as at the start */

    if (win(W_ED1)->height + delta >= least(W_ED1)) {
        win(W_ED1)->height = (uint8_t)(win(W_ED1)->height + delta);
    } else {
        win(W_HELP)->height = win(W_ED2)->height = 0;
        uimn.win.vert = false;
        uimn.tree.editors.focus = 0;
        win(W_IMM)->height = uimn.pane_on ? 0 : IMM_INIT;
        win(W_ED1)->height = win_rows() - win(W_IMM)->height;
    }

    wind_layout();
}

void wind_imm_hide(void)
{
    unmax();
    const uint8_t h = win(W_IMM)->height;
    uint8_t over;

    if (!h || !shown(W_IMM, -1, &over))
        return;

    uimn.win.imm_saved = h;
    win(over)->height += h;
    win(W_IMM)->height = 0;

    if (uimn.tree.workspace.focus == FOC_IMM)
        uimn.tree.workspace.focus = FOC_EDIT;
}

void wind_imm_show(void)
{
    unmax();
    const uint8_t h = uimn.win.imm_saved;
    uimn.win.imm_saved = 0;
    uint8_t over;

    if (!h || win(W_IMM)->height || !shown(W_IMM, -1, &over))
        return;

    const uint8_t spare = win(over)->height - least(over); /* the rows over it can give */
    const uint8_t give = h < spare ? h : spare;

    if (give < least(W_IMM))
        return;

    win(over)->height -= give;
    win(W_IMM)->height = give;
}

void wind_max(void)
{
    UiWins *const ws = &uimn.win;

    if (ws->maxed) {
        unmax();
        return;
    }

    ws->vert_saved = ws->vert;
    ws->vert = false;

    for (uint8_t w = 0; w < NWIN; w++) { /* the heights kept, all hidden */
        ws->saved[w] = win(w)->height;
        win(w)->height = 0;
    }

    win(cur_rows())->height = win_rows();
    ws->maxed = true;
    wind_layout();
}
