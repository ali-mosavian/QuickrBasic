/* Find What in the program: where it matches, the next match through every view, the count.
 *
 * # How it works
 * A search reads the whole text's lines, every procedure's and module's, as they are stored;
 * the cursor's line is committed first by the caller. It goes round from the last line to the
 * first, and ends at the line it started on.
 *
 * The count reads every line: it is kept by Find What, the options and the text's changes
 * (uimn.text.gen).
 *
 * # asm structure (retired/asm32/ui/ui.asm)
 * | asm | here | |
 * |---|---|---|
 * | ui_wordc, ui_match, ui_fdef | srch_wordc, srch_match, srch_default | MOVED from uimain.c |
 */
#include "ui/search.h"
#include "ui/edit.h"
#include "ui/linemap.h"
#include "ui/text.h"
#include "ui/view.h"
#include "util/hash.h"

static struct {
    LineBuf line;
    uint32_t key; /* the count's: Find What's, the options', the text's */
    SrchAt at;
    uint32_t count, nth;
} sr = {.line = {.grows = true}};

/* ui_wordc: c in a word: a name's, or one of "!#$%&" (QBasic's wordCharBitMask, uiword.asm,
   and '_'). */
bool srch_wordc(const uint8_t c) { return is_name_char(c) || memchr("!#$%&", c, 5) != nullptr; }

bool srch_match(const uint8_t *const text, const uint16_t len, const uint16_t col)
{
    const uint8_t *const s = text + col, *const w = uimn.find_what.text;
    const uint16_t n = uimn.find_what.len;

    if (!n || col + n > len)
        return false;

    for (uint16_t i = 0; i < n; i++) {
        uint8_t a = s[i], b = w[i];

        if (!uimn.match_case) {
            a = upcase(a);
            b = upcase(b);
        }

        if (a != b)
            return false;
    }

    if (!uimn.whole_word)
        return true;
    if (col + n < len && srch_wordc(s[n])) /* a whole word: no letter or digit on either side */
        return false;

    return !col || !srch_wordc(s[-1]);
}

/* Text line line (the whole text's) to sr.line; its length. */
static uint16_t line_text(Session *const S, const uint32_t line)
{
    return (uint16_t)text_record(S, lmap_off(S, line), &sr.line);
}

/* A match in sr.line, len characters, at a column from lo up to hi, the first or with back the
   last, to *col. */
static bool match_in(
    const uint16_t len,
    const uint16_t lo,
    const uint16_t hi,
    const bool back,
    uint16_t *const col
)
{
    for (uint16_t i = lo; i < hi; i++) {
        const uint16_t c = back ? (uint16_t)(hi - 1 - (i - lo)) : i;

        if (srch_match(sr.line.text, len, c)) {
            *col = c;
            return true;
        }
    }

    return false;
}

bool srch_find(Session *const S, SrchAt *const at, const bool back)
{
    const uint32_t all = uimn.text.all;

    if (!uimn.find_what.len || !all)
        return false;

    for (uint32_t i = 0; i <= all; i++) { /* every line, at's twice: its part each side of at */
        const uint32_t line = back ? (at->line + all - i) % all : (at->line + i) % all;
        const uint16_t len = line_text(S, line);
        const bool first = !i, last = i == all;
        uint16_t lo = 0, hi = len;

        if (first != last && first != back) /* forward's first, back's last: from at */
            lo = at->col;
        else if (first != last) /* back's first, forward's last: before at */
            hi = at->col < len ? at->col : len;

        uint16_t col;

        if (lo < hi && match_in(len, lo, hi, back, &col)) {
            *at = (SrchAt){line, col};

            return true;
        }
    }

    return false;
}

/* The cursor's place: its line in the whole text, the selection's start or the cursor's column
   with start, else the cursor's. */
static SrchAt cursor_at(const bool start)
{
    uint16_t col = uimn.edline.col;

    if (start && uimn.ed.sel_on && uimn.ed.sel_line == uimn.ed.line && uimn.ed.sel_col < col)
        col = uimn.ed.sel_col;

    return (SrchAt){uimn.text.first + uimn.ed.line, col};
}

SrchAt srch_from(const bool back)
{
    SrchAt at = cursor_at(back);
    at.col += !back && !uimn.ed.sel_on;
    return at;
}

void srch_show(Session *const S, const SrchAt at)
{
    edit_load(S, view_goline(S, at.line));
    uimn.ed.sel_line = uimn.ed.line;
    uimn.ed.sel_col = at.col;
    uimn.ed.sel_on = true;
    uimn.edline.col = (uint16_t)(at.col + uimn.find_what.len);
    edit_ensure();
}

uint32_t srch_count(Session *const S, const SrchAt at, uint32_t *const nth)
{
    const uint8_t flags = (uint8_t)(uimn.match_case | uimn.whole_word << 1);
    const uint32_t key =
        fnv(FNV_SEED ^ uimn.text.gen ^ flags, uimn.find_what.text, uimn.find_what.len);

    if (key != sr.key || at.line != sr.at.line || at.col != sr.at.col) {
        sr.key = key;
        sr.at = at;
        sr.count = sr.nth = 0;

        for (uint32_t line = 0; line < uimn.text.all; line++) {
            const uint16_t len = line_text(S, line);

            for (uint16_t col = 0; col < len; col++) {
                if (!srch_match(sr.line.text, len, col))
                    continue;

                sr.count++;

                if (line == at.line && col == at.col)
                    sr.nth = sr.count;
            }
        }
    }

    *nth = sr.nth;
    return sr.count;
}

bool srch_default(uint16_t *const from)
{
    if (uimn.tree.workspace.focus)
        return false;

    const uint8_t *const t = uimn.edline.text;
    uint16_t to = uimn.edline.col, len = uimn.edline.len, start;

    if (uimn.ed.sel_on) {
        if (uimn.ed.sel_line != uimn.ed.line)
            return false;

        start = uimn.ed.sel_col;

        if (start > to) {
            const uint16_t x = start;
            start = to;
            to = x;
        }
    } else {
        if (!(to < len && srch_wordc(t[to]))) {
            if (!to)
                return false;

            to--;

            if (to >= len || !srch_wordc(t[to]))
                return false;
        }

        start = to;

        while (start && srch_wordc(t[start - 1]))
            start--;
        while (to < len && srch_wordc(t[to]))
            to++;
    }

    const uint16_t n = to - start;

    if (!n || n > FLDMAX)
        return false;

    uimn.find_what.len = uimn.find_what.col = n;
    memcpy(uimn.find_what.text, t + start, n);
    *from = start;
    return true;
}
