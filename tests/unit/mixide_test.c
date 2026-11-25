/* The editor on a program of both dialects: a QBasic main importing a Quickr module, and a Quickr
   main importing a QBasic one. */
#include <stdio.h>
#include <stdlib.h>

#include "plat/host/host.h"
#include "plat/pkey.h"
#include "session.h"
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

static const char geo[] = DIALECT "public type Spot\n"
                                  "    x as int\n"
                                  "    y as int\n"
                                  "end type\n"
                                  "public function area(w as int, h as int) as int\n"
                                  "    let total = w * h\n"
                                  "    return total\n"
                                  "end function\n"
                                  "public function quot(a as int, b as int) as int\n"
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

/* geo's lines were coloured by QBasic's lexer: public a name, not a keyword. */
TEST(mixide_qb_main_colours_each_file_its_own)
{
    open_qb(S, "x& = geo.area(2, 3)\nprint x&\n");
    CHECK(slot_at(S, "public function area(w as int, h as int) as int", 0) == SL_KW);
    CHECK(slot_at(S, "public function area(w as int, h as int) as int", 16) == SL_FNM);
    CHECK(slot_at(S, "public function area(w as int, h as int) as int", 21) == SL_PAR);
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
