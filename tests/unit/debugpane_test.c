/* The debug pane at a stop: Watch, Locals and Call stack as the screen shows them. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "plat/host/host.h"
#include "plat/pkey.h"
#include "plat/pscr.h"
#include "session.h"
#include "ui/debugui.h"
#include "ui/debugwatch.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/mouse.h"
#include "ui/screen.h"
#include "ui/text.h"
#include "ui/theme.h"
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
    S->lexr.cols = true; /* the IDE's: the highlighter's columns */
    uimn_init(S);
    them_set(0); /* the attributes a test reads: none set, all 0 */
    uimn_open(S, path);
    uimn_run(S);
    shown(S);
}

static void done(Session *const S)
{
    while (dwat_count()) /* none a later test's, nor its keys */
        dwat_del(S, 0);

    pkey_flush();

    remove(path);
    rmdir(dir);
    memcpy(dir + sizeof dir - 7, "XXXXXX", 6);
    uimn.on = false;
    S->repl.keep_pre = false;
    S->lexr.cols = false;
    session_new(S);
}

/* Screen row row from column c0 to the pane's right border, trailing spaces cut. */
static const char *screen_text(const uint8_t row, const uint8_t c0)
{
    static char s[PSCR_COLS_MAX + 1];
    uint8_t n = 0;

    for (uint8_t c = c0; c < uimn.cols - 1; c++)
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

enum : uint16_t {
    K_ENTER = 0x1C0D,
    K_TAB = 0x0F09,
    K_BAR = 0x3920, /* Space: K_SPACE is SPACE$'s */
    K_UP = 0x4800,
    K_LINE_END = 0x4F00, /* End: K_END is END's */
    K_DOWN = 0x5000,
    K_ARROW_LEFT = 0x4B00, /* K_LEFT is LEFT$'s */
    K_ARROW_RIGHT = 0x4D00,
    K_INS = 0x5200,
    K_DEL = 0x5300,
};

/* Keys pressed in Locals, the last opening a box, OK'd, the screen not drawn again: the row of its
   top, titled title; 0 none. */
static uint8_t box_of(Session *const S, const uint16_t *const keys, const char *const title)
{
    host_key_push(K_ENTER);
    wind_focus(S, W_DBG);
    uimn.tree.pane.focus = 1;

    for (uint32_t i = 0; keys[i]; i++)
        uimn_key(S, keys[i]);

    uint8_t r = 1;

    while (r < 24 && !strstr(screen_text(r, 0), title))
        r++;

    return r < 24 ? r : 0;
}

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

/* Locals: the stopped call's parameters first, then the module's variables it sees, SHARED (once
   left out); Call stack: each frame's view, its arguments (once left out) and line, the stopped one
   marked; an older frame chosen, its call's own values. */
TEST(debug_pane_shows_frames_and_their_locals)
{
    stopped_in(S, fact);
    CHECK(uimn.pane_on);
    CHECK(rows_are(
        "Locals",
        " n% = 3| t% = 30| \xC4\xC4\xC4 shared \xC4\xC4\xC4| r& = 1| s$ = \"hi\""
    ));
    CHECK(rows_are(
        "Call stack",
        "\x10"
        "fact (n% = 3)   line 5| fact (n% = 4)   line 4| module          line 6"
    ));
    pane_keys(S, 2, (const uint16_t[]){K_DOWN, K_ENTER, 0});

    CHECK(rows_are(
        "Locals",
        " n% = 4| t% = 40| \xC4\xC4\xC4 shared \xC4\xC4\xC4| r& = 1| s$ = \"hi\""
    ));
    pane_keys(S, 2, (const uint16_t[]){K_DOWN, K_ENTER, 0});

    CHECK(rows_are("Locals", " r& = 1| s$ = \"hi\"| d# = 2.5"));
    done(S);
}

/* The column of the line between the edit windows and the pane, on the first text row. */
static uint8_t seam(void)
{
    for (uint8_t c = 1; c < uimn.cols; c++)
        if ((uint8_t)host_cell(0, 2, c) == GLYPH_V)
            return c;

    return 0;
}

/* The pane is a quarter of the screen's columns, 30 at least, after a change of size too: 31 with
   its line at 80 columns, 34 at 132. At 132 it kept 80's column 50: 83 columns wide. */
TEST(debug_pane_is_a_quarter_of_the_screen)
{
    stopped_in(S, fact);
    CHECK(seam() == 49);
    uimn_size((PscrSize){132, 50});

    shown(S);
    CHECK(seam() == 98);
    uimn_size(PSCR_80X25);
    shown(S);
    CHECK(seam() == 49);
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

/* The attribute screen row row's column c is drawn in. */
static uint8_t attr_at(const uint8_t row, const uint8_t c)
{
    return (uint8_t)(host_cell(0, row, c) >> 8);
}

/* Locals put the last changed first, so rows moved at each step: they keep their places, a value
   changed at the stop drawn in its own colour. */
TEST(debug_pane_marks_values_changed_at_the_stop)
{
    stopped_in(S, (const char *const[]){"a = 1: b = 2: stop: b = 3: stop", nullptr});

    const uint8_t r = title_row("Locals") + 1, v = uimn.dp_col + 5; /* " a = 1": 1's column */
    CHECK(them_attr(SL_CHG) != them_attr(SL_ED)); /* else the colours below tell nothing */
    CHECK(rows_are("Locals", " a = 1| b = 2"));
    CHECK(attr_at(r, v) == them_attr(SL_ED) && attr_at(r + 1, v) == them_attr(SL_ED));
    uimn_cont(S);
    shown(S);
    CHECK(rows_are("Locals", " a = 1| b = 3"));
    CHECK(attr_at(r, v) == them_attr(SL_ED) && attr_at(r + 1, v) == them_attr(SL_CHG));
    CHECK(attr_at(r + 1, v - 2) == them_attr(SL_ED)); /* the name's, " = " */
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
    CHECK(strstr(screen_text((uint8_t)(uimn.ed.row0 + 3), 0), " t% = 30 "));
    edit_load(S, view_goline(S, uimn.text.first + 1)); /* shared s$ */
    uimn.edline.col = 7;
    shown(S);
    CHECK(strstr(screen_text((uint8_t)(uimn.ed.row0 + 2), 0), " s$ = \"hi\" "));
    done(S);
}

/* Locals named each variable in capitals, not as its program spells it: as declared, else as
   first named; a procedure's variable as its lines spell it, a module's as the module's, a field's
   spelling none's. */
TEST(debug_pane_names_variables_as_declared)
{
    stopped_in(
        S,
        (const char *const[]){
            "type pt",
            "X as integer",
            "end type",
            "dim Total as long",
            "TOTAL = 5",
            "myVar = 1: MYVAR = 2",
            "COUNT = 7: x = 1",
            "bump 3",
            "sub bump (Count)",
            "MyVar = 9",
            "stop",
            "end sub",
            nullptr,
        }

    );
    CHECK(rows_are("Locals", " Count = 3| MyVar = 9"));
    pane_keys(S, 2, (const uint16_t[]){K_DOWN, K_ENTER, 0});

    CHECK(rows_are("Locals", " Total& = 5| myVar = 2| COUNT = 7| x = 1"));
    done(S);
}

/* Locals showed a record or an array as "(record)", "(array)", its values out of reach, and a
   record twice, once as a number 0 (DIM's look-up of its name before its AS): Right or Enter opens
   one under it, Left closes it from any of its rows; an array a page at a time, a row for the
   rest. */
TEST(debug_pane_opens_records_and_arrays)
{
    stopped_in(
        S,
        (const char *const[]){
            "type pt",
            "x as integer",
            "y as integer",
            "end type",
            "dim p as pt",
            "dim a(1 to 12) as integer",
            "p.x = 3",
            "p.y = 4",
            "for i = 1 to 12",
            "a(i) = i * i",
            "next",
            "stop",
            nullptr,
        }

    );
    CHECK(rows_are(
        "Locals",
        " \x10"
        "a%(1 to 12)| \x10p as pt| i = 13"
    ));
    pane_keys(S, 1, (const uint16_t[]){K_DOWN, K_ARROW_RIGHT, 0});

    CHECK(rows_are(
        "Locals",
        " \x10"
        "a%(1 to 12)| \x1Fp as pt|    .x = 3|    .y = 4| i = 13"
    ));
    pane_keys(S, 1, (const uint16_t[]){K_DOWN, K_ARROW_LEFT, K_UP, K_ENTER, 0});

    CHECK(rows_are(
        "Locals",
        " \x1F"
        "a%(1 to 12)|    (1) = 1|    (2) = 4|    (3) = 9|    (4) = 16|    (5) = 25|"
        "    (6) = 36|    (7) = 49|    (8) = 64|    (9) = 81"
    ));
    uint16_t keys[12] = {0};

    for (uint32_t i = 0; i < 11; i++)
        keys[i] = K_DOWN;

    pane_keys(S, 1, keys);

    CHECK(rows_are(
        "Locals",
        "    (2) = 4|    (3) = 9|    (4) = 16|    (5) = 25|    (6) = 36|    (7) = 49|"
        "    (8) = 64|    (9) = 81|    (10) = 100|    ... (11) to (12)"
    ));
    pane_keys(S, 1, (const uint16_t[]){K_ENTER, K_DOWN, K_DOWN, K_DOWN, 0});

    CHECK(rows_are(
        "Locals",
        "    (5) = 25|    (6) = 36|    (7) = 49|    (8) = 64|    (9) = 81|    (10) = 100|"
        "    (11) = 121|    (12) = 144| \x10p as pt| i = 13"
    ));
    pane_keys(S, 1, (const uint16_t[]){K_UP, K_UP, K_UP, K_UP, K_ARROW_LEFT, 0});

    CHECK(rows_are(
        "Locals",
        " \x10"
        "a%(1 to 12)| \x10p as pt| i = 13"
    ));
    done(S);
}

/* An open record's record field opens under it, deeper; an array's elements are in the run's
   order, the first subscript the fastest; a fixed string's value is its bytes. */
TEST(debug_pane_opens_nested_records_and_arrays)
{
    stopped_in(
        S,
        (const char *const[]){
            "type pt",
            "x as integer",
            "y as integer",
            "end type",
            "type box",
            "title as string * 4",
            "corner as pt",
            "end type",
            "dim b as box",
            "dim g(1, 1) as integer",
            "b.title = \"ab\"",
            "b.corner.x = 7",
            "g(1, 0) = 5",
            "stop",
            nullptr,
        }

    );
    pane_keys(S, 1, (const uint16_t[]){K_DOWN, K_ARROW_RIGHT, K_DOWN, K_DOWN, K_ARROW_RIGHT, 0});

    CHECK(rows_are(
        "Locals",
        " \x10g%(0 to 1, 0 to 1)| \x1F"
        "b as box|    .title = \"ab  \"|    \x1F.corner as pt|"
        "       .x = 7|       .y = 0"
    ));
    pane_keys(S, 1, (const uint16_t[]){K_DOWN, K_ARROW_LEFT, K_ARROW_LEFT, K_UP, K_ARROW_RIGHT, 0});

    CHECK(rows_are(
        "Locals",
        " \x1Fg%(0 to 1, 0 to 1)|    (0, 0) = 0|    (1, 0) = 5|    (0, 1) = 0|    (1, 1) = 0|"
        " \x10"
        "b as box"
    ));
    done(S);
}

/* An array of records' elements open as records; a record's array field opens as an array, a
   STRING * n one's elements their n bytes each. */
TEST(debug_pane_opens_record_arrays_and_array_fields)
{
    stopped_in(
        S,
        (const char *const[]){
            "type pt",
            "x as integer",
            "y as integer",
            "end type",
            "type poly",
            "tags(1) as string * 2",
            "end type",
            "dim ps(1) as pt",
            "dim q as poly",
            "ps(1).y = 6",
            "q.tags(1) = \"hi\"",
            "stop",
            nullptr,
        }

    );
    pane_keys(S, 1, (const uint16_t[]){K_ARROW_RIGHT, K_DOWN, K_DOWN, K_ARROW_RIGHT, 0});

    CHECK(rows_are(
        "Locals",
        " \x1Fps(0 to 1)|    \x10(0) as pt|    \x1F(1) as pt|       .x = 0|       .y = 6|"
        " \x10q as poly"
    ));
    pane_keys(
        S,
        1,
        (const uint16_t[]){K_DOWN, K_DOWN, K_DOWN, K_ARROW_RIGHT, K_DOWN, K_ARROW_RIGHT, 0}

    );

    CHECK(rows_are(
        "Locals",
        " \x1Fps(0 to 1)|    \x10(0) as pt|    \x1F(1) as pt|       .x = 0|       .y = 6|"
        " \x1Fq as poly|    \x1F.tags(0 to 1)|       (0) = \"  \"|       (1) = \"hi\""
    ));
    done(S);
}

/* A row is open by its expression: b.corner opened leaves c.corner, its field's name too, shut. */
TEST(debug_pane_opens_a_field_of_one_record)
{
    stopped_in(
        S,
        (const char *const[]){
            "type pt",
            "x as integer",
            "end type",
            "type box",
            "corner as pt",
            "end type",
            "dim b as box",
            "dim c as box",
            "b.corner.x = 1",
            "c.corner.x = 2",
            "stop",
            nullptr,
        }

    );
    pane_keys(
        S,
        1,
        (const uint16_t[]){K_ARROW_RIGHT, K_DOWN, K_ARROW_RIGHT, K_DOWN, K_DOWN, K_ARROW_RIGHT, 0}

    );

    CHECK(rows_are(
        "Locals",
        " \x1F"
        "b as box|    \x1F.corner as pt|       .x = 1| \x1F"
        "c as box|    \x10.corner as pt"
    ));
    done(S);
}

/* Past 16 frames Call stack ended at the 16th, the rest unknown: a row says how many more, and
   which is last. An argument's string is its first characters; an array's its name. */
TEST(debug_pane_counts_frames_past_its_rows)
{
    stopped_in(
        S,
        (const char *const[]){
            "declare sub deep (n%, s$, a())",
            "dim a(2)",
            "deep 20, \"a long string\", a()",
            "sub deep (n%, s$, a())",
            "if n% = 1 then stop",
            "deep n% - 1, s$, a()",
            "end sub",
            nullptr,
        }

    );
    uint16_t keys[17] = {0};

    for (uint32_t i = 0; i < 16; i++)
        keys[i] = K_DOWN;

    uimn.dp_col = DP_COLMIN; /* the pane widened, its rows whole */
    pane_keys(S, 2, keys);

    CHECK(rows_are(
        "Call stack",
        " deep (n% = 13, s$ = \"a long str\", a()) line 3| deep (n% = 14, s$ = \"a long str\", "
        "a()) line 3|"
        " deep (n% = 15, s$ = \"a long str\", a()) line 3| deep (n% = 16, s$ = \"a long str\", "
        "a()) line 3|"
        " \x19 5 more, module last"
    ));
    const uint8_t more = (uint8_t)(title_row("Call stack") + 5);
    uimn_size(PSCR_80X25); /* the windows where the mouse finds them */
    uimn.mouse = (UiMouse){.event = MS_PRESS, .row = more, .col = (uint8_t)(uimn.dp_col + 2)};

    uimn_key(S, KEY_MOUSE); /* a press on the row past the frames: no frame chosen */
    shown(S);
    CHECK(rows_are(
        "Locals",
        " n% = 1| s$ = \"a long string\"| \x10"
        "a(0 to 2)"
    ));
    done(S);
}

/* Add Watch asked for an expression typed whole: Shift+F9 in the editor, and Ins in Watch, fill in
   the name at the editor's cursor. */
TEST(debug_pane_watches_the_name_at_the_cursor)
{
    stopped_in(S, fact);
    edit_load(S, view_goline(S, uimn.text.first + 2)); /* t% = n% * 10 */
    uimn.edline.col = 0;
    typed("");
    uimn_key(S, (uint16_t)(SCAN_SHIFT_F1 + 8) << 8); /* Shift+F9 */
    shown(S);

    CHECK(rows_are("Watch", " t% = 30"));
    uimn.edline.col = 5;
    typed("");
    pane_keys(S, 0, (const uint16_t[]){K_INS, 0});

    CHECK(rows_are("Watch", " t% = 30| n% = 3"));
    done(S);
}

/* Enter on a value in Locals: a new one asked, its current one filled in, set as the program would
   (path = value) in the frame's scope; an error leaves it. The stopped call's and the module's are
   set; an older call's, not the probe's to reach, are not. */
TEST(debug_pane_sets_a_value)
{
    stopped_in(S, fact);
    host_key_push(K_LINE_END); /* past the 30 filled in */
    typed("0");
    pane_keys(S, 1, (const uint16_t[]){K_DOWN, K_ENTER, 0});

    CHECK(rows_are(
        "Locals",
        " n% = 3| t% = 300| \xC4\xC4\xC4 shared \xC4\xC4\xC4| r& = 1| s$ = \"hi\""
    ));
    typed("\"yo\" + s$");
    pane_keys(S, 1, (const uint16_t[]){K_DOWN, K_DOWN, K_DOWN, K_ENTER, 0});

    CHECK(rows_are(
        "Locals",
        " n% = 3| t% = 300| \xC4\xC4\xC4 shared \xC4\xC4\xC4| r& = 1| s$ = \"yohi\""
    ));
    typed("\"x\"");
    host_key_push(K_ENTER); /* the error's box */
    pane_keys(S, 1, (const uint16_t[]){K_UP, K_UP, K_UP, K_UP, K_ENTER, 0});

    CHECK(rows_are(
        "Locals",
        " n% = 3| t% = 300| \xC4\xC4\xC4 shared \xC4\xC4\xC4| r& = 1| s$ = \"yohi\""
    ));
    pane_keys(S, 2, (const uint16_t[]){K_DOWN, K_ENTER, 0});

    typed("7"); /* the box saying it is not set takes them, the 7 doing nothing */
    pane_keys(S, 1, (const uint16_t[]){K_UP, K_ENTER, 0});

    CHECK(rows_are(
        "Locals",
        " n% = 4| t% = 40| \xC4\xC4\xC4 shared \xC4\xC4\xC4| r& = 1| s$ = \"yohi\""
    ));
    pane_keys(S, 2, (const uint16_t[]){K_UP, K_ENTER, 0});

    CHECK(rows_are(
        "Locals",
        " n% = 3| t% = 300| \xC4\xC4\xC4 shared \xC4\xC4\xC4| r& = 1| s$ = \"yohi\""
    ));
    pane_keys(S, 2, (const uint16_t[]){K_DOWN, K_DOWN, K_ENTER, 0});

    typed("d# * 2");
    pane_keys(S, 1, (const uint16_t[]){K_DOWN, K_DOWN, K_ENTER, 0});

    CHECK(rows_are("Locals", " r& = 1| s$ = \"yohi\"| d# = 5"));
    done(S);
}

/* Enter on a string longer than its row: a box of it whole, 64 characters a line, titled by its
   expression and length; a '|' a line drawn, not the box's next line; past 8 lines, "...". */
TEST(debug_pane_shows_a_long_string_whole)
{
    stopped_in(
        S,
        (const char *const[]){
            "greet$ = \"a rather long string that will not fit in the pane at all|nor on one "
            "line\"",
            "big$ = string$(600, \"x\")",
            "stop",
            nullptr,
        }

    );
    const uint8_t r = box_of(S, (const uint16_t[]){K_ENTER, 0}, "greet$, 73 characters");

    CHECK(r);
    CHECK(strstr(
        screen_text(r + 2, 0),
        "  a rather long string that will not fit in the pane at all\xB3nor on  "
    ));
    CHECK(strstr(screen_text(r + 3, 0), "   one line "));
    shown(S);
    const uint8_t b = box_of(S, (const uint16_t[]){K_DOWN, K_ENTER, 0}, "big$, 600 characters");

    CHECK(b);
    CHECK(strstr(screen_text(b + 9, 0), "xxxxxxxxxxxxxx...  "));
    done(S);
}

static const char *const loop[] = {
    "stop",
    "for i = 1 to 10",
    "y = i",
    "x = i * 2",
    "next",
    "stop",
    nullptr,
};

/* Break When's box (Ctrl+F9) given expr, its boxes ticked: When it changes, Only at this line. */
static void break_when(
    Session *const S,
    const char *const expr,
    const bool changes,
    const bool here
)
{
    for (uint32_t i = 0; expr[i]; i++)
        host_key_push((uint8_t)expr[i]);

    host_key_push(K_TAB);

    if (changes)
        host_key_push(K_BAR);

    host_key_push(K_TAB);

    if (here)
        host_key_push(K_BAR);

    host_key_push(K_ENTER);
    wind_focus(S, wind_active());
    uimn_key(S, (uint16_t)(SCAN_CTRL_F1 + 8) << 8);
    shown(S);
}

/* The cursor on the edit window's line line (0 its view's first). */
static void cursor_to(Session *const S, const uint32_t line)
{
    wind_focus(S, wind_active());
    edit_load(S, view_goline(S, uimn.text.first + line));
    uimn.edline.col = 0;
}

static void continued(Session *const S)
{
    uimn_cont(S);
    shown(S);
}

/* Break When, true: the run stops once its expression turns true, not again while it stays so;
   Watch shows it marked. */
TEST(debug_pane_breaks_when_true)
{
    stopped_in(S, loop);
    break_when(S, "x > 7", false, false);
    continued(S);

    CHECK(rows_are("Locals", " i = 4| y = 4| x = 8"));
    CHECK(rows_are("Watch", " \x04x > 7 = -1"));
    continued(S);

    CHECK(rows_are("Locals", " i = 11| y = 10| x = 20"));
    pane_keys(S, 0, (const uint16_t[]){K_DEL, 0});

    done(S);
}

/* Break When, changes: the run stops at each change of its value, from the one the stop shows,
   a value set there too. */
TEST(debug_pane_breaks_when_a_value_changes)
{
    stopped_in(S, loop);
    break_when(S, "x", true, false);
    continued(S);

    CHECK(rows_are("Locals", " i = 1| y = 1| x = 2"));
    typed("100");
    pane_keys(S, 1, (const uint16_t[]){K_DOWN, K_DOWN, K_ENTER, 0});

    continued(S);

    CHECK(rows_are("Locals", " i = 2| y = 2| x = 4"));
    pane_keys(S, 0, (const uint16_t[]){K_DEL, 0});

    continued(S);

    CHECK(rows_are("Locals", " i = 11| y = 10| x = 20"));
    done(S);
}

/* Break When, only at this line: a breakpoint there that stops when its expression is true, its
   line in its own colour; it keeps to its line as other breakpoints come and go before it, and
   goes with its breakpoint. */
TEST(debug_pane_breaks_at_a_line_when_true)
{
    stopped_in(S, loop);
    cursor_to(S, 3);
    break_when(S, "i = 6", false, true);
    const uint8_t row = (uint8_t)(uimn.ed.row0 + 3 - uimn.ed.top);

    CHECK(them_attr(SL_CBPL) != them_attr(SL_BPL));
    CHECK(attr_at(row, (uint8_t)(uimn.ed.x + 2)) == them_attr(SL_CBPL)); /* x's: SL_OCC's */
    cursor_to(S, 2);
    udbg_bp(S); /* a breakpoint before it */
    continued(S);

    CHECK(rows_are("Locals", " i = 1| y = 0| x = 0"));
    cursor_to(S, 2);
    udbg_bp(S); /* gone again */
    continued(S);

    CHECK(rows_are("Locals", " i = 6| y = 6| x = 10"));
    pane_keys(S, 0, (const uint16_t[]){K_DEL, 0}); /* its row in Watch: its breakpoint too */

    CHECK(!text_mark(S, 3));
    continued(S);

    CHECK(rows_are("Locals", " i = 11| y = 10| x = 20"));
    CHECK(rows_are("Watch", ""));
    done(S);
}

/* A Break When at a line goes with its breakpoint: F9 off, its line deleted, all cleared. */
TEST(debug_pane_break_when_goes_with_its_breakpoint)
{
    stopped_in(S, loop);
    cursor_to(S, 3);
    break_when(S, "i = 6", false, true);
    udbg_bp(S);
    shown(S);

    CHECK(rows_are("Watch", ""));
    break_when(S, "i = 6", false, true);
    text_delete(S, 3);
    shown(S);

    CHECK(rows_are("Watch", ""));
    cursor_to(S, 2);
    break_when(S, "i = 6", false, true);
    udbg_bpclear(S);
    shown(S);

    CHECK(rows_are("Watch", ""));
    done(S);
}

/* Break When set in a SUB: PRINTed only while it runs, in its scope, each call's turn to true a
   stop; at a stop elsewhere, not. */
TEST(debug_pane_breaks_in_its_procedure)
{
    stopped_in(
        S,
        (const char *const[]){
            "declare sub bump (n)",
            "t = 99",
            "for i = 1 to 3",
            "bump i",
            "next",
            "stop",
            "sub bump (n)",
            "if n = 1 then stop",
            "t = n * 10",
            "end sub",
            nullptr,
        }

    );
    break_when(S, "t > 15", false, false);
    continued(S);

    CHECK(rows_are("Locals", " n = 2| t = 20"));
    continued(S); /* a new call's t: 0, then true again */

    CHECK(rows_are("Locals", " n = 3| t = 30"));
    continued(S);

    CHECK(rows_are("Locals", " t = 99| i = 4"));
    CHECK(rows_are("Watch", " \x04t > 15 = (not here)"));
    done(S);
}

/* The edit window's screen row of its line line (its view's, 0 the first). */
static uint8_t row_of_line(const uint32_t line)
{
    return (uint8_t)(uimn.ed.row0 + line - uimn.ed.top);
}

/* The edit window's text on its line line, inside its borders, trailing spaces cut. */
static const char *window_line(const uint32_t line)
{
    static char s[PSCR_COLS_MAX + 1];
    uint8_t n = 0;

    for (uint8_t c = 0; c < uimn.ed.width; c++)
        s[n++] = (char)host_cell(0, row_of_line(line), (uint8_t)(uimn.ed.x + 1 + c));

    while (n && s[n - 1] == ' ')
        n--;

    s[n] = 0;
    return s;
}

static bool line_is(const uint32_t line, const char *const want)
{
    const char *const got = window_line(line);

    if (strcmp(got, want))
        fprintf(stderr, "  line %u: \"%s\"\n  want: \"%s\"\n", line, got, want);

    return !strcmp(got, want);
}

/* At a stop the stopped call's lines had no values: each now shows those of the variables it
   names after its end, once each, as Locals names them; an older frame chosen, its call's. */
TEST(debug_pane_values_beside_the_code)
{
    stopped_in(S, fact);
    CHECK(line_is(0, "sub fact (n%)  n% = 3"));
    CHECK(line_is(1, "shared s$  s$ = \"hi\""));
    CHECK(line_is(2, "t% = n% * 10  t% = 30  n% = 3"));
    CHECK(line_is(3, "if n% > 3 then fact n% - 1  n% = 3"));
    CHECK(line_is(4, "stop"));
    CHECK(line_is(5, "r& = r& * n%  r& = 1  n% = 3"));
    pane_keys(S, 2, (const uint16_t[]){K_DOWN, K_ENTER, 0});

    CHECK(line_is(2, "t% = n% * 10  t% = 40  n% = 4"));
    done(S);
}

/* A record or an array shows nothing; nor a name that is no variable of the frame. */
TEST(debug_pane_values_of_numbers_and_strings_only)
{
    stopped_in(
        S,
        (const char *const[]){
            "type pt",
            "x as integer",
            "end type",
            "dim p as pt",
            "dim a(2)",
            "k = 7",
            "p.x = k: a(1) = k: q = r",
            "stop",
            nullptr,
        }

    );
    CHECK(line_is(3, "dim p as pt"));
    CHECK(line_is(6, "p.x = k: a(1) = k: q = r  k = 7  q = 0  r = 0"));
    done(S);
}

/* The values in SL_HINT, the code as it was; gone once the text changes. */
TEST(debug_pane_values_dim_and_gone_with_a_change)
{
    stopped_in(S, (const char *const[]){"a = 1: b = 2: stop: b = 3: stop", "c = 1", nullptr});

    them_qbasic();
    shown(S);
    CHECK(them_attr(SL_HINT) != them_attr(SL_ED));
    const char *const code = "a = 1: b = 2: stop: b = 3: stop";
    const uint8_t at = (uint8_t)(uimn.ed.x + 1 + strlen(code) + 2), r = row_of_line(0);
    CHECK(line_is(0, "a = 1: b = 2: stop: b = 3: stop  a = 1  b = 2"));
    CHECK(attr_at(r, at) == them_attr(SL_HINT) && attr_at(r, at - 3) != them_attr(SL_HINT));
    continued(S);
    CHECK(line_is(0, "a = 1: b = 2: stop: b = 3: stop  a = 1  b = 3"));
    cursor_to(S, 1);
    uimn_key(S, K_LINE_END);
    uimn_key(S, '2');
    uimn_key(S, K_UP);
    shown(S);
    CHECK(line_is(0, code));
    done(S);
}

/* The values cut at the window's edges, the text scrolled sideways: none spills past the right
   one into the next row. */
TEST(debug_pane_values_cut_at_the_window)
{
    static char line[200];
    const char *const code = "a1 = 1: s$ = string$(70, \"x\") ' ";
    memset(line, 0, sizeof line);
    strcpy(line, code);
    memset(line + strlen(code), '-', 30);
    stopped_in(S, (const char *const[]){line, "stop", nullptr});

    const uint32_t end = (uint32_t)strlen(line) + 2; /* the values' first column */
    const uint8_t r = row_of_line(0);
    const uint16_t border = host_cell(0, r + 1, uimn.ed.x);
    const uint32_t s_at = end + 8;                       /* s$'s value, after "a1 = 1  " */
    uimn.ed.left = (uint16_t)(s_at + 4 - uimn.ed.width); /* "s$ =" shows */
    shown(S);
    const char *const shown_row = window_line(0);
    CHECK(!strcmp(shown_row + strlen(shown_row) - 4, "s$ ="));
    CHECK(host_cell(0, r + 1, uimn.ed.x) == border);
    uimn.ed.left = (uint16_t)(end + 2); /* "a1" cut */
    shown(S);
    CHECK(!strncmp(window_line(0), " = 1  s$ = \"xxx", 15));
    done(S);
}

/* The edit window on another view than the stopped frame's: its lines show nothing. */
TEST(debug_pane_values_in_the_frame_s_view_only)
{
    stopped_in(S, fact);
    edit_load(S, view_goline(S, 4)); /* the module's r& = 1 */
    shown(S);
    CHECK(line_is(4, "r& = 1"));
    done(S);
}
