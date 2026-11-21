/* Ctrl+P's Go To box, and Alt+Left and Alt+Right back and forward past the jumps. */
#include "edfix.h"
#include "plat/host/host.h"
#include "plat/pkey.h"
#include "ui/menu.h"
#include "ui/mouse.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "unit.h"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

static void opened(Session *const S)
{
    fx_open_program(
        S,
        "sub drawworld\r\nprint 1\r\nend sub\r\n"
        "sub drawwave\r\nprint 2\r\nend sub\r\n"
        "sub hi\r\nprint 3\r\nend sub\r\n"
    );
    uimn_rows(25);
    fx_cursor_on(S, "gfx.hi", "hi");
}

/* Ctrl+P, then the keys (0 ended) the box takes, a character's its own. */
static void go_to(Session *const S, const uint16_t *const keys)
{
    for (size_t i = 0; keys[i]; i++)
        host_key_push(keys[i]);

    fx_key(S, FX_CTRL_P, PKEY_CTRL);
}

/* Ctrl+P, then a press at screen row row, column col. */
static void press(Session *const S, const uint8_t row, const uint8_t col)
{
    host_mouse_on();
    mous_init();
    host_mouse_set(row, col, 0, 0, 1);
    fx_key(S, FX_CTRL_P, PKEY_CTRL);
    host_mouse_set(row, col, 0, 0, 0);
    mous_poll();
    uimn.mouse.on = false;
}

static const char *view(void) { return view_name(uimn.ed.view); }

static bool on_line(Session *const S, const char *const text)
{
    return uimn.text.first + uimn.ed.line == fx_line_of(S, text);
}

/* Ctrl+P was nothing: it lists the views whose names, a file's folder aside, hold the letters in
   order, case aside. */
TEST(goto_by_letters_in_order)
{
    opened(S);
    go_to(S, (const uint16_t[]){'D', 'R', 'W', FX_ENTER, 0});

    CHECK(!strcmp(view(), "drawworld"));
    go_to(S, (const uint16_t[]){'d', 'w', 'v', FX_ENTER, 0});

    CHECK(!strcmp(view(), "drawwave"));
    go_to(S, (const uint16_t[]){'d', FX_DOWN, FX_DOWN, FX_UP, FX_ENTER, 0});

    CHECK(!strcmp(view(), "drawworld"));
    go_to(S, (const uint16_t[]){'d', FX_DOWN, FX_LEFT, FX_ENTER, 0}); /* the choice kept */
    CHECK(!strcmp(view(), "drawwave"));
    go_to(S, (const uint16_t[]){'d', FX_DOWN, 'o', FX_ENTER, 0}); /* the first again */
    CHECK(!strcmp(view(), "drawworld"));
    go_to(S, (const uint16_t[]){'d', FX_UP, FX_ENTER, 0});

    CHECK(!strcmp(view(), "drawworld"));
    go_to(S, (const uint16_t[]){'g', 'f', 'x', FX_ENTER, 0});

    const char *const file = view_file(S, uimn.ed.view);
    CHECK(!view_at(uimn.ed.view)->kind && !strcmp(file + strlen(file) - 7, "gfx.bas"));
    const uint32_t gfx = uimn.ed.view;
    go_to(S, (const uint16_t[]){'h', 'i', FX_ESC, 0});

    CHECK(uimn.ed.view == gfx);
    go_to(S, (const uint16_t[]){'z', FX_ENTER, 0}); /* none */
    CHECK(uimn.ed.view == gfx);
    fx_done(S, files);
}

/* A press on an entry goes there; one outside the box closes it. */
TEST(goto_by_mouse)
{
    opened(S);
    press(S, 9, 20); /* main.bas, gfx.bas, drawworld, drawwave */
    CHECK(!strcmp(view(), "drawwave"));
    press(S, 1, 0);
    CHECK(!strcmp(view(), "drawwave"));
    fx_done(S, files);
}

/* Alt+Left goes back to where a jump left, Alt+Right forward again; a jump ends the way forward.
   Go to Definition, Find and F3 are jumps too. */
TEST(goto_back_and_forward)
{
    opened(S);
    go_to(S, (const uint16_t[]){'h', 'i', FX_ENTER, 0});

    CHECK(!strcmp(view(), "hi"));
    fx_key(S, FX_ALT_LEFT, PKEY_ALT);
    CHECK(fx_cursor_at(S, "gfx.hi", "hi"));
    fx_key(S, FX_ALT_RIGHT, PKEY_ALT);
    CHECK(!strcmp(view(), "hi"));
    fx_key(S, FX_ALT_LEFT, PKEY_ALT);
    fx_key(S, FX_CTRL_B, PKEY_CTRL);
    CHECK(fx_cursor_at(S, "sub hi", "hi"));
    fx_key(S, FX_ALT_RIGHT, PKEY_ALT); /* nothing forward */
    CHECK(fx_cursor_at(S, "sub hi", "hi"));
    fx_key(S, FX_ALT_LEFT, PKEY_ALT);
    CHECK(fx_cursor_at(S, "gfx.hi", "hi"));

    uimn_act(S, ACT_FIND);
    fx_type(S, "print");
    fx_key(S, FX_ESC, 0);
    CHECK(on_line(S, "print 1"));
    fx_key(S, FX_F3, 0);
    CHECK(on_line(S, "print 2"));
    fx_key(S, FX_ALT_LEFT, PKEY_ALT);
    CHECK(on_line(S, "print 1"));
    fx_key(S, FX_ALT_LEFT, PKEY_ALT);
    CHECK(fx_cursor_at(S, "gfx.hi", "hi"));
    fx_done(S, files);
}

/* F2's list showing a procedure is a jump too. */
TEST(goto_subs_box_is_a_jump)
{
    opened(S);
    const uint16_t keys[] = {FX_DOWN, FX_DOWN, FX_DOWN, FX_DOWN, FX_ENTER}; /* main, gfx, by name */

    for (size_t i = 0; i < countof(keys); i++)
        host_key_push(keys[i]);

    uimn_act(S, ACT_SUBS);
    CHECK(!strcmp(view(), "hi"));
    fx_key(S, FX_ALT_LEFT, PKEY_ALT);
    CHECK(fx_cursor_at(S, "gfx.hi", "hi"));
    fx_done(S, files);
}

/* A place moves with the lines added before it. */
TEST(goto_places_move_with_the_lines)
{
    opened(S);
    go_to(S, (const uint16_t[]){'h', 'i', FX_ENTER, 0});

    CHECK(fx_cursor_at(S, "sub hi", "sub"));
    go_to(S, (const uint16_t[]){'m', 'a', 'i', 'n', FX_ENTER, 0});

    fx_cursor_on(S, "import gfx", "gfx");
    fx_key(S, FX_END, 0);
    fx_key(S, FX_ENTER, 0); /* a line before gfx.hi: main's lines are the text's first */
    fx_key(S, FX_ALT_LEFT, PKEY_ALT);
    CHECK(!strcmp(view(), "hi") && fx_cursor_at(S, "sub hi", "sub"));
    fx_key(S, FX_ALT_LEFT, PKEY_ALT);
    CHECK(fx_cursor_at(S, "gfx.hi", "hi"));
    fx_done(S, files);
}
