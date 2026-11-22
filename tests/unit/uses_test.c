/* The uses of the name at the cursor: lit on screen, not in strings or comments, by suffix. */
#include "edfix.h"
#include "ui/theme.h"
#include "ui/uimain.h"
#include "unit.h"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

static const char *const sum = "total = n% + 1";
static const char *const shown = "print total, \"total\"; total"; /* the first spelling */

static void opened(Session *const S)
{
    fx_open_program(
        S,
        "sub hi (n%)\r\n"
        "total = n% + 1\r\n"
        "print total, \"total\"; Total\r\n"
        "' total\r\n"
        "n$ = \"x\"\r\n"
        "print n\r\n"
        "end sub\r\n"
    );
    uimn_size(PSCR_80X25);
    them_qbasic();
}

/* Line text's column col drawn in SL_OCC. */
static bool lit_at(Session *const S, const char *const text, const uint16_t col)
{
    fx_shown(S);
    return fx_attr(fx_row_of(fx_line_of(S, text)), fx_col_at(col)) == them_attr(SL_OCC);
}

/* Nothing was lit; the name's uses are, case aside, not in a string or a comment. */
TEST(uses_of_the_name_are_lit)
{
    opened(S);
    fx_cursor_on(S, sum, "total");
    CHECK(them_attr(SL_OCC) != them_attr(SL_ED) && them_attr(SL_OCC) != them_attr(SL_STR));
    CHECK(lit_at(S, shown, 6) && lit_at(S, shown, 10) && lit_at(S, shown, 22));
    CHECK(!lit_at(S, shown, 14) && !lit_at(S, shown, 5) && !lit_at(S, "' total", 2));
    CHECK(lit_at(S, sum, 0) && !lit_at(S, sum, 8));
    fx_done(S, files);
}

/* n% lights n% and n, not n$; just past a name is on it; on a keyword nothing is lit. */
TEST(uses_go_by_suffix)
{
    opened(S);
    fx_cursor_on(S, sum, "n%");
    CHECK(lit_at(S, "sub hi (n%)", 8) && lit_at(S, "print n", 6) && !lit_at(S, "n$ = \"x\"", 0));
    fx_cursor_on(S, "n$ = \"x\"", "n$");
    CHECK(lit_at(S, "print n", 6) && !lit_at(S, sum, 8));
    fx_cursor_on(S, shown, ",");
    CHECK(lit_at(S, sum, 0));
    fx_cursor_on(S, shown, "print");
    CHECK(!lit_at(S, sum, 0) && !lit_at(S, shown, 6));
    fx_done(S, files);
}
