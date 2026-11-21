/* Undo and Redo through the editor's keys. */
#include "edfix.h"
#include "plat/pkey.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "unit.h"

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

static void undo(Session *const S) { fx_key(S, FX_CTRL_Z, PKEY_CTRL); }
static void redo(Session *const S) { fx_key(S, FX_CTRL_Z, PKEY_CTRL | PKEY_SHIFT); }

/* There was no undo: a word typed stayed. Undo on the line takes back a word at a time, then the
   line is as it was. */
TEST(undo_takes_back_a_word_at_a_time)
{
    fx_open_program(S, "sub hi\r\nprint 1\r\nend sub\r\n");
    fx_cursor_on(S, "gfx.hi", "gfx");
    fx_key(S, FX_END, 0);
    fx_type(S, ": print total");
    CHECK(!strcmp(fx_line(), "gfx.hi: print total"));
    undo(S);
    CHECK(!strcmp(fx_line(), "gfx.hi: print "));
    undo(S);
    CHECK(!strcmp(fx_line(), "gfx.hi: "));
    undo(S);
    CHECK(!strcmp(fx_line(), "gfx.hi") && !uimn.edline.dirty);
    redo(S);
    CHECK(!strcmp(fx_line(), "gfx.hi: "));
    fx_done(S, files);
}

/* A line left is one step: Undo puts it back whole, the cursor on it; Redo makes it again. */
TEST(undo_takes_back_a_line_left)
{
    fx_open_program(S, "sub hi\r\nprint 1\r\nend sub\r\n");
    fx_cursor_on(S, "gfx.hi", "gfx");
    fx_key(S, FX_END, 0);
    fx_type(S, ": print 2");
    fx_key(S, FX_UP, 0);
    CHECK(fx_line_of(S, "gfx.hi: print 2") != ~0u);
    undo(S);
    CHECK(fx_line_of(S, "gfx.hi") != ~0u && fx_line_of(S, "gfx.hi: print 2") == ~0u);
    CHECK(uimn.text.first + uimn.ed.line == fx_line_of(S, "gfx.hi"));
    redo(S);
    CHECK(fx_line_of(S, "gfx.hi: print 2") != ~0u);
    fx_done(S, files);
}

/* Ctrl+Y's line comes back where it was; Redo takes it out again. */
TEST(undo_brings_back_a_deleted_line)
{
    fx_open_program(S, "sub hi\r\nprint 1\r\nprint 2\r\nend sub\r\n");
    fx_cursor_on(S, "print 1", "print");
    const uint32_t line = fx_line_of(S, "print 1");
    fx_key(S, FX_CTRL_Y, PKEY_CTRL);
    CHECK(fx_line_of(S, "print 1") == ~0u);
    undo(S);
    CHECK(fx_line_of(S, "print 1") == line && fx_line_of(S, "print 2") == line + 1);
    CHECK(!strcmp(fx_line(), "print 1"));
    redo(S);
    CHECK(fx_line_of(S, "print 1") == ~0u && fx_line_of(S, "print 2") == line);
    fx_done(S, files);
}

/* A SUB typed in the module is its own view; Undo takes it out, and its view. */
TEST(undo_takes_back_a_typed_sub)
{
    fx_open_program(S, "sub hi\r\nprint 1\r\nend sub\r\n");
    fx_cursor_on(S, "gfx.hi", "gfx");
    fx_key(S, FX_END, 0);
    fx_key(S, FX_ENTER, 0);
    fx_type(S, "sub more");
    fx_key(S, FX_ENTER, 0);
    CHECK(fx_line_of(S, "sub more") != ~0u && !strcmp(view_name(uimn.ed.view), "more"));

    for (uint32_t i = 0; i < 4 && fx_line_of(S, "sub more") != ~0u; i++)
        undo(S);

    CHECK(fx_line_of(S, "sub more") == ~0u && fx_line_of(S, "end sub") != ~0u);
    CHECK(!view_at(uimn.ed.view)->kind && fx_line_of(S, "gfx.hi") != ~0u);
    fx_done(S, files);
}

/* A module's line deleted comes back in the module's file, not the main's: saved there. */
TEST(undo_puts_a_line_back_in_its_file)
{
    fx_open_program(S, "sub hi\r\nprint 1\r\nend sub\r\nprint 9\r\n");
    fx_cursor_on(S, "print 9", "print");
    fx_key(S, FX_CTRL_Y, PKEY_CTRL);
    undo(S);
    CHECK(uimn_save(S));
    CHECK(strstr(fx_file_text("gfx.bas"), "print 9"));
    CHECK(!strstr(fx_file_text("main.bas"), "9"));
    fx_done(S, files);
}

/* A change after an undo, typed or not: nothing to redo. */
TEST(undo_a_change_ends_redo)
{
    fx_open_program(S, "sub hi\r\nprint 1\r\nprint 2\r\nend sub\r\n");
    fx_cursor_on(S, "print 1", "print");
    fx_key(S, FX_CTRL_Y, PKEY_CTRL);
    undo(S);
    fx_key(S, FX_END, 0);
    fx_type(S, "0");
    fx_key(S, FX_DOWN, 0);
    redo(S);
    CHECK(fx_line_of(S, "print 10") != ~0u);
    fx_cursor_on(S, "print 10", "print");
    fx_key(S, FX_CTRL_Y, PKEY_CTRL);
    undo(S);
    fx_cursor_on(S, "print 2", "print");
    fx_key(S, FX_CTRL_Y, PKEY_CTRL);
    redo(S);
    CHECK(fx_line_of(S, "print 10") != ~0u && fx_line_of(S, "print 2") == ~0u);
    fx_done(S, files);
}
