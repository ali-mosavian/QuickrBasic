/* Completion in the program window: Tab, or typing a word's second letter, lists what fits at the
 * word. AC builds only.
 *
 * # How it works
 * comp/suggest.c says what fits, ranked. Tab puts in the part the list agrees on, the one entry
 * that fits, or shows the list; typing a word's second letter shows the list when no key waits.
 * In the list the arrows choose, Tab takes (Enter too, when Tab opened it), Esc leaves, a word's
 * character is typed and the list follows the word, Backspace too; another key leaves it and is
 * typed. A taken entry replaces the word; a function gets its '(' and its form on the status bar;
 * a line number's entry shows its line.
 *
 * # Example
 *   x$ = l     Tab lists LEFT$(, LCASE$(, L$ ..: the strings
 *
 * # asm structure (src/ui/compl.asm)
 * | asm | here | |
 * |---|---|---|
 * | ac_tab | cmpl_tab | MIRROR; D1 |
 * | (new) | cmpl_typed | D2 |
 * | ac_word, ac_put, ac_common, ac_ins, ac_sig, ac_prev, ac_draw, ac_pop | cmpl_word, cmpl_put, cmpl_common, cmpl_insert, cmpl_form, cmpl_preview, cmpl_draw, cmpl_popup | MIRROR |
 * | ac_ctx, ac_mask, ac_want, ac_keys, ac_vars, ac_open | comp/suggest.c | MOVED |
 *
 * - D1. A flag result is a bool, true the success.
 * - D2. The asm lists on Tab only.
 */
#include "ui/complete.h"
#include "session.h"
#if AC
#include "comp/dfa.h"
#include "comp/suggest.h"
#include "plat/pkey.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/mouse.h"
#include "ui/screen.h"
#include "ui/text.h"
#include "ui/uimain.h"

enum {
    AC_ROWS = 8,  /* the list's rows */
    AC_PREV = 40, /* a line number's box: columns */
    AC_AUTO = 2,  /* a word's letters that show the list as typed */
    SIG_LEN = 80, /* a function's form */
};
/* The list's box: its frame's rows; a name's column, the columns past a name; a line's text at
   PV_COL past the number, PV_LEN characters at most. */
enum { BOX_FRAME = 2, NAME_COL = 2, NAME_PAD = 4, PV_COL = 7, PV_LEN = AC_PREV - 11 };

static struct {
    Suggs s;
    uint16_t sel; /* the one lit */
    uint16_t top; /* the first shown */
    bool lc;      /* the word in small letters */
    bool typed;   /* the list shown by typing: Enter is typed */
    bool open;    /* the list shown */
    uint8_t at_row, at_col;
    uint8_t bw; /* the box's columns */
    uint8_t bn; /* its rows of names */
    char sig[SIG_LEN];
} ac;

/* the arguments of the functions not of one number */
static const struct {
    uint16_t tok;
    const char *args;
} func_args[] = {
    {TK_PEEK, "address"},
    {TK_LEN, "s$"},
    {TK_VAL, "s$"},
    {TK_ASC, "s$"},
    {TK_CHRS, "code"},
    {TK_LEFTS, "s$, n"},
    {TK_RIGHTS, "s$, n"},
    {TK_MID, "s$, start[, n]"},
    {TK_TAB, "column"},
    {TK_SPC, "n"},
};

/* What a key in the list does. */
typedef enum : uint8_t { STEP_STAY, STEP_TAKE, STEP_LEAVE } Step;

static bool cmpl_wordc(const uint8_t c) { return is_letter(c) || is_digit(c); }

/* What fits at the cursor, to ac.s; false nothing. */
static bool cmpl_find(Session *const S)
{
    const LineBuf *const b = &uimn.edline;

    if (!sugg_find(S, b->text, b->len, b->col, text_rec(S, uimn.ed.line), &ac.s))
        return false;

    ac.lc = false;

    for (uint16_t i = ac.s.start; i < b->col; i++)
        if (is_letter(b->text[i]))
            ac.lc = b->text[i] >= 'a';

    return true;
}

/* The word replaced by the first n characters of s, in small letters when the word is. */
static void cmpl_put(const uint8_t *const s, const uint16_t n)
{
    LineBuf *const b = &uimn.edline;
    uint8_t t[SUGG_LEN + 2];

    for (uint16_t i = 0; i < n; i++)
        t[i] = ac.lc && is_letter(s[i]) ? s[i] | 0x20 : s[i];

    edit_lb_cut(b, ac.s.start, b->col);
    edit_lb_insert(b, t, n);
    uimn.text.changed = true;
}

