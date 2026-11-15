/* The text screen's statements on the runtime's console (comp/gfxstmt.c, vm/gfx.c), standard
   output the screen: where PRINT lands. */
#include "session.h"
#include "unit.h"

/* llrm's headless device (dev/headless/headless.h, dev/host/video.h). */
void headless_set_stdout_is_screen(int yes);
void host_text_row(unsigned row, char out[81]);

/* Line row of the text screen starts with text at column col (both from 0). */
static bool shows(const unsigned row, const unsigned col, const char *const text)
{
    char got[81];
    host_text_row(row, got);
    return !strncmp(got + col, text, strlen(text));
}

static Status shown(Session *const S, const char *const *const lines)
{
    headless_set_stdout_is_screen(1);
    const Status st = unit_run_shown(S, lines);
    headless_set_stdout_is_screen(0);
    return st;
}

/* POS from 1, as CSRLIN: it was the column from 0. */
TEST(locate_moves_print)
{
    static const char *const lines[] = {
        "10 SCREEN 0: CLS: LOCATE 5, 10: PRINT \"hi\";",
        "20 PRINT POS(0); CSRLIN",
        nullptr,
    };

    CHECK(shown(S, lines) == ST_OK);
    CHECK(shows(4, 9, "hi 12  5"));
}

TEST(cls_clears_and_homes)
{
    static const char *const lines[] = {"10 PRINT \"junk\": CLS: PRINT \"top\"", nullptr};

    CHECK(shown(S, lines) == ST_OK);
    CHECK(shows(0, 0, "top ") && !shows(1, 0, "junk"));
}

TEST(print_in_a_graphics_mode)
{
    static const char *const lines[] = {
        "10 SCREEN 13: LOCATE 2, 3: PRINT \"g\";: PRINT CSRLIN",
        nullptr
    };

    CHECK(shown(S, lines) == ST_OK);
}

TEST(locate_out_of_range)
{
    static const char *const lines[] = {"10 SCREEN 0: LOCATE 30", nullptr};

    CHECK(shown(S, lines) == E_USR);
}

/* SCREEN(r, c[, f]) was a Syntax error. QBasic 1.1's: the character there, f nonzero its
   attribute; a graphics mode's character recognised from its pixels, attribute 0, none a space;
   past the screen, Illegal function call. */
TEST(screen_function_reads_a_cell)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 90: SCREEN 0: CLS: COLOR 14, 1: LOCATE 1, 1: PRINT \"QB\";: COLOR 7, 0",
        "20 a = SCREEN(1, 1): b = SCREEN(1, 2): c = SCREEN(1, 1, 1): d = SCREEN(2, 1)",
        "30 x = SCREEN(0, 1): x = SCREEN(26, 1): x = SCREEN(1, 81): x = SCREEN(1, 0): x = "
        "SCREEN(25, 80)",
        "40 SCREEN 13: LOCATE 1, 1: PRINT \"H\";: COLOR 4: PRINT \"j\";",
        "50 g = SCREEN(1, 1): h = SCREEN(1, 2): i = SCREEN(1, 2, 1): j = SCREEN(3, 3)",
        "60 SCREEN 0: LOCATE 10, 1: PRINT a; b; c; d; e$; g; h; i; j;",
        "70 END",
        "90 e$ = e$ + STR$(ERR): RESUME NEXT",
        nullptr,
    };

    CHECK(shown(S, lines) == ST_OK);
    CHECK(shows(9, 0, " 81  66  30  32  5 5 5 5 72  106  0  32 "));
}
