/* The debug pane at a stop: Watch, Locals and Call stack as the screen shows them. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "plat/host/host.h"
#include "plat/pkey.h"
#include "plat/pscr.h"
#include "session.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/screen.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "ui/window.h"
#include "unit.h"

static char dir[] = "/tmp/qb_dpane_XXXXXX";
static char path[64];

static void shown(Session *const S)
{
    draw_all(S);
    scrn_flip();
}

/* main.bas, its lines, opened in the editor and run to its first stop. */
static void stopped_in(Session *const S, const char *const *const lines)
{
    CHECK(mkdtemp(dir));
    snprintf(path, sizeof path, "%s/main.bas", dir);
    FILE *const f = fopen(path, "wb");

    for (uint32_t i = 0; lines[i]; i++)
        CHECK(f && fprintf(f, "%s\r\n", lines[i]) > 0);

    fclose(f);
    S->dbug.mode = 0;
    uimn_init(S);
    uimn_open(S, path);
    uimn_run(S);
    shown(S);
}

static void done(Session *const S)
{
    remove(path);
    rmdir(dir);
    memcpy(dir + sizeof dir - 7, "XXXXXX", 6);
    uimn.on = false;
    S->repl.keep_pre = false;
    session_new(S);
}

/* Screen row row from column c0 to the pane's right border, trailing spaces cut. */
static const char *screen_text(const uint8_t row, const uint8_t c0)
{
    static char s[PSCR_COLS + 1];
    uint8_t n = 0;

    for (uint8_t c = c0; c < PSCR_COLS - 1; c++)
        s[n++] = (char)host_cell(0, row, c);

    while (n && s[n - 1] == ' ')
        n--;

    s[n] = 0;
    return s;
}

/* A pane's row as shown: from its marker column on (a frame's arrow, or a space). */
static const char *pane_row(const uint8_t row) { return screen_text(row, uimn.dp_col); }

/* The row of the pane's window title, 0 none. */
static uint8_t title_row(const char *const title)
{
    for (uint8_t r = 1; r < 24; r++)
        if (strstr(pane_row(r), title))
            return r;

    return 0;
}

/* Window title's rows, joined by '|' with their markers, its empty rows left out. */
static const char *rows_of(const char *const title)
{
    static char s[1024];
    const char *const titles[] = {"Watch", "Locals", "Call stack"};

    const uint8_t r0 = title_row(title);
    uint8_t r1 = 24;

    for (uint32_t i = 0; i < countof(titles); i++) {
        const uint8_t r = title_row(titles[i]);

        if (r > r0 && r < r1)
            r1 = r;
    }

    s[0] = 0;

    for (uint8_t r = r0 + 1; r0 && r < r1; r++) {
        const char *const t = pane_row(r);

        if (!*t)
            continue;
        if (*s)
            strcat(s, "|");

        strcat(s, t);
    }

    return s;
}

static bool rows_are(const char *const title, const char *const want)
{
    const char *const got = rows_of(title);

    if (strcmp(got, want))
        fprintf(stderr, "  %s: \"%s\"\n  want: \"%s\"\n", title, got, want);

    return !strcmp(got, want);
}

/* Keys pressed in the pane's window w (0 Watch, 1 Locals, 2 Call stack), then shown. */
static void pane_keys(Session *const S, const uint8_t w, const uint16_t *const keys)
{
    wind_focus(S, W_DBG);
    uimn.tree.pane.focus = w;

    for (uint32_t i = 0; keys[i]; i++)
        uimn_key(S, keys[i]);

    shown(S);
}

/* Text typed into the dialog the next key opens, then Enter. */
static void typed(const char *const text)
{
    for (uint32_t i = 0; text[i]; i++)
        host_key_push((uint8_t)text[i]);

    host_key_push(0x1C0D);
}

enum : uint16_t { K_ENTER = 0x1C0D, K_DOWN = 0x5000, K_INS = 0x5200, K_DEL = 0x5300 };

static const char *const fact[] = {
    "declare sub fact (n%)",
    "dim shared r&",
    "s$ = \"hi\"",
    "d# = 2.5#",
    "r& = 1",
    "fact 4",
    "print r&",
    "end",
    "sub fact (n%)",
    "shared s$",
    "t% = n% * 10",
    "if n% > 3 then fact n% - 1",
    "stop",
    "r& = r& * n%",
    "end sub",
    nullptr,
};

/* Locals: the stopped call's parameters first; Call stack: each frame's view and line, the
   stopped one marked; an older frame chosen, its call's own values. */
TEST(debug_pane_shows_frames_and_their_locals)
{
    stopped_in(S, fact);
    CHECK(uimn.pane_on);
    CHECK(rows_are("Locals", " N% = 3| T% = 30"));
    CHECK(rows_are(
        "Call stack",
        "\x10"
        "fact            line 5| fact            line 4| module          line 6"
    ));
    pane_keys(S, 2, (const uint16_t[]){K_DOWN, K_ENTER, 0});

    CHECK(rows_are("Locals", " N% = 4| T% = 40"));
    pane_keys(S, 2, (const uint16_t[]){K_DOWN, K_ENTER, 0});

    CHECK(rows_are("Locals", " R& = 1| S$ = \"hi\"| D# = 2.5"));
    done(S);
}

/* Watch: Ins asks for an expression, PRINTed in the stopped call's scope; an error is its message;
   Del drops the selected one. */
TEST(debug_pane_watches_expressions)
{
    stopped_in(S, fact);
    typed("n% * 2");
    pane_keys(S, 0, (const uint16_t[]){K_INS, 0});

    typed("1 / 0");
    pane_keys(S, 0, (const uint16_t[]){K_INS, 0});

    CHECK(rows_are("Watch", " n% * 2 = 6| 1 / 0 = Division by zero"));
    pane_keys(S, 0, (const uint16_t[]){K_DEL, 0});

    CHECK(rows_are("Watch", " n% * 2 = 6"));
    pane_keys(S, 0, (const uint16_t[]){K_DEL, 0});

    CHECK(rows_are("Watch", ""));
    done(S);
}

/* Locals: the last changed first. b, changed after the first stop, came before a. */
TEST(debug_pane_puts_the_last_changed_first)
{
    stopped_in(S, (const char *const[]){"a = 1: b = 2: stop: b = 3: stop", nullptr});

    CHECK(rows_are("Locals", " A = 1| B = 2"));
    uimn_cont(S);
    shown(S);
    CHECK(rows_are("Locals", " B = 3| A = 1"));
    done(S);
}

/* The cursor on a variable: its Locals row under it; a SHARED module one's too. */
TEST(debug_pane_tooltip_shows_a_variable)
{
    stopped_in(S, fact);
    edit_load(S, view_goline(S, uimn.text.first + 2)); /* t% = n% * 10 */
    uimn.edline.col = 0;
    uimn.tip_mouse = false;
    shown(S);
    CHECK(strstr(screen_text((uint8_t)(uimn.ed.row0 + 3), 0), " T% = 30 "));
    edit_load(S, view_goline(S, uimn.text.first + 1)); /* shared s$ */
    uimn.edline.col = 7;
    shown(S);
    CHECK(strstr(screen_text((uint8_t)(uimn.ed.row0 + 2), 0), " S$ = \"hi\" "));
    done(S);
}
