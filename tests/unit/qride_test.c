/* The editor on a Quickr program: opened, listed, edited, saved. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "plat/host/host.h"
#include "plat/pkey.h"
#include "session.h"
#include "comp/suggest.h"
#include "ui/debuglocals.h"
#include "ui/debugpane.h"
#include "ui/debugui.h"
#include "ui/debugwatch.h"
#include "ui/edit.h"
#include "ui/errors.h"
#include "ui/help.h"
#include "ui/highlight.h"
#include "ui/menu.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "ui/window.h"
#include "edfix.h"
#include "unit.h"

void headless_set_stdout_is_screen(int yes);

static const char *const files[] = {"main.bas", "gfx.bas", "util.bas", nullptr};

#define DIALECT "' OPTION DIALECT QUICKR\n"

/* main.bas, importing util.bas, both Quickr's, opened. */
static void open_quickr(Session *const S)
{
    fx_open_program(S, "");
    fx_write_file("main.bas", DIALECT "import util\nlet n = util.twice(2)\nprint n\n");
    fx_write_file(
        "util.bas",
        DIALECT "public function twice(x as int) as int\n    return x * 2\nend function\n"
                "public type Box\n    w as int\n    private h as int\n"
                "    private fn area(self) as int\n        return self.w * self.h\n    end fn\n"
                "end type\n"
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
    fx_retype(S, fx_line_of(S, "print n"), "print n + 1");
    CHECK(fx_line_of(S, "print n + 1") != ~0u);
    CHECK(uimn_save(S));
    CHECK(!strcmp(
        fx_file_text("main.bas"),
        "' OPTION DIALECT QUICKR\r\nimport util\r\nlet n = util.twice(2)\r\nprint n + 1\r\n"
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
    const uint32_t line = fx_line_of(S, "print n");
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
    fx_write_file("main.bas", DIALECT "type P\n    y as int\nend type\nprint 1\n");
    uimn_open(S, fx_path_of("main.bas"));
    CHECK(fx_typed_over(S, fx_line_of(S, "    y as int"), "    x as int"));
    CHECK(!fx_typed_over(S, fx_line_of(S, "print 1"), "x as int"));
    fx_done(S, files);
}

/* fn, a run's command, its output in S->cons.sbuf. */
static void captured(Session *const S, void (*const fn)(Session *S))
{
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    host_key_push(FX_ENTER);          /* the end's key, or a box's */
    headless_set_stdout_is_screen(1); /* the console's state left as the tests after want it */
    fn(S);
    headless_set_stdout_is_screen(0);
    pkey_flush();
    S->cons.capture = 0;
}

/* F5 ran nothing of a Quickr program, or a module's top level after the main's. */
TEST(qride_runs_modules_first)
{
    fx_open_program(S, "");
    fx_write_file("main.bas", DIALECT "import util\nprint util.twice(2)\n");
    fx_write_file(
        "util.bas",
        DIALECT "function hello() as int\n    print \"util\"\n    return 1\nend function\nlet u = "
                "hello()\n"
                "public function twice(x as int) as int\n    return x * 2\nend function\n"
    );
    uimn_open(S, fx_path_of("main.bas"));
    captured(S, uimn_run);
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
    fx_retype(S, fx_line_of(S, "print n"), "let s as string = n");
    edit_load(S, view_goline(S, 0));
    captured(S, uimn_run);
    CHECK(uimn.text.first + uimn.ed.line == fx_line_of(S, "let s as string = n"));
    CHECK(uimn.edline.col == 18);
    fx_done(S, files);
}

/* File > New made a QBasic program, asking nothing; after a Quickr one, its lines Quickr's still (the
   emptied store's first record read). The status bar named no dialect. */
TEST(qride_new_asks_the_dialect)
{
    fx_open_program(S, "PRINT 1\n");
    host_key_push(FX_TAB);
    host_key_push(FX_ENTER);
    uimn_act(S, ACT_NEW);
    pkey_flush();
    CHECK(text_quickr(S));
    CHECK(fx_line_of(S, "' OPTION DIALECT QUICKR") == 0);
    CHECK(uimn.ed.line == 1);
    fx_shown(S);
    CHECK(strstr(fx_row(24), "Quickr") != nullptr);
    host_key_push(FX_ENTER);
    uimn_act(S, ACT_NEW);
    pkey_flush();
    CHECK(!text_quickr(S));
    fx_shown(S);
    CHECK(strstr(fx_row(24), "QBasic") != nullptr);
    fx_done(S, files);
}

/* The cursor's text line. */
static uint32_t cursor_line(void) { return uimn.text.first + uimn.ed.line; }

static const char generic[] = DIALECT "function at[T](a as T[], i as int) as T\n"
                                      "    return a[i]\n"
                                      "end function\n"
                                      "var xs as int[]\n"
                                      "xs.push(1)\n"
                                      "print at(xs, %d)\n"
                                      "print 2\n";

static void open_generic(Session *const S, const int i)
{
    char text[sizeof generic + 8];
    snprintf(text, sizeof text, generic, i);
    fx_open_program(S, "");
    fx_write_file("main.bas", text);
    uimn_open(S, fx_path_of("main.bas"));
}

/* F9, F5, F8, Shift+F8, F10 in a Quickr program: stopped at each line; in a generic function, at its
   last line. */
TEST(qride_steps_by_line)
{
    open_generic(S, 0);
    edit_load(S, view_goline(S, fx_line_of(S, "xs.push(1)")));
    udbg_bp(S);
    captured(S, uimn_run);
    CHECK(uimn.stopped && cursor_line() == fx_line_of(S, "xs.push(1)"));
    captured(S, udbg_step);
    CHECK(uimn.stopped && cursor_line() == fx_line_of(S, "print at(xs, 0)"));
    captured(S, udbg_step);
    CHECK(uimn.stopped && cursor_line() == fx_line_of(S, "    return a[i]"));
    captured(S, udbg_sout);
    CHECK(uimn.stopped && cursor_line() == fx_line_of(S, "print 2"));
    captured(S, udbg_restart);
    captured(S, uimn_cont);
    captured(S, udbg_step);
    captured(S, udbg_pstep);
    CHECK(uimn.stopped && cursor_line() == fx_line_of(S, "print 2"));
    fx_done(S, files);
}

/* F9 at a generic function's line while stopped: F5 ran past it. */
TEST(qride_breaks_in_a_generic_set_while_stopped)
{
    open_generic(S, 0);
    edit_load(S, view_goline(S, fx_line_of(S, "xs.push(1)")));
    udbg_bp(S);
    captured(S, uimn_run);
    edit_load(S, view_goline(S, fx_line_of(S, "    return a[i]")));
    udbg_bp(S);
    captured(S, uimn_cont);
    CHECK(uimn.stopped && cursor_line() == fx_line_of(S, "    return a[i]"));
    fx_done(S, files);
}

/* Locals and Call stack spelled a Quickr function's names QBasic's: "X& = 1", "module (X& = 1)". */
TEST(qride_pane_spells_names_as_typed)
{
    fx_open_program(S, "");
    fx_write_file(
        "main.bas",
        DIALECT
        "function twice(myX as int) as int\n    let y = myX * 2\n    return y\nend function\n"
        "print twice(1)\n"
    );
    uimn_open(S, fx_path_of("main.bas"));
    edit_load(S, view_goline(S, fx_line_of(S, "    return y")));
    udbg_bp(S);
    captured(S, uimn_run);
    dloc_read(S, 0);
    CHECK(dloc_count() >= 2);
    CHECK(!strcmp(dloc_row(0), "myX = 1"));
    CHECK(!strcmp(dloc_row(1), "y = 2"));
    dstk_read(S);
    CHECK(!strncmp(dstk_row(0), "twice (myX = 1)", 15));
    fx_done(S, files);
}

/* An inline record, a frame's and the module's, showed as an int: "p = 1", "g = 3". */
TEST(qride_shows_an_inline_record_by_its_fields)
{
    fx_open_program(S, "");
    fx_write_file(
        "main.bas",
        DIALECT
        "type Pt\n    x as int\n    y as single\nend type\n"
        "var g = Pt(3, 0.5)\n"
        "function f(n as int) as int\n    var p = Pt(n, 2.5)\n    return p.x\nend function\n"
        "print f(1)\n"
    );
    uimn_open(S, fx_path_of("main.bas"));
    edit_load(S, view_goline(S, fx_line_of(S, "    return p.x")));
    udbg_bp(S);
    captured(S, uimn_run);
    dloc_read(S, 0);

    if (dloc_count() == 4) /* Locals keeps a name opened in an earlier test open */
        dloc_close(S, 1);

    CHECK(dloc_count() == 2 && !strcmp(dloc_row(1), "\x10p as Pt"));
    CHECK(dloc_open(S, 1) && dloc_count() == 4);
    CHECK(!strcmp(dloc_row(2), "   .x = 1") && !strcmp(dloc_row(3), "   .y = 2.5"));
    dloc_read(S, 1);

    if (dloc_count() > 1)
        dloc_close(S, 0);

    CHECK(dloc_open(S, 0) && !strcmp(dloc_row(0), "\x1Fg as Pt"));
    CHECK(!strcmp(dloc_row(1), "   .x = 3") && !strcmp(dloc_row(2), "   .y = .5"));
    fx_done(S, files);
}

/* A watch on a Quickr function's names read QBasic's MYX!, a new variable: "myX * 2 = 0". */
TEST(qride_watches_names_as_typed)
{
    fx_open_program(S, "");
    fx_write_file(
        "main.bas",
        DIALECT
        "function twice(myX as int) as int\n    let y = myX * 2\n    return y\nend function\n"
        "print twice(3)\n"
    );
    uimn_open(S, fx_path_of("main.bas"));
    edit_load(S, view_goline(S, fx_line_of(S, "    return y")));
    udbg_bp(S);
    captured(S, uimn_run);
    CHECK(dwat_add((const uint8_t *)"myX * 2 + y", 11));
    dwat_eval(S);
    CHECK(!strcmp(dwat_row(0), "myX * 2 + y = 12"));
    dwat_del(S, 0);
    fx_done(S, files);
}

/* text typed in the immediate window and run; its output in S->cons.sbuf, 0-ended. */
static const char *immediate(Session *const S, const char *const text)
{
    dpan_close(); /* it hides the immediate window */
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
    return (const char *)S->cons.sbuf;
}

/* The immediate window ran a Quickr program's line as QBasic's, at the module: print myX a
   Syntax error. */
TEST(qride_immediate_runs_quickr_at_the_stop)
{
    fx_open_program(S, "");
    fx_write_file(
        "main.bas",
        DIALECT
        "var total = 5\n"
        "function twice(myX as int) as int\n    let y = myX * 2\n    return y\nend function\n"
        "print twice(3)\n"
    );
    uimn_open(S, fx_path_of("main.bas"));
    CHECK(strchr(immediate(S, "print total + 1"), '1'));
    wind_focus(S, W_ED1);
    edit_load(S, view_goline(S, fx_line_of(S, "    return y")));
    udbg_bp(S);
    captured(S, uimn_run);
    CHECK(strstr(immediate(S, "print myX; \" \"; y; \" \"; total; \" \"; \"s\""), "3 6 5 s"));
    immediate(S, "total = myX + y");
    CHECK(strchr(immediate(S, "total"), '9'));
    immediate(S, "print nope");
    CHECK(!strcmp(uimn.errmsg, "unknown name nope"));
    CHECK(uimn.stopped);
    captured(S, uimn_cont);
    CHECK(!uimn.stopped);
    fx_done(S, files);
}

static const char shadows[] =
    DIALECT "type V\n    x as int\n    y as int\n"
            "    fn len2(self) as int\n        return self.x * self.x + self.y * self.y\n    end fn\n"
            "end type\n"
            "function f(n as int) as int\n    let v = V(3, 4)\n    var x = n\n"
            "    if n > 0 then\n        let x = 10\n        print x\n"
            "    end if\n    return x + v.len2()\nend function\n"
            "print f(1)\n";

/* Watch's x the stopped line's, v.len2() a method's call: once read as QBasic's, x the first x
   declared, v.len2() a Syntax error; Locals spelled the inner x X'1, v as its type V. */
TEST(qride_watches_quickr_at_the_stopped_line)
{
    fx_open_program(S, "");
    fx_write_file("main.bas", shadows);
    uimn_open(S, fx_path_of("main.bas"));
    edit_load(S, view_goline(S, fx_line_of(S, "        print x")));
    udbg_bp(S);
    edit_load(S, view_goline(S, fx_line_of(S, "    return x + v.len2()")));
    udbg_bp(S);
    captured(S, uimn_run);
    CHECK(dwat_add((const uint8_t *)"x", 1));
    CHECK(dwat_add((const uint8_t *)"v.len2() + n", 12));
    dwat_eval(S);
    CHECK(!strcmp(dwat_row(0), "x = 10"));
    CHECK(!strcmp(dwat_row(1), "v.len2() + n = 26"));
    dloc_read(S, 0);
    CHECK(
        dloc_count() == 4 && !strcmp(dloc_row(1), "\x10v as V") && !strcmp(dloc_row(3), "x = 10")
    );
    captured(S, uimn_cont);
    dwat_eval(S);
    CHECK(!strcmp(dwat_row(0), "x = 1"));
    dwat_del(S, 1);
    dwat_del(S, 0);
    fx_done(S, files);
}

/* A run-time error in a generic function: the cursor at the program's last line. */
TEST(qride_runtime_error_at_its_line)
{
    open_generic(S, 5);
    captured(S, uimn_run);
    CHECK(cursor_line() == fx_line_of(S, "    return a[i]"));
    CHECK(!strcmp(uimn.errmsg, "Subscript out of range"));
    fx_done(S, files);
}

/* A false assert said QBasic's message for its error code, not "assert failed". */
TEST(qride_assert_says_so)
{
    fx_open_program(S, "");
    fx_write_file("main.bas", DIALECT "let a = 1\nassert a = 2\n");
    uimn_open(S, fx_path_of("main.bas"));
    captured(S, uimn_run);
    CHECK(cursor_line() == fx_line_of(S, "assert a = 2"));
    CHECK(!strcmp(uimn.errmsg, "assert failed"));
    fx_done(S, files);
}

/* Line text's slots, as the program window colours them. */
static const uint8_t *slots(Session *const S, const char *const text)
{
    static uint8_t cols[SYNT_KEEP];
    synt_cols(S, (const uint8_t *)text, (uint32_t)strlen(text), cols);
    return cols;
}

/* A Quickr line coloured by QBasic's lexer: function's name a variable's, yield a name, '{' a string's.
   A method's name was a variable's and self a parameter's. */
TEST(qride_colours_by_quickrs_lexer)
{
    open_quickr(S);
    const uint8_t *c = slots(S, "function twice(x as int) as int ' doubled");
    CHECK(c[0] == SL_KW && c[9] == SL_FNM && c[15] == SL_PAR && c[20] == SL_TYP);
    CHECK(c[28] == SL_TYP && c[32] == SL_COM && c[40] == SL_COM);
    c = slots(S, "yield n");
    CHECK(c[0] == SL_KW && c[6] == SL_NAM);
    c = slots(S, "let s = f\"{n} it's\" + 'x'");
    CHECK(c[0] == SL_KW && c[8] == SL_STR && c[11] == SL_NAM && c[15] == SL_STR && c[21] == SL_OP);
    CHECK(c[23] == SL_COM);
    c = slots(S, "    fn length(mut self, k as double)");
    CHECK(c[4] == SL_KW && c[7] == SL_FNM && c[14] == SL_KW && c[18] == SL_KW && c[24] == SL_PAR);
    fx_done(S, files);
}

/* What sugg_find offers for text typed on main.bas's last line: has it want. */
static bool offers(Session *const S, const char *const text, const char *const want)
{
    Suggs out = {};

    const uint16_t n = (uint16_t)strlen(text);
    const uint32_t rec = text_rec(S, fx_line_of(S, "print total"));
    bool found = false;

    if (sugg_find(S, (const uint8_t *)text, n, n, rec, &out))
        for (uint16_t i = 0; i < out.n; i++)
            found |= !strcmp((const char *)out.list[i].text, want);

    free(out.list);
    return found;
}

/* Completion offered QBasic's words in a Quickr program: RETURN, no module's fns, no fields. After
   a value's dot it offered the functions taking one first, which a.f() no longer calls, and none of
   its type's methods. */
TEST(qride_completes_quickr_words)
{
    fx_open_program(S, "");
    fx_write_file(
        "main.bas",
        DIALECT "import util\ntype P\n    x as int\n    y as int\n"
                "    fn half(self) as int\n        return self.x div 2\n    end fn\n"
                "    fn origin() as P\n        return P(0, 0)\n    end fn\nend type\n"
                "function norm(p as P) as int\n    return p.x + p.y\nend function\n"
                "var q as P\nvar box as util.Box\nlet total = norm(q)\nprint total\n"
    );
    fx_write_file(
        "util.bas",
        DIALECT "public function twice(x as int) as int\n    return x * 2\nend function\n"
                "public type Box\n    w as int\n    private h as int\n"
                "    private fn area(self) as int\n        return self.w * self.h\n    end fn\n"
                "end type\n"
    );
    uimn_open(S, fx_path_of("main.bas"));
    CHECK(offers(S, "re", "return"));
    CHECK(!offers(S, "re", "RETURN"));
    CHECK(offers(S, "print to", "total"));
    CHECK(offers(S, "let a = util.t", "twice"));
    CHECK(offers(S, "let a = q.", "x"));
    CHECK(offers(S, "let a = q.", "y"));
    CHECK(offers(S, "let a = q.", "half"));
    CHECK(!offers(S, "let a = q.", "norm"));
    CHECK(!offers(S, "let a = q.", "origin"));
    CHECK(!offers(S, "let a = q.", "total"));
    CHECK(offers(S, "let a = P.", "origin"));
    CHECK(!offers(S, "let a = P.", "half"));
    CHECK(!offers(S, "let a = P.", "x"));
    CHECK(offers(S, "let a = box.", "w"));
    CHECK(!offers(S, "let a = box.", "h"));
    CHECK(!offers(S, "let a = box.", "area"));
    fx_done(S, files);
}

/* The cursor at col of line text's line. */
static void cursor_at(Session *const S, const char *const text, const uint16_t col)
{
    edit_load(S, view_goline(S, fx_line_of(S, text)));
    uimn.edline.col = col;
}

/* F1 in a Quickr program found QBasic's topics: none for function, none for text.upper, a beep for s. */
TEST(qride_f1_finds_quickr_topics)
{
    fx_open_program(S, "");
    fx_write_file(
        "main.bas",
        DIALECT "function up(s as string) as string\n    return text.upper(s)\nend function\n"
    );
    uimn_open(S, fx_path_of("main.bas"));
    CHECK(help_open("build/host/test.hlp"));
    cursor_at(S, "function up(s as string) as string", 1);
    CHECK(help_topic_exists(S));
    cursor_at(S, "    return text.upper(s)", 17);
    CHECK(help_topic_exists(S));
    help_f1(S);
    fx_shown(S);
    CHECK(strstr(fx_row(1), "Quickr text") != nullptr);
    help_close();
    cursor_at(S, "    return text.upper(s)", 22);
    CHECK(!help_topic_exists(S));
    help_f1(S);
    fx_shown(S);
    CHECK(strstr(fx_row(1), "Quickr index") != nullptr); /* none: Quickr's index */
    help_close();
    fx_done(S, files);
}
