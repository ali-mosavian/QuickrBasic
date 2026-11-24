/* The editor on a Quickr program: opened, listed, edited, saved. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "plat/host/host.h"
#include "plat/pkey.h"
#include "session.h"
#include "ui/edit.h"
#include "ui/errors.h"
#include "ui/menu.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "edfix.h"
#include "unit.h"

void headless_set_stdout_is_screen(int yes);

static const char *const files[] =
    {"main.bas", "gfx.bas", "main.qr", "util.qr", "PROG.QR", "COPY.QR", nullptr};

/* main.qr, importing util.qr, opened. */
static void open_quickr(Session *const S)
{
    fx_open_program(S, "");
    fx_write_file("main.qr", "import util\nlet n = util.twice(2)\nprint(n)\n");
    fx_write_file("util.qr", "public fn twice(x as int) as int\n    return x * 2\nend fn\n");
    uimn_open(S, fx_path_of("main.qr"));
}

/* The SUBs box's item i. */
static const char *item(Session *const S, const uint32_t i)
{
    const DlogList *const l = view_list(S);
    return l && i < l->n ? l->items + i * l->size : "";
}

static uint32_t items(Session *const S)
{
    const DlogList *const l = view_list(S);
    return l ? l->n : 0;
}

/* A .qr was read as QBasic: its lines LIST's spelling, its import not followed. */
TEST(qride_opens_a_program_and_its_modules)
{
    open_quickr(S);
    CHECK(text_quickr(S));
    CHECK(fx_line_of(S, "let n = util.twice(2)") != ~0u);
    const uint32_t ret = fx_line_of(S, "    return x * 2");
    CHECK(ret != ~0u);
    CHECK(text_module(S, ret) == 1);
    fx_shown(S);
    CHECK(!strncmp(fx_row(fx_row_of(0)) + 1, "import util ", 12));
    fx_done(S, files);
}

/* F2 listed a Quickr file and none of its fns. */
TEST(qride_lists_files_and_fns)
{
    open_quickr(S);
    CHECK(items(S) == 3);
    CHECK(!strcmp(item(S, 0), fx_path_of("main.qr")));
    CHECK(!strcmp(item(S, 1), fx_path_of("util.qr")));
    CHECK(!strcmp(item(S, 2), "  twice"));
    fx_done(S, files);
}

/* A Quickr line typed was stored as QBasic's: PRINT (n + 1). */
TEST(qride_keeps_a_line_as_typed)
{
    open_quickr(S);
    fx_retype(S, fx_line_of(S, "print(n)"), "print(n + 1)");
    CHECK(fx_line_of(S, "print(n + 1)") != ~0u);
    CHECK(uimn_save(S));
    CHECK(
        !strcmp(fx_file_text("main.qr"), "import util\r\nlet n = util.twice(2)\r\nprint(n + 1)\r\n")
    );
    fx_done(S, files);
}

/* File, Open listed the .BAS files alone. */
TEST(qride_open_lists_qr_files)
{
    fx_open_program(S, "");
    fx_write_file("PROG.QR", "print(1)\n");
    char was[4096];
    CHECK(getcwd(was, sizeof was) != nullptr);
    CHECK(!chdir(fx_path_of("")));
    remove("main.bas");
    remove("gfx.bas");
    host_key_push(FX_TAB);
    host_key_push(FX_DOWN);
    host_key_push(FX_ENTER);
    host_key_push(FX_ESC);
    uimn_act(S, ACT_OPEN);
    pkey_flush();
    CHECK(!chdir(was));
    const size_t n = strlen(uimn.file);
    CHECK(n >= 7 && !strcmp(uimn.file + n - 7, "PROG.QR"));
    CHECK(text_quickr(S));
    fx_done(S, files);
}

/* Save As named a Quickr program .BAS. */
TEST(qride_save_as_keeps_qr)
{
    open_quickr(S);
    char was[4096];
    CHECK(getcwd(was, sizeof was) != nullptr);
    CHECK(!chdir(fx_path_of("")));
    host_key_push(0x2E63); /* c */
    host_key_push(0x186F); /* o */
    host_key_push(0x1970); /* p */
    host_key_push(0x1579); /* y */
    host_key_push(FX_ENTER);
    host_key_push(FX_ESC);
    uimn_act(S, ACT_SAVEAS);
    pkey_flush();
    CHECK(!chdir(was));
    CHECK(!strcmp(uimn.file, "COPY.QR"));
    CHECK(!strcmp(fx_file_text("COPY.QR"), "import util\r\nlet n = util.twice(2)\r\nprint(n)\r\n"));
    fx_done(S, files);
}

/* A Quickr line typed was checked as QBasic's, or not at all. */
TEST(qride_checks_a_line_as_typed)
{
    open_quickr(S);
    const uint32_t line = fx_line_of(S, "print(n)");
    CHECK(!fx_typed_over(S, line, "let = 3"));
    uint16_t col = 0;
    CHECK(errs_at(S, line, &col) && col == 4);
    CHECK(errs_message(S) == nullptr || strstr(errs_message(S), "expected") != nullptr);
    fx_retype(S, line, "if n > 1 then");
    CHECK(!errs_at(S, line, &col));
    fx_done(S, files);
}

/* A field's line was checked out of its type, a statement's. */
TEST(qride_checks_a_line_in_its_block)
{
    fx_open_program(S, "");
    fx_write_file("main.qr", "type P\n    y as int\nend type\nprint(1)\n");
    uimn_open(S, fx_path_of("main.qr"));
    CHECK(fx_typed_over(S, fx_line_of(S, "    y as int"), "    x as int"));
    CHECK(!fx_typed_over(S, fx_line_of(S, "print(1)"), "x as int"));
    fx_done(S, files);
}

/* Run, its output in S->cons.sbuf. */
static void run_captured(Session *const S)
{
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    host_key_push(FX_ENTER);          /* the end's key, or a box's */
    headless_set_stdout_is_screen(1); /* the console's state left as the tests after want it */
    uimn_run(S);
    headless_set_stdout_is_screen(0);
    pkey_flush();
    S->cons.capture = 0;
}

/* F5 ran nothing of a Quickr program, or a module's top level after the main's. */
TEST(qride_runs_modules_first)
{
    fx_open_program(S, "");
    fx_write_file("main.qr", "import util\nprint(util.twice(2))\n");
    fx_write_file(
        "util.qr",
        "fn hello() as int\n    print(\"util\")\n    return 1\nend fn\nlet u = hello()\n"
        "public fn twice(x as int) as int\n    return x * 2\nend fn\n"
    );
    uimn_open(S, fx_path_of("main.qr"));
    run_captured(S);
    CHECK(strstr((const char *)S->cons.sbuf, "util") != nullptr);
    CHECK(
        S->cons.sblen > 0 &&
        strstr((const char *)S->cons.sbuf, "util") < strstr((const char *)S->cons.sbuf, "4")
    );
    fx_done(S, files);
}

/* A type error stopped nothing: the run went on, the cursor stayed. */
TEST(qride_run_stops_at_a_type_error)
{
    open_quickr(S);
    fx_retype(S, fx_line_of(S, "print(n)"), "let s as string = n");
    edit_load(S, view_goline(S, 0));
    run_captured(S);
    CHECK(uimn.text.first + uimn.ed.line == fx_line_of(S, "let s as string = n"));
    CHECK(uimn.edline.col == 18);
    fx_done(S, files);
}
