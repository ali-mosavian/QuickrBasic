/* The editor on a program of both dialects: a QBasic main importing a Quickr module, and a Quickr
   main importing a QBasic one. */
#include <stdio.h>
#include <stdlib.h>

#include "plat/host/host.h"
#include "plat/pkey.h"
#include "session.h"
#include "mem/module.h"
#include "qr/qr.h"
#include "ui/debuglocals.h"
#include "ui/debugpane.h"
#include "ui/debugstack.h"
#include "ui/debugui.h"
#include "ui/debugwatch.h"
#include "ui/edit.h"
#include "ui/errors.h"
#include "ui/help.h"
#include "ui/highlight.h"
#include "ui/linemap.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "ui/window.h"
#include "edfix.h"
#include "unit.h"

void headless_set_stdout_is_screen(int yes);

static const char *const files[] = {
    "main.bas",
    "gfx.bas",
    "geo.bas",
    "util.bas",
    "shapes.bas",
    "counts.bas",
    nullptr,
};

#define DIALECT "' OPTION DIALECT QUICKR\n"

static const char geo[] = DIALECT "type Spot\n"
                                  "    x as int\n"
                                  "    y as int\n"
                                  "end type\n"
                                  "function area(w as int, h as int) as int\n"
                                  "    let total = w * h\n"
                                  "    return total\n"
                                  "end function\n"
                                  "function quot(a as int, b as int) as int\n"
                                  "    return a div b\n"
                                  "end function\n";

/* main.bas, QBasic's, importing geo.bas, Quickr's: geo's text gtext, main's body body. */
static void open_qb_with(Session *const S, const char *const gtext, const char *const body)
{
    char text[1024];
    snprintf(text, sizeof text, "IMPORT geo\n%s", body);
    fx_open_program(S, "");
    fx_write_file("main.bas", text);
    fx_write_file("geo.bas", gtext);
    uimn_open(S, fx_path_of("main.bas"));
}

static void open_qb(Session *const S, const char *const body) { open_qb_with(S, geo, body); }

static const char shapes[] = "TYPE Rect\n"
                             "    W AS LONG\n"
                             "    H AS LONG\n"
                             "END TYPE\n"
                             "FUNCTION Area& (r AS Rect)\n"
                             "    a& = r.W * r.H\n"
                             "    Area& = a&\n"
                             "END FUNCTION\n"
                             "FUNCTION Quot& (a AS LONG, b AS LONG)\n"
                             "    Quot& = a \\ b\n"
                             "END FUNCTION\n";

/* main.bas, Quickr's, importing shapes.bas, QBasic's: shapes' text stext, main's body body. */
static void open_qr_with(Session *const S, const char *const stext, const char *const body)
{
    char text[1024];
    snprintf(text, sizeof text, DIALECT "import shapes\n%s", body);
    fx_open_program(S, "");
    fx_write_file("main.bas", text);
    fx_write_file("shapes.bas", stext);
    uimn_open(S, fx_path_of("main.bas"));
}

static void open_qr(Session *const S, const char *const body) { open_qr_with(S, shapes, body); }

/* The run keys. */
enum : uint16_t {
    K_F5 = 0x3F00,
    K_F8 = 0x4200,
    K_F10 = 0x4400,
    K_SHIFT_F8 = 0x5B00,
    K_CTRL_F7 = 0x6400,
};

/* key, a run's, pressed: its output, 0-ended. */
static const char *pressed(Session *const S, const uint16_t key)
{
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    host_key_push(FX_ENTER);
    headless_set_stdout_is_screen(1);
    fx_key(S, key, 0);
    headless_set_stdout_is_screen(0);
    pkey_flush();
    S->cons.capture = 0;
    S->cons.sbuf[S->cons.sblen] = 0;
    return (const char *)S->cons.sbuf;
}

/* Run, Restart. */
static void restart(Session *const S)
{
    host_key_push(FX_ENTER);
    headless_set_stdout_is_screen(1);
    udbg_restart(S);
    headless_set_stdout_is_screen(0);
    pkey_flush();
}

/* text typed in the immediate window and run; its output, 0-ended. */
static const char *immediate(Session *const S, const char *const text)
{
    dpan_close();
    wind_focus(S, W_IMM);
    fx_type(S, text);
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    host_key_push(FX_ENTER);
    headless_set_stdout_is_screen(1);
    fx_key(S, FX_ENTER, 0);
    headless_set_stdout_is_screen(0);
    pkey_flush();
    S->cons.capture = 0;
    S->cons.sbuf[S->cons.sblen] = 0;
    wind_focus(S, W_ED1);
    return (const char *)S->cons.sbuf;
}

