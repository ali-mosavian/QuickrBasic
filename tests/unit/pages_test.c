/* SCREEN's active and visible pages and PCOPY in every mode: drawing on a hidden page leaves the screen
   shown as it was (beyond QB 4.5, which had one page in modes 1, 2, 12 and 13, and no PCOPY here). */
#include <stdio.h>

#include "session.h"
#include "unit.h"

unsigned host_width(void);
const uint8_t *host_pixels(void);
void headless_set_stdout_is_screen(int yes);
void host_text_row(unsigned row, char out[81]);

static const int graphics[] = {1, 2, 7, 8, 9, 12, 13};

/* SCREEN mode, , 1, 0, a dot on page 1, then tail; the pixel shown at the dot. */
static unsigned dot_shown(Session *const S, const int mode, const char *const tail)
{
    char line[120];
    snprintf(
        line,
        sizeof line,
        "10 SCREEN %d, , 1, 0: PSET (5, 5), 1: PRINT POINT(5, 5);: %s",
        mode,
        tail
    );
    const char *const lines[] = {line, nullptr};

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1 "); /* POINT reads the active page */
    return host_pixels()[5 * host_width() + 5];
}

TEST(pages_hidden_in_every_mode)
{
    for (size_t i = 0; i < sizeof graphics / sizeof graphics[0]; i++) {
        CHECK(dot_shown(S, graphics[i], "REM") == 0);
        CHECK(dot_shown(S, graphics[i], "SCREEN , , , 1") == 1);
        CHECK(dot_shown(S, graphics[i], "PCOPY 1, 0") == 1);
        CHECK(dot_shown(S, graphics[i], "PCOPY 0, 1: SCREEN , , 0, 1") == 0);
    }
}

/* Every mode has 2 pages at least; QB 4.5's counts where it has more; a page past them is Illegal function
   call. */
TEST(pages_out_of_range)
{
    static const struct {
        int mode, pages;
    } counts[] = {{1, 2}, {2, 2}, {7, 8}, {8, 4}, {9, 2}, {12, 2}, {13, 2}};

    for (size_t i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        char ok[80], active[80], visible[80], copy[80];
        snprintf(
            ok,
            sizeof ok,
            "10 SCREEN %d, , %d, %d",
            counts[i].mode,
            counts[i].pages - 1,
            counts[i].pages - 1
        );
        snprintf(active, sizeof active, "10 SCREEN %d, , %d, 0", counts[i].mode, counts[i].pages);
        snprintf(visible, sizeof visible, "10 SCREEN %d, , 0, %d", counts[i].mode, counts[i].pages);
        snprintf(copy, sizeof copy, "10 SCREEN %d: PCOPY 0, %d", counts[i].mode, counts[i].pages);
        const char *const lines[][2] =
            {{ok, nullptr}, {active, nullptr}, {visible, nullptr}, {copy, nullptr}};

        CHECK(unit_run(S, lines[0]) == ST_OK);
        CHECK(unit_run(S, lines[1]) == E_USR);
        CHECK(unit_run(S, lines[2]) == E_USR);
        CHECK(unit_run(S, lines[3]) == E_USR);
    }
}

static bool row0(const char *const text)
{
    char got[81];
    host_text_row(0, got);
    return !strncmp(got, text, strlen(text));
}

static Status text_run(Session *const S, const char *const tail)
{
    char line[120];
    snprintf(
        line,
        sizeof line,
        "10 SCREEN 0, , 0, 0: CLS: SCREEN , , 1, 0: CLS: PRINT \"hid\";: %s",
        tail
    );
    const char *const lines[] = {line, nullptr};

    headless_set_stdout_is_screen(1);
    const Status st = unit_run_shown(S, lines);
    headless_set_stdout_is_screen(0);
    return st;
}

TEST(text_pages)
{
    CHECK(text_run(S, "REM") == ST_OK && !row0("hid"));
    CHECK(text_run(S, "SCREEN , , , 1") == ST_OK && row0("hid"));
    CHECK(text_run(S, "PCOPY 1, 0") == ST_OK && row0("hid"));
    CHECK(text_run(S, "SCREEN , , 7, 7") == ST_OK);
    CHECK(text_run(S, "SCREEN , , 8") == E_USR);
    CHECK(text_run(S, "PCOPY 8, 0") == E_USR);
}
