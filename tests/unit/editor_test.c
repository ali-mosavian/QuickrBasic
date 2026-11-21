/* The editor on a program that imports modules: their lines shown, edited, run and saved. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include "plat/host/host.h"
#include "plat/pkey.h"
#include "session.h"
#include "ui/debugui.h"
#include "ui/edit.h"
#include "ui/linemap.h"
#include "ui/mouse.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "ui/window.h"
#include "unit.h"

/* The program's screen kept while the editor shows: none here (plat/view's on the Mac). */
uint32_t qbrt_shot_size(void) { return 1; }
void qbrt_shot_take(void *const shot) {}
void qbrt_shot_show(const void *const shot) {}
void qbrt_shot_drop(void) {}

static char dir[] = "/tmp/qb_editor_XXXXXX";

/* dir's file name: its path. */
static const char *path_of(const char *const name)
{
    static char path[4][128];
    static uint32_t next;
    char *const p = path[next++ % 4];
    snprintf(p, sizeof path[0], "%s/%s", dir, name);
    return p;
}

static void write_file(const char *const name, const char *const text)
{
    FILE *const f = fopen(path_of(name), "wb");
    CHECK(f && fputs(text, f) >= 0);
    fclose(f);
}

/* dir's file name: a .qbx of the interface text alone, no code. */
static void write_qbx(const char *const name, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    uint8_t head[20] = {'Q', 'B', 'X', 0x1A, 1, 0, 1, 0, 0, 0, 0, 0, sizeof head};

    memcpy(head + 16, &n, sizeof n);
    FILE *const f = fopen(path_of(name), "wb");
    CHECK(f && fwrite(head, 1, sizeof head, f) == sizeof head && fputs(text, f) >= 0);
    fclose(f);
}

/* dir's file name's text. */
static const char *file_text(const char *const name)
{
    static char text[4096];
    FILE *const f = fopen(path_of(name), "rb");
    const size_t n = f ? fread(text, 1, sizeof text - 1, f) : 0;
    text[n] = 0;

    if (f)
        fclose(f);

    return text;
}

/* main.bas importing gfx.bas, opened. */
static void open_program(Session *const S, const char *const gfx)
{
    CHECK(mkdtemp(dir));
    write_file("main.bas", "import gfx\r\ngfx.hi\r\n");
    write_file("gfx.bas", gfx);
    S->dbug.mode = 0; /* debug_test.c's last step */
    uimn_init(S);
    uimn_open(S, path_of("main.bas"));
}

/* The editor left, its files gone, an empty program. */
static void done(Session *const S, const char *const *const names)
{
    for (size_t i = 0; names[i]; i++)
        remove(path_of(names[i]));

    rmdir(dir);
    memcpy(dir + sizeof dir - 7, "XXXXXX", 6);
    uimn.on = false;
    S->repl.keep_pre = false;
    modu_clear(&S->modu, "");
    session_new(S);
}

/* The text line, from the whole text's first, that is text, as LIST spells it; ~0 none. */
static uint32_t line_of(Session *const S, const char *const text)
{
    for (uint32_t line = 0; line < uimn.text.all; line++) {
        const uint32_t n = text_record(S, lmap_off(S, line), &uimn.build);

        if (n == strlen(text) && !memcmp(uimn.build.text, text, n))
            return line;
    }

    return ~0u;
}

/* Text line line typed over with text and entered, as the editor's Enter does: edit_commit's
   answer, false a syntax error (its box closed). */
static bool typed_over(Session *const S, const uint32_t line, const char *const text)
{
    CHECK(line < uimn.text.all);

    if (line >= uimn.text.all)
        return false;

    edit_load(S, view_goline(S, line));
    uimn.edline.len = (uint16_t)strlen(text);
    memcpy(edit_lb_need(S, &uimn.edline, uimn.edline.len), text, uimn.edline.len);
    uimn.edline.dirty = true;
    host_key_push(0x011B); /* Esc: an error's box closed, not waited on */
    const bool ok = edit_commit(S);
    pkey_flush();
    return ok;
}

