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
