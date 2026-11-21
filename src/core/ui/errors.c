/* The lines with syntax errors: lit, counted, the cursor's line's message, Ctrl+F8.
 *
 * # How it works
 * A line with a syntax error is stored as typed (LF_ASIS; edit.h) and the cursor goes on. Whether
 * a line kept as typed has an error, and where, is edit_error's: a compile, so its answer is kept
 * in a small table by the line's text, its view and whether checking is on. A line kept as typed
 * for its length alone has none.
 *
 * The message shown is the error's first part, what is wrong, on one line.
 */
#include "ui/errors.h"
#include "plat/pscr.h"
#include "ui/edit.h"
#include "ui/linemap.h"
#include "ui/screen.h"
#include "ui/text.h"
#include "ui/view.h"
#include "util/hash.h"

enum { NKEEP = 64, MSG = 80 };

/* A line's answer: its text's hash, whether it has an error, its column. */
typedef struct {
    uint32_t hash;
    bool set, err;
    uint16_t col;
} Kept;

static struct {
    Kept kept[NKEEP];
    LineBuf line;
    uint32_t msg_hash; /* msg's line's */
    char msg[MSG];
    char saved[sizeof uimn.errmsg];
} er = {.line = {.grows = true}};

/* Text line line's hash: its text, its view, the check. */
static uint32_t line_hash(Session *const S, const uint32_t line)
{
    const uint32_t n = text_record(S, lmap_off(S, line), &er.line);
    return fnv(FNV_SEED ^ view_of(line) ^ (uimn.text.check ? 0x80000000u : 0), er.line.text, n);
}

bool errs_at(Session *const S, const uint32_t line, uint16_t *const col)
{
    if (line >= uimn.text.all || !text_kind(S, line).asis)
        return false;

    const uint32_t h = line_hash(S, line);
    Kept *const k = &er.kept[h % NKEEP];

    if (!k->set || k->hash != h) {
        uint16_t c = 0;
        const bool err = edit_error(S, line, &c);
        *k = (Kept){h, true, err, c};
    }

    *col = k->col;
    return k->err;
}

uint32_t errs_count(Session *const S)
{
    uint32_t n = 0;
    uint16_t col;

    for (uint32_t line = 0; line < uimn.text.all; line++)
        n += errs_at(S, line, &col);

    return n;
}

/* uimn.errmsg's what-is-wrong, its lines joined, to er.msg. */
static void what_of(const char *const m)
{
    const char *s = strchr(m, '|');
    uint32_t n = 0;

    for (s = s ? s + 1 : m; *s && !(s[0] == '|' && s[1] == '|') && n < MSG - 1; s++)
        er.msg[n++] = *s == '|' ? ' ' : *s;

    er.msg[n] = 0;
}

/* The cursor's line in the whole text. */
static uint32_t cursor_line(void) { return uimn.ed.line + uimn.text.first; }

const char *errs_message(Session *const S)
{
    const uint32_t line = cursor_line();
    uint16_t col;

    if (uimn.tree.workspace.focus != FOC_EDIT || uimn.edline.dirty || !errs_at(S, line, &col))
        return nullptr;

    const uint32_t h = line_hash(S, line);

    if (h != er.msg_hash) { /* uimn.errmsg is another's: kept */
        memcpy(er.saved, uimn.errmsg, sizeof er.saved);
        edit_error(S, line, &col);
        what_of(uimn.errmsg);
        memcpy(uimn.errmsg, er.saved, sizeof er.saved);
        er.msg_hash = h;
    }

    return er.msg;
}

void errs_paint(
    Session *const S,
    const uint8_t row,
    const uint8_t col,
    const uint32_t line,
    const uint8_t *const text,
    const uint32_t len
)
{
    uint16_t at;

    if (line == cursor_line() && uimn.edline.dirty) /* changed since stored */
        return;
    if (!errs_at(S, line, &at))
        return;

    uint32_t end = at;

    while (end < len && is_word_char(text[end]))
        end++;

    end += end == at; /* not a word: its character, or the place past the end */
    const uint32_t left = uimn.ed.left, right = left + uimn.ed.width;
    const uint32_t a = at > left ? at : left, b = end < right ? end : right;

    if (a < b)
        scrn_attr(row, (uint8_t)(col + a - left), (uint16_t)(b - a), SL_ERR);
}

void errs_next(Session *const S)
{
    edit_commit(S);
    const uint32_t from = cursor_line(), all = uimn.text.all;

    for (uint32_t i = 1; i <= all; i++) {
        const uint32_t line = (from + i) % all;
        uint16_t col;

        if (!errs_at(S, line, &col))
            continue;

        edit_load(S, view_goline(S, line));
        uimn.edline.col = col;
        uimn.ed.sel_on = false;
        edit_ensure();
        return;
    }

    pscr_beep();
}
