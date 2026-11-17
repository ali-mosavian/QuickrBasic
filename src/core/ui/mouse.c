/* The mouse in the editor, when a driver is there.
 *
 * # How it works
 * mous_poll turns the left button into events (press, drag, up, move), left in uimn.mouse.event at
 * uimn.mouse.row, col. uimn_getkey hands one on as KEY_MOUSE. The state is ui.h's uimn.mouse.
 *
 * The widget tree (tree.h) takes an event to the widget under it: the menu bar's title opens its
 * menu, the status bar's key runs; in the windows a press puts the cursor there, a drag selects,
 * the scroll bars, another window's focus, the pane's own. A move is the screen's: the debug
 * pane's tooltip.
 *
 * # asm structure (retired/asm32/ui/mous.asm)
 * | asm | here | |
 * |---|---|---|
 * | ms_init, ms_show, ms_hide | mous_init, mous_show, mous_hide | MIRROR |
 * | ms_poll | mous_poll | MIRROR; D1 |
 * | ms_title | mous_title | MIRROR; D1 |
 * | ms_main | mous_screen, mous_menubar, mous_statusbar, mous_workspace | D2 |
 * | MS_PRESS, MS_DRAG, MS_UP, MS_MOVE | same (mouse.h) | MIRROR |
 * | MS_KEY | KEY_MOUSE (ui.h) | MIRROR |
 * | g_ui.ms.* | uimn.mouse (ui.h) | MOVED |
 *
 * - D1. A flag result is a bool, true the success; mous_title gives the menu through a pointer.
 * - D2. Split by the widget whose cells the event is in, each a WidgetOps.mouse.
 */
#include "ui/mouse.h"
#include "session.h"
#include "plat/pkey.h"
#include "ui/debugpane.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/menu.h"
#include "ui/screen.h"
#include "ui/window.h"
#include "plat/pmou.h"

enum : uint8_t {
    BUTTON_LEFT = 1, /* PmouState.buttons */
};

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

/* The event of the left button, down or not, against m's last poll; 0 none. */
static uint8_t poll_event(const UiMouse *const m, const uint8_t down, const bool moved)
{
    if (down != m->button)
        return down ? MS_PRESS : MS_UP;
    if (down && moved)
        return MS_DRAG;
    if (!down && m->hover && moved)
        return MS_MOVE;

    return 0;
}

bool mous_poll(void)
{
    UiMouse *const m = &uimn.mouse;

    if (!m->on)
        return false;

    const PmouState s = pmou_state();
    const uint8_t down = s.buttons & BUTTON_LEFT;
    const bool moved = s.row != m->row || s.col != m->col;
    const uint8_t ev = poll_event(m, down, moved);
    m->button = down;

    if (!ev)
        return false;

    m->event = ev;
    m->row = s.row;
    m->col = s.col;
    return true;
}

bool mous_title(const uint8_t col, uint8_t *const menu)
{
    for (int m = NMENU - 1; m >= 0; m--) {
        const uint8_t x = menu_col(m) - 1;

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
static void scroll_bar(Session *const S, const uint8_t row)
{
    const UiEdit *const e = &uimn.ed;

    if (row == e->row0) {
        edit_up(S);
        return;
    }

    if (row == (uint8_t)(e->row0 + e->rows - 1)) {
        edit_down(S);
        return;
    }

    uint32_t thumb = e->line * (uint32_t)(e->rows - 3); /* above the thumb or below it */
    if (uimn.text.n)
        thumb /= uimn.text.n;
    if (row < (uint8_t)(thumb + e->row0 + 1))
        edit_pgup(S);
    else
        edit_pgdn(S);
}

/* A press on the status bar at col: the key there run. */
static void status_bar(Session *const S, const uint8_t col)
{
    if (uimn.help)
        return;
    for (const StatusKey *k = draw_keys()->keys; k->run; k++)
        if (col <= k->last) {
            k->run(S);
            return;
        }
}

/* The event in edit window w at row, col: its focus on a press, the cursor there, a selection
   on a drag or a Shift+click; a press on its scroll bar. */
static void edit_click(Session *const S, const uint8_t w, const uint8_t row, const uint8_t col)
{
    const UiMouse *const m = &uimn.mouse;

    if (m->event == MS_PRESS) /* a press: the focus there; a drag stays in its window */
        wind_focus(S, w);
    else {
        const uint8_t cur = wind_cur();

        if (cur != wind_active() && cur != W_IMM)
            return;
    }

    UiEdit *const e = &uimn.ed;

    if (uimn.tree.workspace.focus != FOC_EDIT || row < (uint8_t)e->row0 ||
        row >= (uint8_t)(e->row0 + e->rows))
        return;
    if (col <= e->x)
        return;

    const uint8_t c = (uint8_t)(col - e->x - 1); /* the text's column */
    if (c == (uint8_t)e->width) {
        if (m->event == MS_PRESS)
            scroll_bar(S, row);

        return;
    }

    if (c > (uint8_t)e->width)
        return;

    const uint32_t at = row - e->row0 + e->top; /* the line, the last at most */
    const uint32_t line = at < uimn.text.n ? at : uimn.text.n - 1;

    if (m->event == MS_DRAG || uimn.keyflags & PKEY_SHIFT)
        edit_sel_mark();
    else
        e->sel_on = false;
    if (!edit_goto(S, line))
        return;

    uimn.edline.col = (uint16_t)(col - e->x - 1 + e->left);
}

bool mous_screen(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    if (uimn.mouse.event != MS_MOVE)
        return false;

    dpan_hover(row, col);
    return true;
}

bool mous_menubar(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    if (uimn.mouse.event != MS_PRESS)
        return false;

    uint8_t menu;

    if (mous_title(col, &menu))
        menu_run(S, menu, true);

    return true;
}

bool mous_statusbar(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    if (uimn.mouse.event != MS_PRESS)
        return false;

    status_bar(S, col);
    return true;
}

/* Not a move or a let go: a press or a drag, the windows'. */
static bool pressed(void) { return uimn.mouse.event != MS_MOVE && uimn.mouse.event != MS_UP; }

bool mous_workspace(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    return pressed(); /* in no window */
}

bool mous_window(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    if (!pressed())
        return false;

    edit_click(S, wind_of(w), row, col);
    return true;
}