static uint32_t cursor_line(void) { return uimn.text.first + uimn.ed.line; }

/* Stopped at the line spelled text? */
static bool stopped_at(Session *const S, const char *const text)
{
    return uimn.stopped && cursor_line() == fx_line_of(S, text);
}

/* A breakpoint on the line spelled text. */
static void break_at(Session *const S, const char *const text)
{
    edit_load(S, view_goline(S, fx_line_of(S, text)));
    udbg_bp(S);
}

/* The slot of column col of the line spelled text, as the program window colours it. */
static uint8_t slot_at(Session *const S, const char *const text, const uint32_t col)
{
    static uint8_t cols[SYNT_KEEP];
    const uint32_t line = fx_line_of(S, text);
    CHECK(line != ~0u);

    if (line == ~0u)
        return 0;

    synt_cols(S, text_dialect_at(S, line), (const uint8_t *)text, (uint32_t)strlen(text), cols);
    return cols[col];
}

/* Is a Locals row of frame f want? */
static bool local_is(Session *const S, const uint8_t f, const char *const want)
{
    dloc_read(S, f);

    for (uint8_t i = 0; i < dloc_count(); i++)
        if (!strcmp(dloc_row(i), want))
            return true;

    return false;
}

/* Does call stack row i start with want? */
static bool frame_is(Session *const S, const uint8_t i, const char *const want)
{
    dstk_read(S);
    return i < dstk_rows() && !strncmp(dstk_row(i), want, strlen(want));
}

/* The watch on expr's row, the watch gone after. */
static const char *watched(Session *const S, const char *const expr)
{
    static char row[128];
    CHECK(dwat_add((const uint8_t *)expr, (uint32_t)strlen(expr)));
    dwat_eval(S);
    snprintf(row, sizeof row, "%s", dwat_row((uint8_t)(dwat_count() - 1)));
    dwat_del(S, (uint8_t)(dwat_count() - 1));
    return row;
}

/* Does a screen row hold want? */
static bool screen_has(Session *const S, const char *const want)
{
    fx_shown(S);

    for (uint8_t r = 0; r < 25; r++)
        if (strstr(fx_row(r), want))
            return true;

    return false;
}

/* ---- a QBasic main, a Quickr module */

/* geo's lines were coloured by QBasic's lexer: function's name a variable's, w a name. */
TEST(mixide_qb_main_colours_each_file_its_own)
{
    open_qb(S, "x& = geo.area(2, 3)\nprint x&\n");
    CHECK(slot_at(S, "function area(w as int, h as int) as int", 0) == SL_KW);
    CHECK(slot_at(S, "function area(w as int, h as int) as int", 9) == SL_FNM);
    CHECK(slot_at(S, "function area(w as int, h as int) as int", 14) == SL_PAR);
    CHECK(slot_at(S, "print x&", 0) == SL_KW);
    fx_done(S, files);
}

/* A Quickr line typed in geo was stored as QBasic's tokens: geo lost it, the run failed. */
TEST(mixide_qb_main_keeps_a_quickr_line_as_typed)
{
    open_qb(S, "print geo.area(2, 3)\n");
    fx_retype(S, fx_line_of(S, "    let total = w * h"), "    let total = w * h + 1");
    const uint32_t line = fx_line_of(S, "    let total = w * h + 1");
    CHECK(line != ~0u && prog_line(&S->prog, lmap_off(S, line))->flags & LF_TEXT);
    CHECK(strstr(pressed(S, K_F5), " 7") != nullptr);
    CHECK(uimn_save(S));
    CHECK(strstr(fx_file_text("geo.bas"), "    let total = w * h + 1\r\n") != nullptr);
    fx_done(S, files);
}

/* A line was checked by the program's dialect, not its file's: a Quickr f-string a QBasic syntax
   error, a QBasic suffix in Quickr none. */
TEST(mixide_qb_main_checks_a_line_by_its_file)
{
    open_qb(S, "print geo.area(2, 3)\n");
    const uint32_t line = fx_line_of(S, "    let total = w * h");
    CHECK(fx_typed_over(S, line, "    let s = f\"{w}\""));
    uint16_t col = 0;
    CHECK(!fx_typed_over(S, line, "    total$ = \"a\""));
    CHECK(errs_at(S, line, &col) && col == 4 && strstr(errs_message(S), "a value alone"));
    CHECK(!fx_typed_over(S, fx_line_of(S, "print geo.area(2, 3)"), "x = f\"{w}\""));
    fx_done(S, files);
}