static void retype(Session *const S, const uint32_t line, const char *const text)
{
    CHECK(typed_over(S, line, text));
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

/* The program run as the editor runs it: its output in S->cons.sbuf. */
static Status run(Session *const S)
{
    uimn_sync(S);
    udbg_prep(S);
    return unit_typed(S, "RUN");
}

/* An imported module's lines were hidden: the SUBs box listed the main program alone. */
TEST(editor_lists_each_file)
{
    open_program(S, "sub hi\r\nprint 1\r\nend sub\r\n");
    CHECK(items(S) == 3);
    CHECK(!strcmp(item(S, 0), path_of("main.bas")));
    CHECK(!strcmp(item(S, 1), path_of("gfx.bas")));
    CHECK(!strcmp(item(S, 2), "  hi"));
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* A run read a module from its file again: the module's lines as edited were dropped. A line
   typed in the module took its names from the main program's: t was the main's, 0. */
TEST(editor_runs_a_module_as_edited)
{
    open_program(S, "const t = 7\r\nsub hi\r\nprint 1\r\nend sub\r\n");
    retype(S, line_of(S, "print 1"), "print t;");
    CHECK(run(S) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 7 ");
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* dir's file name's time, set to 1 first (set). */
static time_t file_time(const char *const name, const bool set)
{
    if (set)
        utime(path_of(name), &(struct utimbuf){1, 1});

    struct stat st;
    return stat(path_of(name), &st) ? 0 : st.st_mtime;
}

/* Save wrote the main program alone, over its own file: a module's edits were lost. A module not
   changed is not written. The main's IMPORT, kept as typed (a syntax error to the editor's check),
   is written as LIST spells it. */
TEST(editor_saves_each_changed_module)
{
    open_program(S, "import util\r\nsub hi\r\nprint 1\r\nend sub\r\n");
    write_file("main.bas", "import  gfx\r\ngfx.hi\r\n");
    write_file("util.bas", "const u = 1\r\n");
    file_time("util.bas", true);
    uimn_open(S, path_of("main.bas"));
    retype(S, line_of(S, "print 1"), "print 2");
    CHECK(uimn_save(S));
    CHECK(!strcmp(file_text("gfx.bas"), "import util\r\nsub hi\r\nprint 2\r\nend sub\r\n"));
    CHECK(file_time("util.bas", false) == 1);
    CHECK(!strcmp(file_text("main.bas"), "import gfx\r\ngfx.hi\r\n"));
    CHECK(!uimn.text.unsaved);
    done(S, (const char *[]){"main.bas", "gfx.bas", "util.bas", nullptr});
}

/* An IMPORT typed loads its module: its file and procedures listed. */
TEST(editor_loads_a_typed_import)
{
    open_program(S, "sub hi\r\nend sub\r\n");
    write_file("main.bas", "print 1\r\n");
    write_file("util.bas", "sub there\r\nend sub\r\n");
    uimn_open(S, path_of("main.bas"));
    CHECK(items(S) == 1);
    retype(S, 0, "import util");
    CHECK(items(S) == 3);
    CHECK(!strcmp(item(S, 1), path_of("util.bas")));
    CHECK(!strcmp(item(S, 2), "  there"));
    CHECK(!text_unsaved(S, 1));
    done(S, (const char *[]){"main.bas", "gfx.bas", "util.bas", nullptr});
}

/* A SUB typed in a module's view is the module's: saved to its file, listed under it. */
TEST(editor_puts_a_typed_sub_in_its_file)
{
    open_program(S, "const t = 7\r\nsub hi\r\nend sub\r\n");
    retype(S, line_of(S, "const t = 7"), "sub there");
    CHECK(items(S) == 4);
    CHECK(!strcmp(item(S, 3), "  there"));
    CHECK(uimn_save(S));
    CHECK(
        !strcmp(file_text("gfx.bas"), "\r\nsub hi\r\nend sub\r\nsub there\r\n    \r\nend sub\r\n")
    );
    CHECK(!strcmp(file_text("main.bas"), "import gfx\r\ngfx.hi\r\n"));
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* A module's lines were stored as LIST spells them: their indent, and a tab's, dropped from the
   view and from the file saved. */
TEST(editor_keeps_a_module_s_indent)
{
    open_program(S, "sub hi\r\n    print 1\r\n\tprint 2\r\nend sub\r\n");
    retype(S, line_of(S, "    print 1"), "    print 3");
    CHECK(uimn_save(S));
    CHECK(
        !strcmp(file_text("gfx.bas"), "\r\nsub hi\r\n    print 3\r\n        print 2\r\nend sub\r\n")
    );
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* Nothing was declared once opened: a call to a module's SUB typed was a syntax error. */
TEST(editor_checks_a_call_into_a_module)
{
    open_program(S, "sub hi\r\nend sub\r\n");
    uint8_t text[16] = "gfx.hi";
    uint32_t lexed;
    CHECK(edit_canon(S, text, 6, &lexed) && lexed);
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* A file's last procedure deleted: the windows on it went to the next file's module. */
TEST(editor_deletes_a_module_s_last_sub)
{
    open_program(S, "sub hi\r\nend sub\r\n");
    write_file("main.bas", "import gfx\r\nimport util\r\n");
    write_file("util.bas", "const u = 1\r\n");
    uimn_open(S, path_of("main.bas"));
    view_goline(S, line_of(S, "sub hi"));

    for (int i = 0; i < 3; i++)
        host_key_push(0x0F09); /* Tab: to Delete */

    host_key_push(0x1C0D); /* Enter: Delete */
    host_key_push(0x1C0D); /* Enter: Yes */
    view_subs(S);
    pkey_flush();
    CHECK(items(S) == 3);
    CHECK(!strcmp(view_file(S, uimn.ed.view), path_of("gfx.bas")));
    done(S, (const char *[]){"main.bas", "gfx.bas", "util.bas", nullptr});
}

/* A procedure added before a module's views: the cursor's place left in each went to the view
   before it. */
TEST(editor_keeps_a_module_view_s_place)
{
    open_program(S, "const t = 7\r\nconst u = 8\r\n");
    view_goline(S, line_of(S, "const t = 7"));
    edit_load(S, 1); /* the cursor on const u */
    retype(S, line_of(S, "gfx.hi"), "sub there");
    view_goline(S, line_of(S, "const t = 7"));
    CHECK(uimn.ed.line == 1);
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* At Open a call into a module failed the editor's check, the module not bound yet: the line was
   kept as typed. One typed before its IMPORT is checked again once the module is. */
TEST(editor_checks_module_calls_once_bound)
{
    open_program(S, "sub hi (n)\r\nend sub\r\n");
    write_file("main.bas", "import gfx\r\ngfx.hi  1\r\n");
    uimn_open(S, path_of("main.bas"));
    CHECK(line_of(S, "gfx.hi 1") == 1);
    CHECK(!uimn.text.unsaved);
    write_file("main.bas", "print 1\r\nprint 2\r\n");
    uimn_open(S, path_of("main.bas"));
    CHECK(!typed_over(S, 1, "gfx.hi  2"));
    retype(S, 0, "import gfx");
    CHECK(line_of(S, "gfx.hi 2") == 1);
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* A module no import reached stayed loaded till the next Open. One edited stays till saved. The
   window on one dropped goes to the main's module, not to the view now in its place. */
TEST(editor_drops_a_module_not_imported)
{
    open_program(S, "sub hi\r\nend sub\r\n");
    write_file("main.bas", "import util\r\nimport gfx\r\n");
    write_file("util.bas", "const u = 1\r\n");
    uimn_open(S, path_of("main.bas"));
    CHECK(items(S) == 4);
    retype(S, line_of(S, "const u = 1"), "const u = 2");
    retype(S, line_of(S, "import util"), "print 1");
    uimn_sync(S);
    CHECK(items(S) == 4);
    CHECK(uimn_save(S));
    retype(S, line_of(S, "print 1"), "print 2");
    view_goline(S, line_of(S, "const u = 2"));
    uimn_sync(S);
    CHECK(items(S) == 3);
    CHECK(!strcmp(item(S, 1), path_of("gfx.bas")));
    CHECK(!strcmp(view_file(S, uimn.ed.view), path_of("main.bas")));
    CHECK(!strcmp(file_text("util.bas"), "const u = 2\r\n"));
    done(S, (const char *[]){"main.bas", "gfx.bas", "util.bas", nullptr});
}

/* main.bas, its names defined in it and in gfx.bas, opened. */
static void open_definitions(Session *const S)
{
    open_program(S, "sub hi\r\nprint \"hi\"\r\nend sub\r\nsub plot (x)\r\nprint x\r\nend sub\r\n");
    write_file(
        "main.bas",
        "import gfx\r\nfrom gfx import plot as show\r\nconst top = 3\r\ndim shared total\r\ndim n, "
        "visits\r\n"
        "type pt\r\nx as integer\r\nend type\r\ndim p as pt\r\np.x = 1\r\ngfx.hi\r\nshow 2\r\n"
        "bump 1\r\nprint total; top; p.x\r\ngoto 100\r\n100 print \"a\": goto done\r\ndone:\r\n"
        "sub bump (n)\r\nstatic visits\r\ndim k, q as pt\r\nk = n + top\r\nvisits = visits + k\r\n"
        "total = total + k\r\nend sub\r\n"
    );
    uimn_open(S, path_of("main.bas"));
}

static uint16_t col_of(const char *const text, const char *const word)
{
    const char *const at = strstr(text, word);
    CHECK(at != nullptr);
    return at ? (uint16_t)(at - text) : 0;
}

/* The cursor on the first `word` of the text line spelled `text`. */
static void cursor_on(Session *const S, const char *const text, const char *const word)
{
    const uint32_t line = line_of(S, text);
    CHECK(line != ~0u);
    edit_load(S, view_goline(S, line));
    uimn.edline.col = col_of(text, word);
}

/* The cursor on the first `word` of the line spelled `text`. */
static bool cursor_at(Session *const S, const char *const text, const char *const word)
{
    return uimn.text.first + uimn.ed.line == line_of(S, text) &&
           uimn.edline.col == col_of(text, word);
}

/* Ctrl+B on the first `word` of the line `text`: the cursor on the first `at` of the line `to`. */
static bool defined(
    Session *const S,
    const char *const text,
    const char *const word,
    const char *const to,
    const char *const at
)
{
    cursor_on(S, text, word);
    uimn_key(S, 0x3002);
    return cursor_at(S, to, at);
}

/* Ctrl+B, Ctrl+click: the line that defines the name at the cursor, in its file or the module an
   import binds it to; nothing for a keyword. */
TEST(editor_goes_to_a_definition)
{
    open_definitions(S);
    CHECK(defined(S, "gfx.hi", "hi", "sub hi", "hi"));
    CHECK(!strcmp(view_file(S, uimn.ed.view), path_of("gfx.bas")));
    CHECK(defined(S, "show 2", "show", "sub plot (x)", "plot"));
    CHECK(defined(S, "bump 1", "bump", "sub bump (n)", "bump"));
    CHECK(defined(S, "print total; top; p.x", "total", "dim shared total", "total"));
    CHECK(defined(S, "print total; top; p.x", "top", "const top = 3", "top"));
    CHECK(defined(S, "print total; top; p.x", "p.x", "dim p as pt", "p"));
    CHECK(defined(S, "dim p as pt", "pt", "type pt", "pt"));
    CHECK(defined(S, "goto 100", "100", "100 print \"a\": goto done", "100"));
    CHECK(defined(S, "100 print \"a\": goto done", "done", "done:", "done"));
    CHECK(defined(S, "k = n + top", "k", "dim k, q as pt", "k"));
    CHECK(
        defined(S, "k = n + top", "n", "sub bump (n)", "n")
    ); /* the parameter, not the module's n */
    CHECK(defined(S, "k = n + top", "top", "const top = 3", "top"));
    CHECK(defined(S, "dim k, q as pt", "pt", "type pt", "pt"));
    CHECK(defined(S, "visits = visits + k", "visits", "static visits", "visits"));
    CHECK(defined(S, "total = total + k", "total", "dim shared total", "total"));
    CHECK(defined(S, "print total; top; p.x", "print", "print total; top; p.x", "print"));
    cursor_on(S, "import gfx", "gfx");
    uimn_key(S, 0x3002);
    CHECK(!strcmp(view_file(S, uimn.ed.view), path_of("gfx.bas")) && !view_at(uimn.ed.view)->kind);
    CHECK(!uimn.win.win[W_ED2].height); /* all in the one window */
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

TEST(editor_goes_to_a_definition_on_ctrl_click)
{
    open_definitions(S);
    uimn_rows(25); /* the windows where the mouse finds them */
    cursor_on(S, "import gfx", "import");
    const uint32_t line = line_of(S, "bump 1") - uimn.text.first;
    uimn.mouse = (UiMouse){.event = MS_PRESS,
                           .row = (uint8_t)(uimn.ed.row0 + line - uimn.ed.top),
                           .col = (uint8_t)(uimn.ed.x + 1 + 1 - uimn.ed.left)};

    uimn.keyflags = PKEY_CTRL;
    uimn_key(S, KEY_MOUSE);
    uimn.keyflags = 0;
    CHECK(cursor_at(S, "sub bump (n)", "bump"));
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* Ctrl+Alt+B: the definition in the split's other window, split side by side first; the cursor's
   window keeps its line; none found, no split. */
TEST(editor_goes_to_a_definition_beside)
{
    open_definitions(S);
    uimn_rows(25);
    uimn.keyflags = PKEY_CTRL | PKEY_ALT;
    cursor_on(S, "print total; top; p.x", "print");
    uimn_key(S, 0x3000);
    CHECK(!uimn.win.win[W_ED2].height);
    cursor_on(S, "bump 1", "bump");
    uimn_key(S, 0x3000);
    CHECK(uimn.win.vert && wind_active() == W_ED2 && cursor_at(S, "sub bump (n)", "bump"));
    cursor_on(S, "k = n + top", "top");
    uimn_key(S, 0x3000);
    CHECK(uimn.win.vert && wind_active() == W_ED1 && cursor_at(S, "const top = 3", "top"));
    wind_act(S, W_ED2);
    CHECK(cursor_at(S, "k = n + top", "top"));
    uimn.keyflags = 0;
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

TEST(editor_goes_to_a_definition_beside_on_ctrl_alt_click)
{
    open_definitions(S);
    uimn_rows(25);
    cursor_on(S, "import gfx", "import");
    const uint32_t line = line_of(S, "bump 1") - uimn.text.first;
    uimn.mouse = (UiMouse){.event = MS_PRESS,
                           .row = (uint8_t)(uimn.ed.row0 + line - uimn.ed.top),
                           .col = (uint8_t)(uimn.ed.x + 1 + 1 - uimn.ed.left)};

    uimn.keyflags = PKEY_CTRL | PKEY_ALT;
    uimn_key(S, KEY_MOUSE);
    uimn.keyflags = 0;
    CHECK(uimn.win.vert && wind_active() == W_ED2 && cursor_at(S, "sub bump (n)", "bump"));
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* SUBs' Edit in Split: the window split, one over the other, the other one active. */
TEST(editor_edits_a_sub_in_a_split)
{
    open_definitions(S);
    host_key_push(0x0F09); /* Tab: to Edit in Active */
    host_key_push(0x0F09); /* Tab: to Edit in Split */
    host_key_push(0x1C0D);
    view_subs(S);
    pkey_flush();
    CHECK(uimn.win.win[W_ED2].height && !uimn.win.vert && wind_active() == W_ED2);
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* uimn_init: one edit window; a split left from before stayed, on the last program's lines: the
   next test to go to it crashed. */
TEST(editor_starts_with_one_window)
{
    open_program(S, "sub hi\r\nend sub\r\n");
    wind_other(S, true); /* split, whatever a test before left */
    CHECK(uimn.win.win[W_ED2].height);
    uimn_init(S);
    CHECK(!uimn.win.win[W_ED2].height && !uimn.win.vert);
    done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});
}

/* A native module's interface was edited as a .bas's lines are: the editor changed what the code
   was built against. A line typed over, or one split by Enter, is refused, its box closed. */
TEST(editor_keeps_a_native_module_s_lines)
{
    static const char declare[] = "declare sub beep_twice cdecl ()";
    CHECK(mkdtemp(dir));
    write_qbx("nat.qbx", "declare sub beep_twice cdecl ()\r\n");
    write_file("main.bas", "import nat\r\nnat.beep_twice\r\n");
    S->dbug.mode = 0;
    uimn_init(S);
    uimn_open(S, path_of("main.bas"));
    const uint32_t line = line_of(S, declare), all = uimn.text.all;
    CHECK(line != ~0u);
    CHECK(!typed_over(S, line, "declare sub other cdecl ()"));
    edit_load(S, view_goline(S, line));
    uimn.edline.col = 4;
    host_key_push(0x011B);
    edit_enter(S);
    pkey_flush();
    CHECK(line_of(S, declare) == line && uimn.text.all == all);
    CHECK(!text_unsaved(S, 1));
    done(S, (const char *[]){"main.bas", "nat.qbx", nullptr});
}
