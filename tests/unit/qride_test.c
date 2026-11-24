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

static const char *const files[] = {"main.bas", "gfx.bas", "util.bas", nullptr};

#define DIALECT "' OPTION DIALECT QUICKR\n"

/* main.bas, importing util.bas, both Quickr's, opened. */
static void open_quickr(Session *const S)
{
    fx_open_program(S, "");
    fx_write_file("main.bas", DIALECT "import util\nlet n = util.twice(2)\nprint(n)\n");
    fx_write_file(
        "util.bas",
        DIALECT "public fn twice(x as int) as int\n    return x * 2\nend fn\n"
    );
    uimn_open(S, fx_path_of("main.bas"));
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

/* A Quickr file was read as QBasic: its lines LIST's spelling, its import not followed. */
TEST(qride_opens_a_program_and_its_modules)
{
    open_quickr(S);
    CHECK(text_quickr(S));
    CHECK(fx_line_of(S, "let n = util.twice(2)") != ~0u);
    const uint32_t ret = fx_line_of(S, "    return x * 2");
    CHECK(ret != ~0u);
    CHECK(text_module(S, ret) == 1);
    fx_shown(S);
    CHECK(!strncmp(fx_row(fx_row_of(1)) + 1, "import util", 11));
    fx_done(S, files);
}

/* F2 listed a Quickr file and none of its fns. */
TEST(qride_lists_files_and_fns)
{
    open_quickr(S);
    CHECK(items(S) == 3);
    CHECK(!strcmp(item(S, 0), fx_path_of("main.bas")));
    CHECK(!strcmp(item(S, 1), fx_path_of("util.bas")));
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
    CHECK(!strcmp(
        fx_file_text("main.bas"),
        "' OPTION DIALECT QUICKR\r\nimport util\r\nlet n = util.twice(2)\r\nprint(n + 1)\r\n"
    ));
    fx_done(S, files);
}

/* The dialect's line was a line as any other: typed over, cut, joined to the next. */
TEST(qride_keeps_the_dialect_line)
{
    open_quickr(S);
    edit_load(S, view_goline(S, 0));
    fx_type(S, "x");
    fx_key(S, FX_CTRL_Y, 0);
    edit_load(S, view_goline(S, 1));
    fx_key(S, FX_BACKSPACE, 0);
    edit_commit(S);
    CHECK(fx_line_of(S, "' OPTION DIALECT QUICKR") == 0);
    CHECK(fx_line_of(S, "import util") == 1);
    edit_load(S, view_goline(S, 0));
    fx_key(S, FX_END, 0);
    fx_key(S, FX_ENTER, 0);
    CHECK(uimn.ed.line == 1 && fx_line_of(S, "") == 1);
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
    fx_write_file("main.bas", DIALECT "type P\n    y as int\nend type\nprint(1)\n");
    uimn_open(S, fx_path_of("main.bas"));
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
    fx_write_file("main.bas", DIALECT "import util\nprint(util.twice(2))\n");
    fx_write_file(
        "util.bas",
        DIALECT "fn hello() as int\n    print(\"util\")\n    return 1\nend fn\nlet u = hello()\n"
        "public fn twice(x as int) as int\n    return x * 2\nend fn\n"
    );
    uimn_open(S, fx_path_of("main.bas"));
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
