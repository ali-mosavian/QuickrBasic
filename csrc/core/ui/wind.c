/* The windows (src/ui/win.asm). */
#include "ui/wind.h"
#include "ui/dpan.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/view.h"

static UiWin *win(uint8_t w) { return &uimn.win.win[w]; }

/* Window w is ED2 beside ED1, shown. */
static bool beside(uint8_t w)
{
    return w == W_ED2 && uimn.win.vert && win(W_ED2)->height;
}

void wind_layout(void)
{
    UiWins *ws = &uimn.win;
    ws->cols = uimn.pane_on ? DP_COL : 80;
    uint8_t row = 1; /* the next window's title row */
    for (uint8_t w = 0; w < NWIN; w++) {
        UiWin *p = win(w);
        if (beside(w)) {
            p->height = win(W_ED1)->height;
            row -= p->height; /* ED1's rows again */
        }
        uint16_t first = row + 1; /* the first row under the title */
        if (w == W_IMM) {
            uimn.imm.row0 = first;
            uimn.imm.rows = p->height ? p->height - 1 : 0;
        } else {
            UiEdit *e = w == ws->active ? &uimn.ed : &p->ed; /* the active one's is uimn.ed */
            uint32_t cols = ws->cols; /* all, or a half */
            e->x = 0;
            if (ws->vert && win(W_ED2)->height) {
                cols >>= 1;
                if (w == W_ED2)
                    e->x = (uint8_t)cols;
            }
            e->width = (uint16_t)(cols - 2);
            e->row0 = first;
            e->rows = p->height >= 2 ? p->height - 2 : 0; /* the title, the bottom row */
        }
        row += p->height;
    }
}

void wind_init(void)
{
    win(W_ED1)->height = 20;
    win(W_IMM)->height = 3;
    uimn.win.active = W_ED1;
    wind_layout();
}

/* uimn.ed and edit window w's state exchanged. */
static void swap(uint8_t w)
{
    UiEdit t = uimn.ed;
    uimn.ed = win(w)->ed;
    win(w)->ed = t;
}

void wind_draw(void)
{
    for (uint8_t w = W_ED1; w < W_IMM; w++) {
        if (!win(w)->height)
            continue;
        if (w == uimn.win.active) {
            draw_edit(false);
            continue;
        }
        swap(w); /* the other edit window: its state in uimn.ed while it draws */
        view_apply();
        draw_edit(true);
        swap(w);
        view_apply();
    }
    dpan_draw();
    if (win(W_IMM)->height)
        draw_imm();
}

void wind_act(uint8_t w)
{
    edit_commit();
    uimn.focus = FOC_EDIT;
    if (w == uimn.win.active)
        return;
    win(uimn.win.active)->col = uimn.edline.col; /* the old one's state to its ed */
    swap(uimn.win.active);
    uimn.win.active = w;
    swap(w); /* the new one's from its ed */
    view_apply();
    uint16_t col = win(w)->col;
    edit_load(uimn.ed.line);
    uimn.edline.col = col;
}

void wind_focus(uint8_t w)
{
    uint8_t focus;
    if (w == W_IMM)
        focus = FOC_IMM;
    else if (w == W_DBG)
        focus = FOC_PANE;
    else {
        wind_act(w);
        return;
    }
    edit_commit();
    uimn.focus = focus;
}

uint8_t wind_cur(void)
{
    if (uimn.focus == FOC_EDIT)
        return uimn.win.active;
    return uimn.focus == FOC_IMM ? W_IMM : W_DBG;
}

static const uint8_t order[] = {W_ED1, W_ED2, W_DBG, W_IMM}; /* F6's */
enum { NWORD = countof(order) };

/* The focus to the window shown d places on in order. */
static void step(uint32_t d)
{
    uint8_t cur = wind_cur();
    uint32_t i = 0;
    while (order[i] != cur)
        i++;
    for (;;) {
        i = (i + d) % NWORD;
        uint8_t w = order[i];
        if (w == W_DBG ? uimn.pane_on : win(w)->height != 0) {
            wind_focus(w);
            return;
        }
    }
}

void wind_next(void) { step(1); }
void wind_prev(void) { step(NWORD - 1); }

bool wind_at(uint8_t row, uint8_t col, uint8_t *w)
{
    if (uimn.pane_on && col >= DP_COL && row && /* the pane: its columns, the edit windows' rows */
        row <= (uint8_t)(23 - win(W_IMM)->height)) {
        *w = W_DBG;
        return true;
    }
    uint8_t past = 1;
    for (uint8_t i = 0; i < NWIN; i++) {
        if (beside(i)) /* in ED1's rows, counted there */
            continue;
        past += win(i)->height;
        if (row >= past)
            continue;
        if (i == W_ED1 && beside(W_ED2) && col >= uimn.win.cols >> 1)
            i = W_ED2; /* ED1's rows, ED2's columns */
        *w = i;
        return true;
    }
    return false;
}

/* The heights before Ctrl+F10, if maximised. */
static void unmax(void)
{
    UiWins *ws = &uimn.win;
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
    uimn.win.active = W_ED1;
    if (!uimn.win.vert)
        win(W_ED1)->height += win(W_ED2)->height;
    win(W_ED2)->height = 0;
    uimn.win.vert = false;
    wind_layout();
}

void wind_split(void)
{
    unmax();
    edit_commit();
    if (win(W_ED2)->height) {
        unsplit();
        return;
    }
    uint8_t h = win(W_ED1)->height; /* ED2 hidden: ED1 is the active one */
    if (h < 6)
        return;
    win(W_ED2)->height = h >> 1;
    win(W_ED1)->height -= h >> 1;
    copy_ed1();
}

void wind_vsplit(void)
{
    unmax();
    edit_commit();
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
    uint8_t w = wind_cur();
    return w == W_DBG || beside(w) ? W_ED1 : w;
}

/* Window w's least height: its title and a row, an edit window's bottom row too. */
static uint8_t least(uint8_t w) { return w == W_IMM ? 2 : 3; }

/* The window shown next to window w, d 1 under, -1 over; false: none. */
static bool shown(uint8_t w, int d, uint8_t *next)
{
    for (uint32_t i = w + d; i < NWIN; i += d)
        if (win(i)->height && !beside(i)) {
            *next = (uint8_t)i;
            return true;
        }
    return false;
}

/* A row from window from to window to. */
static void move(uint8_t from, uint8_t to)
{
    win(from)->height--;
    win(to)->height++;
    wind_layout();
}

void wind_grow(void)
{
    unmax();
    uint8_t w = cur_rows(), n; /* w gains, a neighbour that can spare a row loses: under first */
    for (int d = 1; d >= -1; d -= 2)
        if (shown(w, d, &n) && win(n)->height > least(n)) {
            move(n, w);
            return;
        }
}

void wind_shrink(void)
{
    unmax();
    uint8_t w = cur_rows(), n; /* w loses, the neighbour under it, else over it, gains */
    if (win(w)->height <= least(w))
        return;
    if (shown(w, 1, &n) || shown(w, -1, &n))
        move(w, n);
}

void wind_max(void)
{
    UiWins *ws = &uimn.win;
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
    win(cur_rows())->height = 23;
    ws->maxed = true;
    wind_layout();
}
