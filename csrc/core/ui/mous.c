/* The mouse in the editor (src/ui/mouse.asm). */
#include "ui/mous.h"
#include "ui/dpan.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/menu.h"
#include "ui/scrn.h"
#include "ui/wind.h"
#include "plat/pmou.h"

void mous_init(void)
{
    if (pmou_init())
        uimn.mouse.on = true;
}

void mous_show(void)
{
    if (uimn.mouse.on)
        pmou_show();
}

void mous_hide(void)
{
    if (uimn.mouse.on)
        pmou_hide();
}

bool mous_poll(void)
{
    UiMouse *m = &uimn.mouse;
    if (!m->on)
        return false;
    PmouState s = pmou_state();
    uint8_t b = s.buttons & 1, ev;
    bool moved = s.row != m->row || s.col != m->col;
    if (b != m->button)
        ev = b ? MS_PRESS : MS_UP;
    else if (b && moved)
        ev = MS_DRAG;
    else if (!b && m->hover && moved)
        ev = MS_MOVE;
    else {
        m->button = b;
        return false;
    }
    m->button = b;
    m->event = ev;
    m->row = s.row;
    m->col = s.col;
    return true;
}

bool mous_title(uint8_t col, uint8_t *menu)
{
    for (int m = NMENU - 1; m >= 0; m--) {
        uint8_t x = menu_x[m] - 1;
        if (col < x)
            continue;
        if ((uint8_t)(x + scrn_slen(menu_title[m]) + 1) < col)
            return false;
        *menu = (uint8_t)m;
        return true;
    }
    return false;
}

/* A press on the edit window's scroll bar at row. */
static void scroll_bar(uint8_t row)
{
    const UiEdit *e = &uimn.ed;
    if (row == e->row0) {
        edit_up();
        return;
    }
    if (row == (uint8_t)(e->row0 + e->rows - 1)) {
        edit_down();
        return;
    }
    uint32_t thumb = e->line * (uint32_t)(e->rows - 3); /* above the thumb or below it */
    if (uimn.text.n)
        thumb /= uimn.text.n;
    if (row < (uint8_t)(thumb + e->row0 + 1))
        edit_pgup();
    else
        edit_pgdn();
}

/* A press on the status bar at col: the key there run. */
static void status_bar(uint8_t col)
{
    if (uimn.help)
        return;
    for (const StatusKey *k = draw_keys()->keys; k->run; k++)
        if (col <= k->last) {
            k->run();
            return;
        }
}

void mous_main(void)
{
    UiMouse *m = &uimn.mouse;
    uint8_t row = m->row, col = m->col;
    if (m->event == MS_MOVE) {
        dpan_hover(row, col);
        return;
    }
    if (m->event == MS_UP)
        return;
    if (!row) {
        uint8_t menu;
        if (m->event == MS_PRESS && mous_title(col, &menu))
            menu_run(menu, true);
        return;
    }
    if (row == 24) {
        if (m->event == MS_PRESS)
            status_bar(col);
        return;
    }
    uint8_t w;
    if (!wind_at(row, col, &w))
        return;
    if (w == W_DBG) {
        dpan_mouse(row, col);
        return;
    }
    if (w < W_ED1)
        return;
    if (m->event == MS_PRESS) /* a press: the focus there; a drag stays in its window */
        wind_focus(w);
    else {
        uint8_t cur = wind_cur();
        if (cur != uimn.win.active && cur != W_IMM)
            return;
    }
    UiEdit *e = &uimn.ed;
    if (uimn.focus != FOC_EDIT || row < (uint8_t)e->row0 ||
        row >= (uint8_t)(e->row0 + e->rows))
        return;
    uint8_t c = col - e->x; /* the text's column + 1 */
    if (col <= e->x)
        return;
    c--;
    if (c == (uint8_t)e->width) {
        if (m->event == MS_PRESS)
            scroll_bar(row);
        return;
    }
    if (c > (uint8_t)e->width)
        return;
    uint32_t line = row - e->row0 + e->top; /* the line, the last at most */
    if (line >= uimn.text.n)
        line = uimn.text.n - 1;
    if (m->event == MS_DRAG || uimn.keyflags & 3) /* a drag, a Shift+click select */
        edit_sel_mark();
    else
        e->sel_on = false;
    if (!edit_goto(line))
        return;
    uimn.edline.col = (uint16_t)(col - e->x - 1 + e->left);
}