/* Stepping into a Quickr function, Locals spelled its names QBasic's (w&), the call stack named
   no function, a watch read QBasic's total& (0), the immediate window ran QBasic. */
TEST(mixide_qb_main_debugs_quickr_code)
{
    open_qb(S, "x& = geo.area(2, 3)\nprint x&\nprint 1\n");
    break_at(S, "x& = geo.area(2, 3)");
    pressed(S, K_F5);
    CHECK(stopped_at(S, "x& = geo.area(2, 3)"));
    pressed(S, K_F8);
    CHECK(stopped_at(S, "    let total = w * h"));
    pressed(S, K_F8);
    CHECK(stopped_at(S, "    return total"));
    CHECK(local_is(S, 0, "w = 2") && local_is(S, 0, "total = 6"));
    CHECK(frame_is(S, 0, "area (w = 2, h = 3)"));
    CHECK(frame_is(S, 1, "module"));
    CHECK(!strcmp(watched(S, "total + w"), "total + w = 8"));
    CHECK(strstr(immediate(S, "total * 10"), "60") != nullptr);
    pressed(S, K_SHIFT_F8);
    CHECK(stopped_at(S, "print x&"));
    pressed(S, K_F10);
    CHECK(stopped_at(S, "print 1"));
    fx_done(S, files);
}

/* A breakpoint and run to the cursor in geo; the cursor on a name, its tip: Quickr's spelling. */
TEST(mixide_qb_main_breaks_in_quickr_code)
{
    open_qb(S, "print geo.area(2, 3)\nprint geo.area(4, 5)\n");
    break_at(S, "    return total");
    pressed(S, K_F5);
    CHECK(stopped_at(S, "    return total"));
    edit_load(S, view_goline(S, fx_line_of(S, "    return total")));
    uimn.edline.col = 12;
    uimn.tip_mouse = false;
    CHECK(screen_has(S, " total = 6 "));
    edit_load(S, view_goline(S, fx_line_of(S, "    return total")));
    udbg_bp(S);
    edit_load(S, view_goline(S, fx_line_of(S, "    let total = w * h")));
    pressed(S, K_CTRL_F7);
    CHECK(stopped_at(S, "    let total = w * h") && local_is(S, 0, "w = 4"));
    restart(S);
    CHECK(uimn.stopped);
    pressed(S, K_F5);
    CHECK(!uimn.stopped);
    fx_done(S, files);
}

/* A run-time error in geo: the cursor at its line, QBasic's message. */
TEST(mixide_qb_main_runtime_error_in_quickr_code)
{
    open_qb(S, "print geo.quot(1, 0)\n");
    pressed(S, K_F5);
    CHECK(cursor_line() == fx_line_of(S, "    return a div b"));
    CHECK(!strcmp(uimn.errmsg, "Division by zero"));
    fx_done(S, files);
}

/* A type error in geo: F5 said nothing of where. */
TEST(mixide_qb_main_compile_error_in_quickr_code)
{
    open_qb(S, "print geo.area(2, 3)\n");
    fx_retype(S, fx_line_of(S, "    let total = w * h"), "    let total as string = w");
    pressed(S, K_F5);
    CHECK(cursor_line() == fx_line_of(S, "    let total as string = w"));
    CHECK(strstr(uimn.errmsg, "string") != nullptr);
    fx_done(S, files);
}

static const char util[] = DIALECT "function twice(x as int) as int\n"
                                   "    return x * 2\n"
                                   "end function\n";

static const char geo_util[] = DIALECT "import util\n"
                                       "function area(w as int, h as int) as int\n"
                                       "    return util.twice(w) * h\n"
                                       "end function\n";

/* geo importing util, both Quickr's: the second run found no util, dropped by the first's relink
   (its import was never found again). */
TEST(mixide_qb_main_runs_again_with_quickr_imports)
{
    fx_open_program(S, "");
    fx_write_file("util.bas", util);
    fx_write_file("main.bas", "IMPORT geo\nprint geo.area(2, 3)\n");
    fx_write_file("geo.bas", geo_util);
    uimn_open(S, fx_path_of("main.bas"));
    CHECK(strstr(pressed(S, K_F5), " 12") != nullptr);
    fx_retype(S, fx_line_of(S, "print geo.area(2, 3)"), "print geo.area(3, 3)");
    CHECK(strstr(pressed(S, K_F5), " 18") != nullptr);
    CHECK(fx_line_of(S, "    return x * 2") != ~0u);
    fx_done(S, files);
}

