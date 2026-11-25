/* Completion in a program of both dialects, and after a module's name in either. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "session.h"
#include "comp/suggest.h"
#include "ui/linemap.h"
#include "ui/uimain.h"
#include "edfix.h"
#include "unit.h"

static const char *const files[] = {
    "main.bas",
    "geo.bas",
    "shapes.bas",
    nullptr,
};

#define DIALECT "' OPTION DIALECT QUICKR\n"

static const char geo[] = DIALECT "public type Spot\n"
                                  "    x as int\n"
                                  "    y as int\n"
                                  "    fn moved(self) as Spot\n"
                                  "        return Spot(self.x + 1, self.y)\n"
                                  "    end fn\n"
                                  "end type\n"
                                  "public enum Colour\n"
                                  "    red\n"
                                  "    green\n"
                                  "end enum\n"
                                  "public const limit = 10\n"
                                  "public var count as int\n"
                                  "public function area(w as int, h as int) as int\n"
                                  "    let total = w * h\n"
                                  "    return total\n"
                                  "end function\n"
                                  "function helper() as int\n"
                                  "    return 1\n"
                                  "end function\n";

static const char shapes[] = "TYPE Rect\n"
                             "    W AS LONG\n"
                             "    H AS LONG\n"
                             "END TYPE\n"
                             "CONST MAXW = 5\n"
                             "FUNCTION Area& (r AS Rect)\n"
                             "    Area& = r.W * r.H\n"
                             "END FUNCTION\n"
                             "SUB Show (a AS LONG)\n"
                             "    PRINT a\n"
                             "END SUB\n"
                             "PRIVATE SUB Hidden\n"
                             "END SUB\n";

/* main.bas, its text main, importing geo.bas, Quickr's, and shapes.bas, QBasic's. */
static void open_with(Session *const S, const char *const main)
{
    fx_open_program(S, "");
    fx_write_file("main.bas", main);
    fx_write_file("geo.bas", geo);
    fx_write_file("shapes.bas", shapes);
    uimn_open(S, fx_path_of("main.bas"));
}

/* What completion offers for text typed on the line spelled at: has it want. */
static bool offers(Session *const S, const char *const at, const char *const text, const char *want)
{
    Suggs out = {};

    const uint16_t n = (uint16_t)strlen(text);
    const uint32_t line = fx_line_of(S, at);
    CHECK(line != ~0u);
    bool found = false;

    if (line != ~0u && sugg_find(S, (const uint8_t *)text, n, n, lmap_off(S, line), &out))
        for (uint16_t i = 0; i < out.n; i++)
            found |= !strcmp((const char *)out.list[i].text, want);

    free(out.list);
    return found;
}

static const char qb_main[] = "IMPORT geo\n"
                              "IMPORT shapes\n"
                              "DIM s AS geo.Spot\n"
                              "DIM r AS shapes.Rect\n"
                              "total = 1\n"
                              "PRINT total\n";

/* In a QBasic program a Quickr module's line was completed as QBasic's: PRINT, not print. */
TEST(mixcomplete_qb_main_completes_quickr_lines_as_quickr)
{
    open_with(S, qb_main);
    CHECK(offers(S, "    return total", "    pri", "print"));
    CHECK(!offers(S, "    return total", "    pri", "PRINT"));
    fx_done(S, files);
}

static const char qr_main[] = DIALECT "import geo\n"
                                      "import shapes\n"
                                      "var s as geo.Spot\n"
                                      "var r as shapes.Rect\n"
                                      "var xs as int[]\n"
                                      "var d as dict[string, int]\n"
                                      "let w = \"hi\"\n"
                                      "let total = 1\n"
                                      "print total\n";

/* In a Quickr program a QBasic module's line was completed as Quickr's: print, not PRINT. */
TEST(mixcomplete_qr_main_completes_qbasic_lines_as_qbasic)
{
    open_with(S, qr_main);
    CHECK(offers(S, "    print a", "    PRI", "PRINT"));
    CHECK(!offers(S, "    print a", "    PRI", "print"));
    fx_done(S, files);
}
