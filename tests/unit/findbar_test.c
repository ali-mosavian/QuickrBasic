/* The find bar: the program searched as Find What is typed, its matches lit, round every view. */
#include "edfix.h"
#include "plat/pkey.h"
#include "ui/menu.h"
#include "ui/theme.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "unit.h"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

/* total in two SUBs, three times; the bar open with the cursor on main's gfx.hi. */
static void opened(Session *const S)
{
    fx_open_program(
        S,
        "sub hi\r\ntotal = 1\r\nprint total\r\nend sub\r\nsub lo\r\nprint total + 1\r\nend sub\r\n"
    );
    uimn_rows(25);
    them_qbasic();
    fx_cursor_on(S, "gfx.hi", "gfx");
    uimn_act(S, ACT_FIND);
}

/* word on line text selected, the cursor at its end. */
static bool selected(Session *const S, const char *const text, const char *const word)
{
    const uint16_t col = fx_col_of(text, word);
    return uimn.text.first + uimn.ed.line == fx_line_of(S, text) && uimn.ed.sel_on &&
           uimn.ed.sel_line == uimn.ed.line && uimn.ed.sel_col == col &&
           uimn.edline.col == col + strlen(word);
}

static const char *bar(Session *const S)
{
    fx_shown(S);
    return fx_row(status_row());
}

/* Find was a box: OK, then the first match. Each key typed now selects the first match, in the
   SUB it is in; Enter goes on through the other SUB and round. */
TEST(find_bar_finds_as_typed)
{
    opened(S);
    CHECK(selected(S, "gfx.hi", "gfx.hi")); /* the word at the cursor, its own match */
    fx_type(S, "tot");
    CHECK(selected(S, "total = 1", "tot") && !strcmp(view_name(uimn.ed.view), "hi"));
    fx_type(S, "al");
    CHECK(strstr(bar(S), "Find: total") && strstr(bar(S), "1 of 3"));
    fx_key(S, FX_ENTER, 0);
    CHECK(selected(S, "print total", "total") && strstr(bar(S), "2 of 3"));
    fx_key(S, FX_ENTER, 0);
    CHECK(selected(S, "print total + 1", "total") && !strcmp(view_name(uimn.ed.view), "lo"));
    fx_key(S, FX_ENTER, 0);
    CHECK(selected(S, "total = 1", "total") && strstr(bar(S), "1 of 3"));
    fx_done(S, files);
}

/* Shift+Enter and Shift+F3 go back, round to the last. */
TEST(find_bar_goes_back)
{
    opened(S);
    fx_type(S, "total");
    fx_key(S, FX_ENTER, PKEY_SHIFT);
    CHECK(selected(S, "print total + 1", "total"));
    fx_key(S, FX_SHIFT_F3, PKEY_SHIFT);
    CHECK(selected(S, "print total", "total"));
    fx_done(S, files);
}

/* The other matches on screen are lit, the one at the cursor selected. */
TEST(find_bar_lights_the_matches)
{
    opened(S);
    fx_type(S, "total");
    fx_shown(S);
    const uint8_t row = fx_row_of(fx_line_of(S, "print total"));
    CHECK(them_attr(SL_FIND) != them_attr(SL_ED));
    CHECK(fx_attr(row, fx_col_at(6)) == them_attr(SL_FIND));
    CHECK(fx_attr(row, fx_col_at(5)) != them_attr(SL_FIND));
    CHECK(fx_attr(fx_row_of(fx_line_of(S, "total = 1")), fx_col_at(0)) == them_attr(SL_SEL));
    fx_key(S, FX_ESC, 0);
    fx_shown(S);
    CHECK(fx_attr(row, fx_col_at(6)) != them_attr(SL_FIND)); /* the bar gone, unlit */
    fx_done(S, files);
}

/* Nothing matches: the cursor back where the search began, the bar says so. */
TEST(find_bar_no_match_goes_back)
{
    opened(S);
    fx_type(S, "tot");
    fx_type(S, "zz");
    CHECK(fx_cursor_at(S, "gfx.hi", "gfx") && !uimn.ed.sel_on);
    CHECK(strstr(bar(S), "No match"));
    fx_done(S, files);
}

/* Alt+C: TOTAL is no match with Match Case; Alt+W: tot none with Whole Word. */
TEST(find_bar_options)
{
    opened(S);
    fx_type(S, "TOTAL");
    CHECK(selected(S, "total = 1", "total"));
    fx_key(S, FX_ALT_C, PKEY_ALT);
    CHECK(uimn.match_case && strstr(bar(S), "No match"));
    fx_key(S, FX_ALT_C, PKEY_ALT);

    for (uint32_t i = 0; i < 2; i++)
        fx_key(S, FX_BACKSPACE, 0);

    fx_key(S, FX_ALT_W, PKEY_ALT);
    CHECK(uimn.whole_word && strstr(bar(S), "No match"));
    fx_key(S, FX_ALT_W, PKEY_ALT);
    CHECK(!uimn.whole_word && selected(S, "total = 1", "tot"));
    fx_done(S, files);
}

/* Esc leaves the match selected; a key not the bar's closes it and acts. */
TEST(find_bar_closes)
{
    opened(S);
    fx_type(S, "total");
    fx_key(S, FX_ESC, 0);
    CHECK(selected(S, "total = 1", "total") && !strstr(bar(S), "Find:"));
    uimn_act(S, ACT_FIND);
    fx_key(S, FX_DOWN, 0);
    CHECK(!strstr(bar(S), "Find:") && !strcmp(fx_line(), "print total"));
    fx_done(S, files);
}

/* F3 with the bar closed: on through every view, round; Shift+F3 back. */
TEST(find_f3_goes_round_the_views)
{
    opened(S);
    fx_type(S, "total");
    fx_key(S, FX_ESC, 0);
    fx_key(S, FX_F3, 0);
    CHECK(selected(S, "print total", "total"));
    fx_key(S, FX_F3, 0);
    CHECK(selected(S, "print total + 1", "total") && !strcmp(view_name(uimn.ed.view), "lo"));
    fx_key(S, FX_F3, 0);
    CHECK(selected(S, "total = 1", "total"));
    fx_key(S, FX_SHIFT_F3, PKEY_SHIFT);
    CHECK(selected(S, "print total + 1", "total"));
    fx_done(S, files);
}

/* A match typed with the bar closed is counted when it opens again on the same match. */
TEST(find_bar_counts_after_a_change)
{
    opened(S);
    fx_type(S, "total");
    CHECK(strstr(bar(S), "1 of 3"));
    fx_key(S, FX_ESC, 0);
    fx_cursor_on(S, "print total", "total");
    fx_key(S, FX_END, 0);
    fx_type(S, " + total");
    fx_key(S, FX_UP, 0);
    fx_cursor_on(S, "total = 1", "total");
    uimn_act(S, ACT_FIND);
    CHECK(selected(S, "total = 1", "total") && strstr(bar(S), "1 of 4"));
    fx_done(S, files);
}
