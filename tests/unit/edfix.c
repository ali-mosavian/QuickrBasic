/* The editor's tests' fixture: a program of files opened, its lines found, keys run as the key
   loop runs them. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "edfix.h"
#include "plat/host/host.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/screen.h"
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

void fx_write_qbx(const char *const name, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    uint8_t head[20] = {'Q', 'B', 'X', 0x1A, 1, 0, 1, 0, 0, 0, 0, 0, sizeof head};

    memcpy(head + 16, &n, sizeof n);
    FILE *const f = fopen(fx_path_of(name), "wb");
    CHECK(f && fwrite(head, 1, sizeof head, f) == sizeof head && fputs(text, f) >= 0);
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
    S->lexr.cols = true; /* the IDE's, uimn_front_end's regions: the error columns */
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
    S->lexr.cols = false;
    S->dbug.emit = false; /* uimn_init's debugger out: as boot left it */
    S->dbug.hooks = nullptr;
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
   answer, false a syntax error (the line kept as typed). */
bool fx_typed_over(Session *const S, const uint32_t line, const char *const text)
{
    CHECK(line < uimn.text.all);

    if (line >= uimn.text.all)
        return false;

    edit_load(S, view_goline(S, line));
    uimn.edline.len = (uint16_t)strlen(text);
    memcpy(edit_lb_need(S, &uimn.edline, uimn.edline.len), text, uimn.edline.len);
    uimn.edline.dirty = true;
    return edit_commit(S);
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

void fx_key(Session *const S, const uint16_t key, const uint8_t flags)
{
    uimn.keyflags = flags;
    uimn_command(S, key);
    uimn.keyflags = 0;
}

void fx_type(Session *const S, const char *const text)
{
    for (size_t i = 0; text[i]; i++)
        fx_key(S, (uint8_t)text[i], 0);
}

const char *fx_line(void)
{
    static char line[256];
    const uint16_t n = uimn.edline.len < sizeof line ? uimn.edline.len : sizeof line - 1;
    memcpy(line, uimn.edline.text, n);
    line[n] = 0;
    return line;
}

void fx_shown(Session *const S)
{
    draw_all(S);
    scrn_flip();
}

const char *fx_row(const uint8_t row)
{
    static char s[PSCR_COLS + 1];
    uint8_t n = 0;

    for (uint8_t c = 0; c < PSCR_COLS; c++)
        s[n++] = (char)host_cell(0, row, c);

    while (n && s[n - 1] == ' ')
        n--;

    s[n] = 0;
    return s;
}

uint8_t fx_attr(const uint8_t row, const uint8_t col)
{
    return (uint8_t)(host_cell(0, row, col) >> 8);
}

uint8_t fx_row_of(const uint32_t line)
{
    return (uint8_t)(uimn.ed.row0 + line - uimn.text.first - uimn.ed.top);
}

uint8_t fx_col_at(const uint16_t col) { return (uint8_t)(uimn.ed.x + 1 + col - uimn.ed.left); }
