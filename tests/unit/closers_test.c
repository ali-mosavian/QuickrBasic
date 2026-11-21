/* Closing pairs: ( and " typed get their closing one, which typing steps over. */
#include "edfix.h"
#include "ui/uimain.h"
#include "unit.h"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

/* An empty line in the SUB, the cursor on it. */
static void opened(Session *const S)
{
    fx_open_program(S, "sub hi\r\nprint 1\r\nend sub\r\n");
    fx_cursor_on(S, "print 1", "print");
    fx_key(S, FX_HOME, 0);
    fx_key(S, FX_ENTER, 0);
    fx_key(S, FX_UP, 0);
}

static bool line_is(const char *const text, const uint16_t col)
{
    return !strcmp(fx_line(), text) && uimn.edline.col == col;
}

/* ( typed was ( alone; now ( ), the cursor between; the ) typed steps over. */
TEST(closers_bracket)
{
    opened(S);
    fx_type(S, "print (");
    CHECK(line_is("print ()", 7));
    fx_type(S, "1");
    CHECK(line_is("print (1)", 8));
    fx_type(S, ")");
    CHECK(line_is("print (1)", 9));
    fx_type(S, ", \"a\", f(");
    CHECK(line_is("print (1), \"a\", f()", 18)); /* past a string ended */
    fx_done(S, files);
}

/* " gets its closing "; inside the string the " typed ends it, stepping over. */
TEST(closers_quote)
{
    opened(S);
    fx_type(S, "print \"");
    CHECK(line_is("print \"\"", 7));
    fx_type(S, "hi\"");
    CHECK(line_is("print \"hi\"", 10));
    fx_done(S, files);
}

/* A whole line typed is as typed. */
TEST(closers_a_line_typed_is_itself)
{
    opened(S);
    const char *const text = "print \"a(b)\", f(g(1)), \"\"";
    fx_type(S, text);
    CHECK(line_is(text, (uint16_t)strlen(text)));
    fx_done(S, files);
}

/* Nothing added before a word, in a string ((), in a comment ("), or in overtype. */
TEST(closers_not_everywhere)
{
    opened(S);
    fx_type(S, "x");
    fx_key(S, FX_HOME, 0);
    fx_type(S, "(");
    CHECK(line_is("(x", 1));
    fx_key(S, FX_END, 0);
    fx_type(S, " + \"a(");
    CHECK(line_is("(x + \"a(\"", 8));
    fx_key(S, FX_END, 0);
    fx_type(S, " ' \"");
    CHECK(line_is("(x + \"a(\" ' \"", 13));
    fx_done(S, files);
}

/* In a string before a space, ( is alone; a " before a string's is typed, not stepped over. */
TEST(closers_not_in_strings)
{
    opened(S);
    fx_type(S, "print \"a b");
    fx_key(S, FX_LEFT, 0);
    fx_key(S, FX_LEFT, 0);
    fx_type(S, "(");
    CHECK(line_is("print \"a( b\"", 9));
    fx_key(S, FX_HOME, 0);

    for (uint32_t i = 0; i < 6; i++)
        fx_key(S, FX_RIGHT, 0);

    fx_type(S, "\"");
    CHECK(line_is("print \"\"a( b\"", 7));
    fx_done(S, files);
}

/* Overtype adds nothing. */
TEST(closers_not_in_overtype)
{
    opened(S);
    fx_key(S, 0x5200, 0); /* Ins */
    fx_type(S, "f(");
    fx_key(S, 0x5200, 0);
    CHECK(line_is("f(", 2));
    fx_done(S, files);
}

/* Backspace between ( ) or an empty string's quotes takes both out. */
TEST(closers_backspace_takes_the_pair)
{
    opened(S);
    fx_type(S, "print (");
    fx_key(S, FX_BACKSPACE, 0);
    CHECK(line_is("print ", 6));
    fx_type(S, "\"");
    fx_key(S, FX_BACKSPACE, 0);
    CHECK(line_is("print ", 6));
    fx_type(S, "\"a\"");
    fx_key(S, FX_LEFT, 0);
    fx_key(S, FX_BACKSPACE, 0);
    CHECK(line_is("print \"\"", 7)); /* the a; the quotes then an empty string's */
    fx_key(S, FX_END, 0);
    fx_type(S, "\"b");
    fx_key(S, FX_HOME, 0);

    for (uint32_t i = 0; i < 8; i++)
        fx_key(S, FX_RIGHT, 0);

    fx_key(S, FX_BACKSPACE, 0); /* between two strings' quotes: one */
    CHECK(line_is("print \"\"b\"", 7));
    fx_done(S, files);
}
