/* Whole lines moved, copied and commented out.
 *
 * # How it works
 * The lines are the selection's (edit_sel's SEL_LINES), else the cursor's, in its view. A move
 * takes the neighbour out and puts it back on the block's other side, so the block's own records
 * stay, their breakpoints with them. Each is one command, so one undo step.
 *
 * None changes a native module's lines (edit_refused). None goes past a procedure's bounds: a SUB
 * or FUNCTION line, the view's first, or an END SUB or END FUNCTION line is never moved, copied,
 * commented or moved past; the key beeps. A comment goes after the line's number and indent; a
 * line taken out of one is stored as the editor stores a line left (edit_store), so with an error
 * it is kept as typed.
 */
#include "ui/lines.h"
#include "plat/pscr.h"
#include "ui/edit.h"
#include "ui/text.h"
#include "ui/view.h"

static LineBuf lb = {.grows = true};

/* View line line's text to lb, 0-ended; its length. */
static uint16_t get(Session *const S, const uint32_t line)
{
    const uint16_t n = (uint16_t)text_get(S, line, &lb);
    lb.text[n] = 0;
    lb.len = n;
    return n;
}

/* The lines worked on: the selection's, else the cursor's. */
static void block(uint32_t *const a, uint32_t *const b)
{
    if (edit_sel() == SEL_LINES) {
        *a = uimn.ed.sel_a;
        *b = uimn.ed.sel_b;
    } else
        *a = *b = uimn.ed.line;
}

/* s starts with word, case aside, a word's end after it. */
static bool starts(const uint8_t *const s, const char *const word)
{
    const size_t n = strlen(word);

    for (size_t i = 0; i < n; i++)
        if (upcase(s[i]) != upcase((uint8_t)word[i]))
            return false;

    return !is_word_char(s[n]);
}

/* View line line bounds its procedure: its SUB or FUNCTION line, or its END. */
static bool bound(Session *const S, const uint32_t line)
{
    if (!line && view_at(uimn.ed.view)->kind)
        return true;

    get(S, line);
    const uint8_t *const body = edit_body(lb.text);
    return starts(body, "end sub") || starts(body, "end function");
}

/* Lines a to b, of the view, bound nothing. */
static bool inside(Session *const S, const uint32_t a, const uint32_t b)
{
    for (uint32_t line = a; line <= b; line++)
        if (bound(S, line))
            return false;

    return true;
}

/* The cursor and the selection's end moved by delta lines, the cursor's line loaded. */
static void shift(Session *const S, const int32_t delta)
{
    uimn.ed.sel_line = (uint32_t)((int32_t)uimn.ed.sel_line + delta);
    edit_load(S, (uint32_t)((int32_t)uimn.ed.line + delta));
    edit_ensure();
}

/* View line line's text and kind, out of the text, put back before view line to. */
static void carry(Session *const S, const uint32_t line, const uint32_t to)
{
    const uint32_t whole = uimn.text.first + line;
    const TextKind k = text_kind(S, whole);
    const bool marked = text_mark(S, line);
    get(S, line);
    text_delete(S, line);
    uimn.text.asis = k.asis;
    text_insert(S, to, lb.text, lb.len);

    if (marked)
        text_toggle(S, to);
}

void lines_move(Session *const S, const bool down)
{
    if (edit_refused(S))
        return;

    edit_commit(S);
    uint32_t a, b;
    block(&a, &b);

    if (down ? b + 1 >= uimn.text.n : !a) {
        pscr_beep();
        return;
    }

    if (!inside(S, down ? a : a - 1, down ? b + 1 : b)) {
        pscr_beep();
        return;
    }

    if (down)
        carry(S, b + 1, a);
    else
        carry(S, a - 1, b);

    shift(S, down ? 1 : -1);
}

void lines_copy(Session *const S, const bool down)
{
    if (edit_refused(S))
        return;

    edit_commit(S);
    uint32_t a, b;
    block(&a, &b);

    if (!inside(S, a, b)) {
        pscr_beep();
        return;
    }

    const uint32_t n = b - a + 1;

    for (uint32_t i = 0; i < n; i++) {
        const TextKind k = text_kind(S, uimn.text.first + a + i);
        get(S, a + i);
        uimn.text.asis = k.asis;
        text_insert(S, b + 1 + i, lb.text, lb.len);
    }

    shift(S, down ? (int32_t)n : 0);
}

/* lb's statement's column: past its number and indent. */
static uint16_t body_col(void) { return (uint16_t)(edit_body(lb.text) - lb.text); }

/* Column c of a line whose statement at at grew by delta: where its character went. */
static uint16_t moved(const uint16_t c, const uint16_t at, const int32_t delta)
{
    if (c < at)
        return c;

    const int32_t to = (int32_t)c + delta;
    return (uint16_t)(to < at ? at : to);
}

/* View line line commented, or with out its comment taken away: stored; the cursor and the
   selection's end on it moved with its text. */
static void comment(Session *const S, const uint32_t line, const bool out)
{
    get(S, line);
    const uint16_t at = body_col();

    if (!lb.text[at])
        return; /* blank */

    const uint16_t cut = out ? (uint16_t)(1 + (lb.text[at + 1] == ' ')) : 0;
    const uint8_t mark[] = "' ";
    const uint16_t put = out ? 0 : 2;
    uint8_t *const t = edit_lb_need(S, &lb, (uint32_t)(lb.len + put));
    memmove(t + at + put, t + at + cut, (size_t)(lb.len - at - cut));
    memcpy(t + at, mark, put);
    lb.len = (uint16_t)(lb.len + put - cut);
    edit_store(S, line, &lb);
    const int32_t delta = (int32_t)put - (int32_t)cut;

    if (uimn.ed.sel_line == line)
        uimn.ed.sel_col = moved(uimn.ed.sel_col, at, delta);
    if (uimn.ed.line == line)
        uimn.edline.col = moved(uimn.edline.col, at, delta);
}

void lines_comment(Session *const S)
{
    if (edit_refused(S))
        return;

    edit_commit(S);
    uint32_t a, b;
    block(&a, &b);

    if (!inside(S, a, b)) {
        pscr_beep();
        return;
    }

    bool out = true, any = false;

    for (uint32_t line = a; line <= b; line++) {
        get(S, line);
        const uint16_t at = body_col();

        if (lb.text[at]) {
            any = true;
            out = out && lb.text[at] == '\'';
        }
    }

    if (!any)
        return;

    for (uint32_t line = a; line <= b; line++)
        comment(S, line, out);

    edit_load(S, uimn.ed.line);
}
