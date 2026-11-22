/* Lines with syntax errors: kept, lit, told on the status bar, gone to with Ctrl+F8. */
#include "edfix.h"
#include "plat/pkey.h"
#include "ui/errors.h"
#include "ui/theme.h"
#include "ui/uimain.h"
#include "unit.h"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

/* gfx.bas with the line `print 1 +` typed in place of `print 1`, the cursor left on the line
   below. */
static void typed_error(Session *const S)
{
    fx_open_program(S, "sub hi\r\nprint 1\r\nprint 2\r\nend sub\r\n");
    uimn_size(PSCR_80X25);
    them_qbasic();
    fx_cursor_on(S, "print 1", "print");
    fx_key(S, FX_END, 0);
    fx_type(S, " +");
    fx_key(S, FX_DOWN, 0);
}

/* A syntax error held the cursor on its line under a box; the line is kept as typed, the cursor
   goes on. */
TEST(errors_let_the_cursor_go_on)
{
    typed_error(S);
    CHECK(!strcmp(fx_line(), "print 2"));
    uint16_t col = 0;
    CHECK(errs_at(S, fx_line_of(S, "print 1 +"), &col) && col == 9);
    fx_done(S, files);
}

/* The error's word lit; on its line the status bar tells the error, elsewhere how many there are. */
TEST(errors_are_lit_and_told)
{
    typed_error(S);
    const uint32_t line = fx_line_of(S, "print 1 +");
    fx_key(S, FX_UP, 0);
    fx_shown(S);
    CHECK(them_attr(SL_ERR) != them_attr(SL_ED));
    CHECK(fx_attr(fx_row_of(line), fx_col_at(9)) == them_attr(SL_ERR));
    CHECK(fx_attr(fx_row_of(line), fx_col_at(0)) != them_attr(SL_ERR));
    CHECK(strstr(fx_row(status_row()), "PRINT statement is incomplete"));
    fx_key(S, FX_DOWN, 0);
    fx_shown(S);
    CHECK(strstr(fx_row(status_row()), "(1 error)"));
    fx_done(S, files);
}

/* Ctrl+F8: to each line with an error in turn, at its error, past the procedure's own lines;
   back to the first after the last. */
TEST(errors_ctrl_f8_goes_to_each)
{
    typed_error(S);
    fx_cursor_on(S, "gfx.hi", "gfx");
    fx_key(S, FX_END, 0);
    fx_type(S, " )");
    fx_key(S, FX_DOWN, 0);
    fx_key(S, FX_CTRL_F8, PKEY_CTRL);
    CHECK(uimn.text.first + uimn.ed.line == fx_line_of(S, "print 1 +") && uimn.edline.col == 9);
    fx_key(S, FX_CTRL_F8, PKEY_CTRL);
    CHECK(uimn.text.first + uimn.ed.line == fx_line_of(S, "gfx.hi )"));
    fx_key(S, FX_CTRL_F8, PKEY_CTRL);
    CHECK(uimn.text.first + uimn.ed.line == fx_line_of(S, "print 1 +"));
    fx_key(S, FX_ALT_LEFT, PKEY_ALT); /* each a jump */
    CHECK(uimn.text.first + uimn.ed.line == fx_line_of(S, "gfx.hi )"));
    fx_done(S, files);
}

/* Syntax Checking off: the line's error was its check's, so none; on again, back. */
TEST(errors_go_with_syntax_checking)
{
    typed_error(S);
    CHECK(errs_count(S) == 1);
    uimn.text.check = false;
    CHECK(errs_count(S) == 0);
    uimn.text.check = true;
    CHECK(errs_count(S) == 1);
    fx_done(S, files);
}