/* An import typed in geo: read as QBasic's tokens, util not read till a run. */
TEST(mixide_qb_main_reads_an_import_typed_in_quickr_code)
{
    open_qb_with(S, DIALECT "' none yet\n", "print 1\n");
    fx_write_file("util.bas", util);
    fx_retype(S, fx_line_of(S, "' none yet"), "import util");
    CHECK(fx_line_of(S, "    return x * 2") != ~0u);
    fx_done(S, files);
}

/* F1 on a Quickr word in geo looked it up in QBasic's topics. */
TEST(mixide_qb_main_f1_finds_quickr_topics)
{
    open_qb_with(
        S,
        DIALECT "import text\nfunction up(s as string) as string\n"
                "    return text.upper(s)\n"
                "end function\n",
        "print geo.up(\"a\")\n"
    );
    CHECK(help_open("build/host/test.hlp"));
    edit_load(S, view_goline(S, fx_line_of(S, "    return text.upper(s)")));
    uimn.edline.col = 17;
    help_f1(S);
    fx_shown(S);
    CHECK(strstr(fx_row(1), "Quickr text") != nullptr);
    help_close();
    fx_done(S, files);
}

/* ---- a Quickr main, a QBasic module */

/* The editor refused a Quickr main importing a QBasic module: shapes not read, no run. */
TEST(mixide_qr_main_opens_and_runs)
{
    open_qr(S, "var r = shapes.Rect(2, 3)\nprint shapes.area(r)\n");
    CHECK(text_dialect(S) == &dialect_quickr);
    const uint32_t a = fx_line_of(S, "    a& = r.W * r.H");
    CHECK(a != ~0u && text_module(S, a) == 1);
    CHECK(slot_at(S, "function Area& (r as Rect)", 0) == SL_KW);
    CHECK(slot_at(S, "var r = shapes.Rect(2, 3)", 0) == SL_KW);
    CHECK(strstr(pressed(S, K_F5), "6") != nullptr);
    fx_done(S, files);
}

/* A QBasic line typed in shapes, and a run after: QBasic's tokens, checked as QBasic's. */
TEST(mixide_qr_main_edits_a_qbasic_line)
{
    open_qr(S, "var r = shapes.Rect(2, 3)\nprint shapes.area(r)\n");
    pressed(S, K_F5);
    fx_retype(S, fx_line_of(S, "    a& = r.W * r.H"), "    a& = r.W * r.H + 1");
    const uint32_t line = fx_line_of(S, "    a& = r.W * r.H + 1");
    CHECK(line != ~0u && !(prog_line(&S->prog, lmap_off(S, line))->flags & LF_TEXT));
    CHECK(strstr(pressed(S, K_F5), "7") != nullptr);
    CHECK(!fx_typed_over(S, line, "    a& = [1]"));
    CHECK(fx_typed_over(S, line, "    a& = r.W * r.H + 2"));
    CHECK(strstr(pressed(S, K_F5), "8") != nullptr);
    fx_done(S, files);
}

/* Into a QBasic function and out, as QBasic spells its names. */
TEST(mixide_qr_main_debugs_qbasic_code)
{
    open_qr(S, "var r = shapes.Rect(2, 3)\nlet a = shapes.area(r)\nprint a\nprint 1\n");
    break_at(S, "let a = shapes.area(r)");
    pressed(S, K_F5);
    CHECK(stopped_at(S, "let a = shapes.area(r)"));
    pressed(S, K_F8);
    CHECK(stopped_at(S, "    a& = r.W * r.H"));
    pressed(S, K_F8);
    CHECK(stopped_at(S, "    Area& = a&"));
    CHECK(local_is(S, 0, "a& = 6"));
    CHECK(frame_is(S, 0, "Area&"));
    edit_load(S, view_goline(S, fx_line_of(S, "    Area& = a&")));
    uimn.edline.col = 12;
    uimn.tip_mouse = false;
    CHECK(screen_has(S, " a& = 6 "));
    CHECK(!strcmp(watched(S, "a& + 1"), "a& + 1 = 7"));
    CHECK(strstr(immediate(S, "print 7 \\ 2"), "3") != nullptr);
    pressed(S, K_SHIFT_F8);
    CHECK(stopped_at(S, "print a"));
    CHECK(local_is(S, 0, "a = 6"));
    CHECK(strstr(immediate(S, "a + 1"), "7") != nullptr);
    pressed(S, K_F10);
    CHECK(stopped_at(S, "print 1"));
    fx_done(S, files);
}