/* The part every entry starts with, past the word, put in: none fuzzy. */
static void cmpl_common(void)
{
    const Sugg *const first = &ac.s.list[0];
    uint16_t n = first->len;

    for (uint16_t i = 0; i < ac.s.n; i++) {
        const Sugg *const e = &ac.s.list[i];

        if (e->fuzzy)
            return;

        uint16_t k = 0;

        while (k < n && k < e->len && e->text[k] == first->text[k])
            k++;

        n = k;
    }

    if (n > uimn.edline.col - ac.s.start)
        cmpl_put(first->text, n);
}

/* Entry e's form in ac.sig: a function's arguments, a procedure's from its line. */
static void cmpl_form(Session *const S, const Sugg *const e)
{
    uint32_t n = e->len;
    memcpy(ac.sig, e->text, n);

    if (e->rec) { /* a SUB or FUNCTION: the parameters on its line */
        const uint32_t len = text_record(S, e->rec, uimn.build);
        uimn.build[len] = 0;
        const char *const open = strchr((const char *)uimn.build, '(');
        const char *close = nullptr; /* its last ')' */

        for (const char *c = open; c && *c; c++)
            if (*c == ')')
                close = c;

        const uint32_t room = SIG_LEN - n - 3;
        const uint32_t k = close ? (uint32_t)(close - open - 1) : 0;

        if (close) {
            ac.sig[n++] = e->kind == SG_SUB ? ' ' : '(';
            memcpy(ac.sig + n, open + 1, k < room ? k : room);
            n += k < room ? k : room;

            if (e->kind == SG_FN)
                ac.sig[n++] = ')';
        }

        ac.sig[n] = 0;
        return;
    }

    const char *a = "x";

    for (uint32_t i = 0; i < countof(func_args); i++)
        if (func_args[i].tok == e->tok)
            a = func_args[i].args;

    if (ac.sig[n - 1] != '(')
        ac.sig[n++] = '(';

    const uint32_t an = (uint32_t)strlen(a);
    memcpy(ac.sig + n, a, an);
    n += an;
    ac.sig[n++] = ')';
    ac.sig[n] = 0;
}

/* Entry e has a form: a function, a procedure. */
static bool cmpl_hasform(const Sugg *const e) { return e->kind == SG_FN || e->kind == SG_SUB; }

static void cmpl_show_form(Session *const S, const Sugg *const e)
{
    cmpl_form(S, e);
    uimn.help = ac.sig;
}

/* Entry i replaces the word; a function gets its '(' and its form on the status bar. */
static void cmpl_insert(Session *const S, const uint16_t i)
{
    const Sugg *const e = &ac.s.list[i];
    cmpl_put(e->text, e->len);
    sugg_used(e);

    if (!cmpl_hasform(e))
        return;

    cmpl_show_form(S, e);

    if (e->kind == SG_FN && e->text[e->len - 1] != '(' && strchr(ac.sig, '('))
        edit_lb_insert(&uimn.edline, (const uint8_t *)"(", 1);
}

/* Line number entry e's line after its number, at row, col + PV_COL in the box. */
static void cmpl_preview(
    Session *const S,
    const Sugg *const e,
    const uint8_t row,
    const uint8_t col
)
{
    const uint32_t len = text_record(S, e->rec, uimn.build);
    uint32_t off = 0;

    while (off < len && is_digit(uimn.build[off]))
        off++;
    while (off < len && uimn.build[off] == ' ')
        off++;

    const uint32_t n = len - off;
    scrn_strn(row, (uint8_t)(col + PV_COL), uimn.build + off, n > PV_LEN ? PV_LEN : n, SL_BAR);
}

/* The list's box, ac.sel lit. */
static void cmpl_draw(Session *const S)
{
    scrn_box(ac.at_row, ac.at_col, ac.bn + BOX_FRAME, ac.bw, SL_BAR, nullptr);
    scrn_shadow(ac.at_row, ac.at_col, ac.bn + BOX_FRAME, ac.bw);

    for (uint8_t b = 0; b < ac.bn; b++) {
        const uint8_t row = ac.at_row + b + 1, col = ac.at_col + NAME_COL;
        const Sugg *const e = &ac.s.list[ac.top + b];
        scrn_strn(row, col, e->text, e->len, SL_BAR);

        if (e->kind == SG_LABEL)
            cmpl_preview(S, e, row, col);
        if (ac.top + b == ac.sel)
            scrn_attr(row, col - 1, ac.bw - 2, SL_DSEL);
    }
}

