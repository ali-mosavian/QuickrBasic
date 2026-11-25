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
