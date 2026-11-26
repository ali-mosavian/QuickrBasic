/* OPTION DISABLE: QB's words for devices made names in a module's lines after it (comp/lexer.c's
   lexr_disable, io/repl.c). */
#include <stdio.h>
#include "session.h"
#include "unit.h"

/* src run as a program file is. */
static Status run_text(Session *const S, const char *const src)
{
    return unit_run_bytes(S, src, strlen(src));
}

TEST(disable_device_words_are_names)
{
    CHECK(run_text(S, "line = 1\n") == E_SN);
    CHECK(run_text(S, "line = 1\noption disable line\n") == E_SN); /* the lines after it */
    CHECK(run_text(S, "option disable line, open\nline = 1: open = 2\nprint line; open;\n") == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1  2 ");
    CHECK(run_text(S, "option disable line\ncircle = 1\n") == E_SN); /* not listed */
    CHECK(run_text(S, "option disable print\n? 1\n") == E_SN);       /* ? is no PRINT */
}

/* A FUNCTION named POINT, a Syntax error till OPTION DISABLE lists it. */
TEST(disable_names_the_lines_after_it)
{
    static const char point[] = "function point (x as integer) as integer\n"
                                "    point = x * 2\n"
                                "end function\n"
                                "print point(21);\n";
    char src[256];
    snprintf(src, sizeof src, "option disable point\n%s", point);

    CHECK(run_text(S, point) == E_SN);
    CHECK(run_text(S, src) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 42 ");
}

/* A program that disables PRINT and RUN, then a typed PRINT and RUN: QB's. */
TEST(disable_direct_lines_disable_nothing)
{
    CHECK(run_text(S, "option disable print, run\nx = 1\n") == ST_OK);
    CHECK(unit_typed(S, "print x;") == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1 ");
}

TEST(disable_bad_lists_are_syntax_errors)
{
    CHECK(run_text(S, "option disable if\n") == E_SN); /* the language's own */
    CHECK(run_text(S, "option disable lines\n") == E_SN);
    CHECK(run_text(S, "option disable line circle\n") == E_SN);
    CHECK(run_text(S, "option disable line,\n") == E_SN);
    CHECK(run_text(S, "option disable\n") == E_SN);
    CHECK(run_text(S, "option disable line\noption disable line, circle\ncircle = 1\n") == ST_OK);
}