/* The list's box under the word, else over it. */
static void cmpl_place(void)
{
    uint8_t bw = NAME_PAD; /* the box: the longest name and NAME_PAD */

    for (uint16_t i = 0; i < ac.s.n; i++) {
        const Sugg *const e = &ac.s.list[i];
        const uint8_t w = e->kind == SG_LABEL ? AC_PREV : e->len + NAME_PAD;

        if (w > bw)
            bw = w;
    }

    ac.bw = bw;
    ac.bn = (uint8_t)(ac.s.n > AC_ROWS ? AC_ROWS : ac.s.n);
    const uint8_t h = ac.bn + BOX_FRAME;
    uint8_t row = (uint8_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0 + 1); /* under, else over */

    if ((uint8_t)(row + h) > STATUS_ROW)
        row = row - h - 1;

    const uint16_t c = ac.s.start > uimn.ed.left ? ac.s.start - uimn.ed.left : 0;
    uint8_t col = (uint8_t)c;

    if ((uint8_t)(col + bw) > PSCR_COLS)
        col = PSCR_COLS - bw;

    ac.at_row = row;
    ac.at_col = col;
}

/* A mouse event in the list: a click on a name takes it, one outside leaves. */
static Step cmpl_mouse(void)
{
    if (uimn.mouse.event != MS_PRESS)
        return STEP_STAY;

    const uint8_t r = uimn.mouse.row - ac.at_row;
    const uint8_t x = uimn.mouse.col - ac.at_col;

    if (uimn.mouse.row <= ac.at_row || r > ac.bn || uimn.mouse.col <= ac.at_col || x >= ac.bw)
        return STEP_LEAVE;

    ac.sel = (uint16_t)(r - 1 + ac.top);
    return STEP_TAKE;
}

static bool cmpl_is_move(const uint8_t scan)
{
    return scan == SCAN_UP || scan == SCAN_DOWN || scan == SCAN_PGUP || scan == SCAN_PGDN;
}

/* An arrow or page key: ac.sel moved, the list scrolled to it. */
static void cmpl_move(const uint8_t scan)
{
    int32_t s = ac.sel;

    if (scan == SCAN_UP)
        s = s ? s - 1 : 0;
    else if (scan == SCAN_PGUP)
        s = s >= AC_ROWS ? s - AC_ROWS : 0;
    else {
        s += scan == SCAN_PGDN ? AC_ROWS : 1;

        if (s >= ac.s.n)
            s = ac.s.n - 1;
    }

    ac.sel = (uint16_t)s;

    if (s < ac.top)
        ac.top = (uint16_t)s;
    if (s >= ac.bn + ac.top)
        ac.top = (uint16_t)(s - (AC_ROWS - 1));
}

/* The list again for the word as it is now: false nothing fits, or no word. */
static bool cmpl_refind(Session *const S)
{
    if (uimn.edline.col == ac.s.start || !cmpl_find(S))
        return false;

    ac.sel = 0;
    ac.top = 0;
    cmpl_place();
    return true;
}

/* key in the list. */
static Step cmpl_key(Session *const S, const uint16_t key)
{
    if (key == KEY_MOUSE)
        return cmpl_mouse();

    const uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);

    if (ch == CH_TAB || (ch == CH_ENTER && !ac.typed))
        return STEP_TAKE;
    if (ch == CH_ESC)
        return STEP_LEAVE;
    if (cmpl_wordc(ch) || ch == CH_BACKSPACE) { /* the word goes on: the list follows it */
        uimn_key(S, key);
        return cmpl_refind(S) ? STEP_STAY : STEP_LEAVE;
    }

    if (ch || !cmpl_is_move(scan)) {
        uimn_key(S, key);
        return STEP_LEAVE;
    }

    cmpl_move(scan);
    return STEP_STAY;
}

/* The list under the word till a key takes an entry or leaves. */
static void cmpl_popup(Session *const S)
{
    ac.sel = 0;
    ac.top = 0;
    ac.open = true;
    cmpl_place();
    Step step;

    do {
        uimn.help = nullptr;

        if (cmpl_hasform(&ac.s.list[ac.sel]))
            cmpl_show_form(S, &ac.s.list[ac.sel]);

        draw_all(S);
        cmpl_draw(S);
        step = cmpl_key(S, uimn_getkey());
    } while (step == STEP_STAY);

    ac.open = false;
    uimn.help = nullptr;

    if (step == STEP_TAKE)
        cmpl_insert(S, ac.sel);
}

bool cmpl_tab(Session *const S)
{
    if (!cmpl_find(S))
        return false;

    ac.typed = false;
    cmpl_common();

    if (ac.s.n == 1)
        cmpl_insert(S, 0);
    else
        cmpl_popup(S);

    return true;
}

void cmpl_typed(Session *const S)
{
    const LineBuf *const b = &uimn.edline;
    const uint16_t col = b->col;

    if (ac.open || pkey_ready() || col < AC_AUTO || (col < b->len && cmpl_wordc(b->text[col])))
        return;

    uint16_t s = col;

    while (s && cmpl_wordc(b->text[s - 1]))
        s--;

    if (col - s != AC_AUTO || !is_letter(b->text[s]) || !cmpl_find(S))
        return;

    ac.typed = true;
    cmpl_popup(S);
}
#endif
