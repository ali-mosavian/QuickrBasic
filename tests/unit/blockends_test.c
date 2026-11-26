/* Enter after a block's opener puts its end in below (ui/blockends.h), in each dialect. */
#include "edfix.h"
#include "ui/linemap.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "unit.h"

#define DIALECT "' OPTION DIALECT QUICKR\n"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

/* Whole text line line as listed, its indent cut, 0-ended ("" past the text); its indent to
   *at. */
static const char *text_at(Session *const S, const uint32_t line, uint16_t *const at)
{
    static char t[LNBUF];
    *at = 0;

    if (line >= uimn.text.all)
        return "";

    const uint32_t n = text_record(S, lmap_off(S, line), &uimn.build);
    uint16_t i = 0;

    while (i < n && uimn.build.text[i] == ' ')
        i++;

    memcpy(t, uimn.build.text + i, n - i);
    t[n - i] = 0;
    *at = i;
    return t;
}

/* Typed on a new line below the line spelled after, then Enter: the cursor a step in on an
   empty line, below it end at the typed line's indent, and the line after that next. */
static bool ends(
    Session *const S,
    const char *const after,
    const char *const typed,
    const char *const end,
    const char *const next
)
{
    fx_cursor_on(S, after, after[0] == ' ' ? "" : after);
    fx_key(S, FX_END, 0);
    fx_key(S, FX_ENTER, 0);
    fx_type(S, typed);
    fx_key(S, FX_ENTER, 0);
    const uint32_t cur = uimn.text.first + uimn.ed.line;
    uint16_t open, at, past;
    text_at(S, cur - 1, &open);
    const bool body = uimn.edline.col == open + 4 && uimn.edline.len == open + 4;
    const bool closed = !strcmp(text_at(S, cur + 1, &at), end) && at == open;
    const bool rest = !strcmp(text_at(S, cur + 2, &past), next);
    return body && closed && rest;
}

/* Enter after FOR gave an empty line and no NEXT: each block's end had to be typed. Listed in
   the editor's keywords' case. */
TEST(blockends_qbasic)
{
    static const char *const openers[][2] = {
        {"for i = 1 to 3", "next"},
        {"if x then", "end if"},
        {"do", "loop"},
        {"while x", "wend"},
        {"select case x", "end select"},
        {"type pt", "end type"},
    };

    for (uint32_t i = 0; i < countof(openers); i++) {
        fx_open_program(S, "x = 1\r\nPRINT x\r\n");
        CHECK(ends(S, "x = 1", openers[i][0], openers[i][1], "print x"));
        fx_done(S, files);
    }
}

/* The same in Quickr: its ends as it spells them; a method's, a block lambda's, its brackets
   after it. */
TEST(blockends_quickr)
{
    static const char *const openers[][2] = {
        {"for i in 0..9", "next"},
        {"if x > 0 then", "end if"},
        {"while x > 0", "wend"},
        {"do", "loop"},
        {"select case x", "end select"},
        {"function f() as int", "end function"},
        {"test \"t\"", "end test"},
        {"enum E", "end enum"},
        {"let g = fn(v as int) as int", "end fn"},
    };

    for (uint32_t i = 0; i < countof(openers); i++) {
        fx_open_program(S, "");
        fx_write_file("main.bas", DIALECT "let x = 1\nprint x\n");
        uimn_open(S, fx_path_of("main.bas"));
        CHECK(ends(S, "let x = 1", openers[i][0], openers[i][1], "print x"));
        fx_done(S, files);
    }

    fx_open_program(S, "");
    fx_write_file("main.bas", DIALECT "type P\n    x as int\nend type\nprint 1\n");
    uimn_open(S, fx_path_of("main.bas"));
    CHECK(ends(S, "    x as int", "fn half(self) as int", "end fn", "end type"));
    fx_done(S, files);

    fx_open_program(S, "");
    fx_write_file("main.bas", DIALECT "let x = 1\nprint x\n");
    uimn_open(S, fx_path_of("main.bas"));
    CHECK(ends(S, "let x = 1", "print apply(fn(v as int)", "end fn)", "print x"));
    fx_done(S, files);
}

/* Not put in: a block ended below already, Enter inside the line, a line's IF. */
TEST(blockends_only_when_missing)
{
    fx_open_program(S, "FOR i = 1 TO 3\r\n    PRINT i\r\nNEXT\r\n");
    fx_cursor_on(S, "for i = 1 to 3", "for");
    fx_key(S, FX_END, 0);
    fx_key(S, FX_ENTER, 0);
    uint16_t at;
    CHECK(!strcmp(text_at(S, uimn.text.first + uimn.ed.line + 1, &at), "print i"));
    fx_done(S, files);

    fx_open_program(S, "x = 1\r\n");
    fx_cursor_on(S, "x = 1", "x");
    fx_key(S, FX_END, 0);
    fx_key(S, FX_ENTER, 0);
    fx_type(S, "for i = 1 to 3");
    fx_key(S, FX_LEFT, 0);
    fx_key(S, FX_ENTER, 0);
    CHECK(fx_line_of(S, "next") == ~0u);
    fx_done(S, files);

    fx_open_program(S, "x = 1\r\n");
    fx_cursor_on(S, "x = 1", "x");
    fx_key(S, FX_END, 0);
    fx_key(S, FX_ENTER, 0);
    fx_type(S, "if x then print 1");
    fx_key(S, FX_ENTER, 0);
    CHECK(fx_line_of(S, "end if") == ~0u);
    fx_done(S, files);
}

/* A FOR typed inside another, whose NEXT is at the outer's column: the inner one's put in. */
TEST(blockends_an_outer_end_is_not_ours)
{
    fx_open_program(S, "FOR j = 1 TO 2\r\n    PRINT j\r\nNEXT\r\n");
    CHECK(ends(S, "    print j", "for i = 1 to 3", "next", "next"));
    fx_done(S, files);
}
