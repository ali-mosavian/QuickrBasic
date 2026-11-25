/* What matches the cursor's bracket or block keyword: lit, and gone to with Ctrl+].
 *
 * # How it works
 * A bracket at the cursor, or just before it, outside strings and comments (the highlighter's
 * slots), has its partner on its line. A block keyword at the cursor (indent.h's events: FOR,
 * NEXT, IF with THEN, ELSE, END IF, ...) has the others of its block: its opening one found up
 * the view, then each middle and the end down from it, the nested blocks of its kind passed by
 * their depth. A keyword whose block has no end matches nothing.
 *
 * The matches are kept by the cursor's place, its line's text and the text's changes; found
 * again only when one of those changed.
 */
#include "ui/pairs.h"
#include "plat/pscr.h"
#include "ui/edit.h"
#include "ui/highlight.h"
#include "ui/indent.h"
#include "ui/screen.h"
#include "ui/text.h"
#include "util/hash.h"

enum { NMARK = 64 };

/* A match: its whole text's line, its columns. */
typedef struct {
    uint32_t line;
    uint16_t col, len;
} Mark;

static struct {
    uint32_t key;
    uint32_t n;
    Mark mark[NMARK];
    LineBuf line;
    uint8_t cols[SYNT_KEEP];
} pr = {.line = {.grows = true}};

static void add(const uint32_t line, const uint16_t col, const uint16_t len)
{
    if (pr.n < NMARK)
        pr.mark[pr.n++] = (Mark){uimn.text.first + line, col, len};
}

/* The cursor's line's character at i is code: no string's, no comment's. */
static bool code(const uint16_t i)
{
    return i >= SYNT_KEEP || (pr.cols[i] != SL_STR && pr.cols[i] != SL_COM);
}

/* The cursor's line's bracket at i matched along it. */
static void bracket(const uint16_t i)
{
    const LineBuf *const b = &uimn.edline;
    const bool open = b->text[i] == '(';
    int32_t depth = 0;

    for (int32_t j = i; j >= 0 && j < b->len; j += open ? 1 : -1) {
        if (!code((uint16_t)j))
            continue;

        depth += b->text[j] == '(' ? 1 : b->text[j] == ')' ? -1 : 0;

        if (!depth) {
            add(uimn.ed.line, i, 1);
            add(uimn.ed.line, (uint16_t)j, 1);
            return;
        }
    }
}

/* View line line's block events to ev; their count. */
static uint32_t events(Session *const S, const uint32_t line, IndtEv *const ev)
{
    if (line == uimn.ed.line)
        return indt_events(uimn.edline.text, uimn.edline.len, ev);

    const uint16_t n = (uint16_t)text_get(S, line, &pr.line);
    return indt_events(pr.line.text, n, ev);
}

/* Up from event e of line, its block's opening one's line to *line, its event to *e. */
static bool opening(Session *const S, uint32_t *const line, uint32_t *const e)
{
    IndtEv ev[INDT_NEV];
    const IndtKind kind = events(S, *line, ev) ? ev[*e].kind : K_FOR;
    uint32_t depth = 0;

    for (uint32_t l = *line + 1; l--;) {
        const uint32_t n = events(S, l, ev);

        for (uint32_t i = l == *line ? *e + 1 : n; i--;) {
            if (ev[i].kind != kind || ev[i].what == EV_MID ||
                (l == *line && i == *e && ev[i].what == EV_END))
                continue;
            if (ev[i].what == EV_END)
                depth++;
            else if (depth)
                depth--;
            else {
                *line = l;
                *e = i;
                return true;
            }
        }
    }

    return false;
}

/* Down from the opening event e of line: it, its block's middles and its end, kept. */
static void block(Session *const S, const uint32_t line, const uint32_t e)
{
    IndtEv ev[INDT_NEV];
    events(S, line, ev);
    const IndtKind kind = ev[e].kind;
    uint32_t depth = 0;
    add(line, ev[e].col, ev[e].len);

    for (uint32_t l = line; l < uimn.text.n; l++) {
        const uint32_t n = events(S, l, ev);

        for (uint32_t i = l == line ? e + 1 : 0; i < n; i++) {
            if (ev[i].kind != kind)
                continue;
            if (ev[i].what == EV_OPEN)
                depth++;
            else if (depth && ev[i].what == EV_END)
                depth--;
            else if (!depth) {
                add(l, ev[i].col, ev[i].len);

                if (ev[i].what == EV_END)
                    return;
            }
        }
    }

    pr.n = 0; /* never ended: no match */
}

/* The cursor's block keyword's block. */
static void keyword(Session *const S)
{
    IndtEv ev[INDT_NEV];
    const uint16_t col = uimn.edline.col;
    const uint32_t n = events(S, uimn.ed.line, ev);

    for (uint32_t e = 0; e < n; e++) {
        if (col < ev[e].col || col > ev[e].col + ev[e].len)
            continue;

        uint32_t line = uimn.ed.line, at = e;

        if (ev[e].what == EV_OPEN || opening(S, &line, &at))
            block(S, line, at);

        return;
    }
}

void pairs_find(Session *const S)
{
    const LineBuf *const b = &uimn.edline;
    const uint32_t place = uimn.text.first + uimn.ed.line ^ (uint32_t)b->col << 20;
    const uint32_t key = fnv(FNV_SEED ^ uimn.text.gen ^ place, b->text, b->len);

    if (key == pr.key)
        return;

    pr.key = key;
    pr.n = 0;
    synt_cols(
        S,
        text_dialect_at(S, uimn.text.first + uimn.ed.line),
        b->text,
        b->len < SYNT_KEEP ? b->len : SYNT_KEEP,
        pr.cols
    );
    const uint16_t col = b->col;

    if (col < b->len && (b->text[col] == '(' || b->text[col] == ')') && code(col))
        bracket(col);
    else if (col && col <= b->len && (b->text[col - 1] == '(' || b->text[col - 1] == ')') &&
             code(col - 1))
        bracket(col - 1);
    else
        keyword(S);
}

void pairs_paint(const uint8_t row, const uint8_t col, const uint32_t line)
{
    const uint32_t left = uimn.ed.left, right = left + uimn.ed.width;

    for (uint32_t i = 0; i < pr.n; i++) {
        const Mark *const m = &pr.mark[i];

        if (m->line != line)
            continue;

        const uint32_t a = m->col > left ? m->col : left, end = m->col + m->len;
        const uint32_t b = end < right ? end : right;

        if (a < b)
            scrn_attr(row, (uint8_t)(col + a - left), (uint16_t)(b - a), SL_MATCH);
    }
}

void pairs_jump(Session *const S)
{
    pairs_find(S);
    const uint32_t line = uimn.text.first + uimn.ed.line;
    const uint16_t col = uimn.edline.col;

    for (uint32_t i = 0; i < pr.n; i++) {
        const Mark *const m = &pr.mark[i];

        if (m->line != line || col < m->col || col > m->col + m->len)
            continue;

        const Mark *const to = &pr.mark[(i + 1) % pr.n];
        edit_goto(S, to->line - uimn.text.first);
        uimn.edline.col = to->col;
        uimn.ed.sel_on = false;
        edit_ensure();
        return;
    }

    pscr_beep();
}
