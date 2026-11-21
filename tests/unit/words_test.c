/* Words: deleted by Ctrl+Backspace and Ctrl+Del, selected by a double click; a line by a triple. */
#include "edfix.h"
#include "plat/host/host.h"
#include "plat/pkey.h"
#include "ui/edit.h"
#include "ui/mouse.h"
#include "ui/uimain.h"
#include "unit.h"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

static void opened(Session *const S)
{
    fx_open_program(S, "sub hi\r\nprint total\r\nprint 2\r\nend sub\r\n");
    uimn_rows(25);
}

/* Ctrl+Backspace typed a 7Fh character; it deletes back to the word's start, then the spaces
   and the word before; at the line's start it joins the line above. */
TEST(words_ctrl_backspace_deletes_back)
{
    opened(S);
    fx_cursor_on(S, "print total", "print");
    fx_key(S, FX_END, 0);
    fx_key(S, FX_CTRL_BACKSPACE, PKEY_CTRL);
    CHECK(!strcmp(fx_line(), "print ") && uimn.edline.col == 6);
    fx_key(S, FX_CTRL_BACKSPACE, PKEY_CTRL);
    CHECK(!strcmp(fx_line(), "") && uimn.edline.col == 0);
    fx_key(S, FX_CTRL_BACKSPACE, PKEY_CTRL);
    CHECK(!strcmp(fx_line(), "sub hi"));
    fx_done(S, files);
}

/* Ctrl+Del deletes to where Ctrl+Right goes; at the line's end the next joins it. */
TEST(words_ctrl_del_deletes_on)
{
    opened(S);
    fx_cursor_on(S, "print total", "print");
    fx_key(S, FX_CTRL_DEL, PKEY_CTRL);
    CHECK(!strcmp(fx_line(), "total") && uimn.edline.col == 0);
    fx_key(S, FX_END, 0);
    fx_key(S, FX_CTRL_DEL, PKEY_CTRL);
    CHECK(!strcmp(fx_line(), "totalprint 2"));
    fx_done(S, files);
}

/* A press on the text, clicks of it in a row. */
static void click(
    Session *const S,
    const char *const text,
    const char *const word,
    const uint8_t clicks
)
{
    const uint32_t line = fx_line_of(S, text) - uimn.text.first;
    uimn.mouse = (UiMouse){
        .event = MS_PRESS,
        .row = (uint8_t)(uimn.ed.row0 + line - uimn.ed.top),
        .col = fx_col_at(fx_col_of(text, word)),
        .clicks = clicks,
    };

    uimn_key(S, KEY_MOUSE);
}

/* A double click selects the word, a triple the line; one alone only places the cursor. */
TEST(words_double_click_selects)
{
    opened(S);
    fx_cursor_on(S, "print 2", "print"); /* the SUB's view shown */
    click(S, "print total", "tal", 2);
    CHECK(edit_sel() == SEL_PIECE && uimn.ed.sel_a == 6 && uimn.ed.sel_b == 11);
    click(S, "print total", "tal", 1);
    CHECK(edit_sel() == SEL_NONE && uimn.edline.col == 8);
    click(S, "print total", "tal", 3);
    CHECK(edit_sel() == SEL_LINES && uimn.ed.sel_a == uimn.ed.sel_b);
    CHECK(uimn.text.first + uimn.ed.sel_a == fx_line_of(S, "print total"));
    fx_done(S, files);
}

/* Presses at one cell soon after each other count 1, 2, 3, then 1 again; elsewhere 1. */
TEST(words_presses_are_counted)
{
    host_mouse_on();
    mous_init();
    const uint8_t counts[] = {1, 2, 3, 1};

    for (uint32_t i = 0; i < countof(counts); i++) {
        host_mouse_set(5, 9, 0, 0, 1);
        CHECK(mous_poll() && uimn.mouse.event == MS_PRESS && uimn.mouse.clicks == counts[i]);
        host_mouse_set(5, 9, 0, 0, 0);
        CHECK(mous_poll() && uimn.mouse.event == MS_UP);
    }

    host_mouse_set(5, 10, 0, 0, 1);
    CHECK(mous_poll() && uimn.mouse.clicks == 1);
    host_mouse_set(5, 10, 0, 0, 0);
    mous_poll();
    uimn.mouse.on = false;
}
