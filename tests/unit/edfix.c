/* The editor's tests' fixture: a program of files opened, its lines found. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "edfix.h"
#include "plat/host/host.h"
#include "plat/pkey.h"
#include "ui/edit.h"
#include "ui/linemap.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "unit.h"

static char dir[] = "/tmp/qb_editor_XXXXXX";

/* dir's file name: its path. */
const char *fx_path_of(const char *const name)
{
    static char path[4][128];
    static uint32_t next;
    char *const p = path[next++ % 4];
    snprintf(p, sizeof path[0], "%s/%s", dir, name);
    return p;
}

void fx_write_file(const char *const name, const char *const text)
{
    FILE *const f = fopen(fx_path_of(name), "wb");
    CHECK(f && fputs(text, f) >= 0);
    fclose(f);
}

/* dir's file name's text. */
const char *fx_file_text(const char *const name)
{
    static char text[4096];
    FILE *const f = fopen(fx_path_of(name), "rb");
    const size_t n = f ? fread(text, 1, sizeof text - 1, f) : 0;
    text[n] = 0;

    if (f)
        fclose(f);

    return text;
}

/* main.bas importing gfx.bas, opened. */
void fx_open_program(Session *const S, const char *const gfx)
{
    CHECK(mkdtemp(dir));
    fx_write_file("main.bas", "import gfx\r\ngfx.hi\r\n");
    fx_write_file("gfx.bas", gfx);
    S->dbug.mode = 0; /* debug_test.c's last step */
    uimn_init(S);
    uimn_open(S, fx_path_of("main.bas"));
}

/* The editor left, its files gone, an empty program. */
void fx_done(Session *const S, const char *const *const names)
{
    for (size_t i = 0; names[i]; i++)
        remove(fx_path_of(names[i]));

    rmdir(dir);
    memcpy(dir + sizeof dir - 7, "XXXXXX", 6);
    uimn.on = false;
    S->repl.keep_pre = false;
    modu_clear(&S->modu, "");
    session_new(S);
}

/* The text line, from the whole text's first, that is text, as LIST spells it; ~0 none. */
uint32_t fx_line_of(Session *const S, const char *const text)
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
bool fx_typed_over(Session *const S, const uint32_t line, const char *const text)
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

void fx_retype(Session *const S, const uint32_t line, const char *const text)
{
    CHECK(fx_typed_over(S, line, text));
}

uint16_t fx_col_of(const char *const text, const char *const word)
{
    const char *const at = strstr(text, word);
    CHECK(at != nullptr);
    return at ? (uint16_t)(at - text) : 0;
}

/* The cursor on the first `word` of the text line spelled `text`. */
void fx_cursor_on(Session *const S, const char *const text, const char *const word)
{
    const uint32_t line = fx_line_of(S, text);
    CHECK(line != ~0u);
    edit_load(S, view_goline(S, line));
    uimn.edline.col = fx_col_of(text, word);
}

/* The cursor on the first `word` of the line spelled `text`. */
bool fx_cursor_at(Session *const S, const char *const text, const char *const word)
{
    return uimn.text.first + uimn.ed.line == fx_line_of(S, text) &&
           uimn.edline.col == fx_col_of(text, word);
}
