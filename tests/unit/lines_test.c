/* Whole lines moved, copied and commented out by keys. */
#include "edfix.h"
#include "plat/pkey.h"
#include "ui/errors.h"
#include "ui/linemap.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "unit.h"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

static void opened(Session *const S)
{
    fx_open_program(S, "sub hi\r\nprint 1\r\nprint 2\r\nprint 3\r\nend sub\r\n");
}

/* Lines a, b and c in that order, one after another. */
static bool in_order(
    Session *const S,
    const char *const a,
    const char *const b,
    const char *const c
)
{
    const uint32_t x = fx_line_of(S, a);
    return x != ~0u && fx_line_of(S, b) == x + 1 && fx_line_of(S, c) == x + 2;
}

/* Text line line (the whole text's), 0-ended. */
static const char *text_at(Session *const S, const uint32_t line)
{
    static LineBuf b = {.grows = true};

    const uint32_t n = text_record(S, lmap_off(S, line), &b);
    b.text[n] = 0;
    return (const char *)b.text;
}

static void key(Session *const S, const uint16_t k, const uint8_t flags) { fx_key(S, k, flags); }

/* Alt+Down was nothing: it swaps the line with the one below, the cursor with it; Alt+Up back. */
TEST(lines_alt_down_moves_the_line)
{
    opened(S);
    fx_cursor_on(S, "print 1", "1");
    key(S, FX_ALT_DOWN, PKEY_ALT);
    CHECK(in_order(S, "print 2", "print 1", "print 3") && fx_cursor_at(S, "print 1", "1"));
    key(S, FX_ALT_UP, PKEY_ALT);
    CHECK(in_order(S, "print 1", "print 2", "print 3") && fx_cursor_at(S, "print 1", "1"));
    fx_done(S, files);
}

/* The lines selected move together, the selection with them. */
TEST(lines_move_the_selection)
{
    opened(S);
    fx_cursor_on(S, "print 1", "print");
    key(S, FX_DOWN, PKEY_SHIFT);
    key(S, FX_DOWN, PKEY_SHIFT); /* print 1 and print 2; the cursor at print 3's start */
    key(S, FX_ALT_UP, PKEY_ALT);
    CHECK(fx_line_of(S, "print 1") == fx_line_of(S, "sub hi") + 1); /* none past the SUB line */
    key(S, FX_ALT_DOWN, PKEY_ALT);
    CHECK(in_order(S, "print 3", "print 1", "print 2"));
    CHECK(uimn.ed.sel_on && uimn.text.first + uimn.ed.sel_line == fx_line_of(S, "print 1"));
    CHECK(uimn.text.first + uimn.ed.line == fx_line_of(S, "print 2") + 1);
    fx_done(S, files);
}

/* No line goes out of its SUB, nor END SUB in. */
TEST(lines_stay_in_their_sub)
{
    opened(S);
    fx_cursor_on(S, "print 3", "print");
    key(S, FX_ALT_DOWN, PKEY_ALT);
    CHECK(fx_line_of(S, "end sub") == fx_line_of(S, "print 3") + 1);
    fx_cursor_on(S, "end sub", "end");
    key(S, FX_ALT_UP, PKEY_ALT);
    CHECK(fx_line_of(S, "end sub") == fx_line_of(S, "print 3") + 1);
    fx_cursor_on(S, "print 1", "print");
    key(S, FX_ALT_DOWN, PKEY_ALT | PKEY_SHIFT);
    key(S, FX_ALT_UP, PKEY_ALT);
    CHECK(fx_line_of(S, "sub hi") + 1 == fx_line_of(S, "print 1"));
    fx_done(S, files);
}

/* Shift+Alt+Down copies the line below, the cursor on the copy; Shift+Alt+Up above, the cursor
   on the upper. */
TEST(lines_copy_above_and_below)
{
    opened(S);
    fx_cursor_on(S, "print 2", "2");
    const uint32_t line = fx_line_of(S, "print 2");
    key(S, FX_ALT_DOWN, PKEY_ALT | PKEY_SHIFT);
    CHECK(!strcmp(text_at(S, line), "print 2") && !strcmp(text_at(S, line + 1), "print 2"));
    CHECK(fx_line_of(S, "print 3") == line + 2);
    CHECK(uimn.text.first + uimn.ed.line == line + 1 && !strcmp(fx_line(), "print 2"));
    fx_cursor_on(S, "print 3", "3");
    key(S, FX_ALT_UP, PKEY_ALT | PKEY_SHIFT);
    CHECK(fx_line_of(S, "print 3") == line + 2 && uimn.text.first + uimn.ed.line == line + 2);
    CHECK(fx_line_of(S, "end sub") == line + 4);
    fx_done(S, files);
}

/* A move is one undo step; a breakpoint goes with its line, the moved one's and the one it
   passed. */
TEST(lines_move_undoes_and_keeps_breakpoints)
{
    opened(S);
    fx_cursor_on(S, "print 2", "print");
    text_toggle(S, uimn.ed.line);
    fx_cursor_on(S, "print 1", "print");
    key(S, FX_ALT_DOWN, PKEY_ALT);
    CHECK(text_mark(S, fx_line_of(S, "print 2") - uimn.text.first));
    CHECK(!text_mark(S, fx_line_of(S, "print 1") - uimn.text.first));
    text_toggle(S, fx_line_of(S, "print 1") - uimn.text.first);
    key(S, FX_ALT_UP, PKEY_ALT);
    CHECK(text_mark(S, fx_line_of(S, "print 1") - uimn.text.first));
    fx_key(S, FX_CTRL_Z, PKEY_CTRL);
    CHECK(in_order(S, "print 2", "print 1", "print 3"));
    fx_done(S, files);
}

/* Ctrl+- comments the lines out, or back when each is; a mix is commented. */
TEST(lines_comment_out_and_back)
{
    opened(S);
    fx_cursor_on(S, "print 3", "3");
    key(S, FX_CTRL_MINUS, PKEY_CTRL);
    CHECK(fx_line_of(S, "' print 3") != ~0u && fx_cursor_at(S, "' print 3", "3"));
    fx_cursor_on(S, "print 1", "print");
    key(S, FX_DOWN, PKEY_SHIFT);
    key(S, FX_DOWN, PKEY_SHIFT);
    key(S, FX_DOWN, PKEY_SHIFT);
    key(S, FX_CTRL_MINUS, PKEY_CTRL);
    CHECK(in_order(S, "' print 1", "' print 2", "' ' print 3"));
    fx_cursor_on(S, "' print 1", "'");
    key(S, FX_CTRL_MINUS, PKEY_CTRL);
    CHECK(in_order(S, "print 1", "' print 2", "' ' print 3"));
    fx_done(S, files);
}

/* A line with a syntax error moves as typed, its error kept. */
TEST(lines_move_a_line_with_an_error)
{
    opened(S);
    fx_cursor_on(S, "print 1", "print");
    fx_key(S, FX_END, 0);
    fx_type(S, " +");
    fx_key(S, FX_DOWN, 0);
    key(S, FX_ALT_UP, PKEY_ALT); /* print 2 up: the line with the error carried down */
    uint16_t col;
    CHECK(in_order(S, "print 2", "print 1 +", "print 3"));
    CHECK(errs_at(S, fx_line_of(S, "print 1 +"), &col) && col == 9);
    fx_cursor_on(S, "print 1 +", "print");
    key(S, FX_ALT_UP, PKEY_ALT | PKEY_SHIFT);
    CHECK(errs_at(S, fx_line_of(S, "print 1 +") + 1, &col) && col == 9);
    fx_done(S, files);
}
