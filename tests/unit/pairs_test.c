/* The cursor's bracket's partner and its block's keywords: lit, gone to with Ctrl+]. */
#include "edfix.h"
#include "plat/pkey.h"
#include "ui/theme.h"
#include "ui/uimain.h"
#include "unit.h"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

static void opened(Session *const S)
{
    fx_open_program(
        S,
        "sub hi\r\n"
        "for i = 1 to 3\r\n"
        "for j = 1 to 2\r\n"
        "next j\r\n"
        "if i > 1 then\r\n"
        "print (i * (2 + i))\r\n"
        "else\r\n"
        "print \"(x\"\r\n"
        "print f(\"(\", x)\r\n"
        "end if\r\n"
        "if i then print i\r\n"
        "next i\r\n"
        "end sub\r\n"
    );
    uimn_rows(25);
    them_qbasic();
}

/* Line text's column col drawn in SL_MATCH. */
static bool lit(Session *const S, const char *const text, const uint16_t col)
{
    fx_shown(S);
    return fx_attr(fx_row_of(fx_line_of(S, text)), fx_col_at(col)) == them_attr(SL_MATCH);
}

/* Nothing was lit; the bracket at the cursor or just before it and its partner are, not in a
   string. */
TEST(pairs_light_the_partner_bracket)
{
    opened(S);
    const char *const p = "print (i * (2 + i))";
    fx_cursor_on(S, p, "(");
    CHECK(them_attr(SL_MATCH) != them_attr(SL_ED));
    CHECK(lit(S, p, 6) && lit(S, p, 18) && !lit(S, p, 11) && !lit(S, p, 17));
    fx_cursor_on(S, p, "(2");
    CHECK(lit(S, p, 11) && lit(S, p, 17) && !lit(S, p, 6));
    fx_key(S, FX_END, 0);
    CHECK(lit(S, p, 18) && lit(S, p, 6));
    fx_cursor_on(S, "print \"(x\"", "(");
    CHECK(!lit(S, "print \"(x\"", 7));
    const char *const q = "print f(\"(\", x)";
    fx_cursor_on(S, q, "(");
    CHECK(lit(S, q, 7) && lit(S, q, 14) && !lit(S, q, 9));
    fx_done(S, files);
}

/* On IF, ELSE or END IF, all three are lit; a one-line IF is no block. */
TEST(pairs_light_the_block)
{
    opened(S);
    const char *const words[][2] = {{"if i > 1 then", "if"}, {"else", "else"}, {"end if", "if"}};

    for (uint32_t i = 0; i < countof(words); i++) {
        fx_cursor_on(S, words[i][0], words[i][1]);
        CHECK(
            lit(S, "if i > 1 then", 0) && lit(S, "if i > 1 then", 1) && !lit(S, "if i > 1 then", 3)
        );
        CHECK(lit(S, "else", 0) && lit(S, "end if", 0) && lit(S, "end if", 5));
        CHECK(!lit(S, "for i = 1 to 3", 0));
    }

    fx_cursor_on(S, "if i then print i", "if");
    CHECK(!lit(S, "if i then print i", 0) && !lit(S, "else", 0));
    fx_done(S, files);
}

/* NEXT i lights its FOR, not the one nested in it. */
TEST(pairs_pass_nested_blocks)
{
    opened(S);
    fx_cursor_on(S, "next i", "next");
    CHECK(lit(S, "for i = 1 to 3", 0) && lit(S, "next i", 0));
    CHECK(!lit(S, "for j = 1 to 2", 0) && !lit(S, "next j", 0));
    fx_cursor_on(S, "for j = 1 to 2", "for");
    CHECK(lit(S, "next j", 0) && !lit(S, "next i", 0));
    fx_done(S, files);
}

/* Ctrl+] goes to the next keyword of the block, round; to the partner bracket and back. */
TEST(pairs_ctrl_bracket_goes_round)
{
    opened(S);
    fx_cursor_on(S, "if i > 1 then", "if");
    fx_key(S, FX_CTRL_RBRACKET, PKEY_CTRL);
    CHECK(fx_cursor_at(S, "else", "else"));
    fx_key(S, FX_CTRL_RBRACKET, PKEY_CTRL);
    CHECK(fx_cursor_at(S, "end if", "end"));
    fx_key(S, FX_CTRL_RBRACKET, PKEY_CTRL);
    CHECK(fx_cursor_at(S, "if i > 1 then", "if"));
    const char *const p = "print (i * (2 + i))";
    fx_cursor_on(S, p, "(");
    fx_key(S, FX_CTRL_RBRACKET, PKEY_CTRL);
    CHECK(uimn.edline.col == 18);
    fx_key(S, FX_CTRL_RBRACKET, PKEY_CTRL);
    CHECK(uimn.edline.col == 6);
    fx_done(S, files);
}