/* A breakpoint in shapes, run to the cursor there, restart. */
TEST(mixide_qr_main_breaks_in_qbasic_code)
{
    open_qr(S, "var r = shapes.Rect(2, 3)\nprint shapes.area(r)\nprint shapes.quot(9, 3)\n");
    break_at(S, "    Area& = a&");
    pressed(S, K_F5);
    CHECK(stopped_at(S, "    Area& = a&"));
    edit_load(S, view_goline(S, fx_line_of(S, "    Quot& = a \\ b")));
    pressed(S, K_CTRL_F7);
    CHECK(stopped_at(S, "    Quot& = a \\ b") && local_is(S, 0, "a& = 9"));
    restart(S);
    CHECK(uimn.stopped);
    pressed(S, K_F5);
    CHECK(stopped_at(S, "    Area& = a&"));
    pressed(S, K_F5);
    CHECK(!uimn.stopped);
    fx_done(S, files);
}

/* A run-time error in shapes: the cursor at its line. */
TEST(mixide_qr_main_runtime_error_in_qbasic_code)
{
    open_qr(S, "print shapes.quot(1, 0)\n");
    pressed(S, K_F5);
    CHECK(cursor_line() == fx_line_of(S, "    Quot& = a \\ b"));
    CHECK(!strcmp(uimn.errmsg, "Division by zero"));
    fx_done(S, files);
}

/* A compile error in shapes, and one in main naming it: each at its line. */
TEST(mixide_qr_main_compile_errors)
{
    open_qr(S, "print shapes.quot(4, 2)\n");
    fx_retype(S, fx_line_of(S, "print shapes.quot(4, 2)"), "print shapes.nope(4, 2)");
    pressed(S, K_F5);
    CHECK(cursor_line() == fx_line_of(S, "print shapes.nope(4, 2)"));
    CHECK(strstr(uimn.errmsg, "nope") != nullptr);
    fx_retype(S, fx_line_of(S, "print shapes.nope(4, 2)"), "print shapes.quot(4, 2)");
    fx_retype(S, fx_line_of(S, "    Quot& = a \\ b"), "    goto nowhere");
    pressed(S, K_F5);
    CHECK(cursor_line() == fx_line_of(S, "    goto nowhere"));
    CHECK(!strcmp(uimn.errmsg, "Label not defined"));
    fx_done(S, files);
}

static const char shapes_counts[] = "IMPORT counts\n"
                                    "FUNCTION Twice& (x AS LONG)\n"
                                    "    Twice& = counts.Plus&(x, x)\n"
                                    "END FUNCTION\n";

/* shapes importing counts, both QBasic's: read once, run twice. */
TEST(mixide_qr_main_qbasic_imports)
{
    fx_open_program(S, "");
    fx_write_file(
        "counts.bas",
        "FUNCTION Plus& (a AS LONG, b AS LONG)\n"
        "    Plus& = a + b\n"
        "END FUNCTION\n"
    );
    fx_write_file("main.bas", DIALECT "import shapes\nprint shapes.twice(3)\n");
    fx_write_file("shapes.bas", shapes_counts);
    uimn_open(S, fx_path_of("main.bas"));
    CHECK(fx_line_of(S, "    Plus& = a + b") != ~0u);
    CHECK(strstr(pressed(S, K_F5), "6") != nullptr);
    fx_retype(S, fx_line_of(S, "print shapes.twice(3)"), "print shapes.twice(4)");
    CHECK(strstr(pressed(S, K_F5), "8") != nullptr);
    fx_done(S, files);
}

/* A QBasic module a Quickr program imported no more stayed till the program was opened again:
   its lines in the store, its names still the program's. */
TEST(mixide_qr_main_drops_a_qbasic_module_no_more_imported)
{
    open_qr(S, "print 1\n");
    CHECK(fx_line_of(S, "    Quot& = a \\ b") != ~0u);
    CHECK(fx_typed_over(S, fx_line_of(S, "import shapes"), "print 2"));
    CHECK(strstr(pressed(S, K_F5), "2") != nullptr);
    CHECK(fx_line_of(S, "    Quot& = a \\ b") == ~0u);
    CHECK(modu_find(&S->modu, fx_path_of("shapes.bas")) == modu_count(&S->modu));
    fx_done(S, files);
}
