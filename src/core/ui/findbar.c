/* The find bar: Find What typed on the status row, the program searched as it is typed.
 *
 * # How it works
 * The bar is a modal widget over the status bar: it has the keys while it is up. Each change to
 * Find What, or to Match Case or Whole Word, selects the first match from where the search began
 * (fb.from), or puts the cursor back there when there is none. Enter and F3 go on from the
 * match, which becomes the search's start. A key or click that is not the bar's hides it and
 * then does what it does.
 *
 * The text the bar opens with is selected: a character typed replaces it.
 */
#include "ui/findbar.h"
#include "plat/pkey.h"
#include "plat/pscr.h"
#include "ui/dialog.h"
#include "ui/edit.h"
#include "ui/mouse.h"
#include "ui/screen.h"
#include "ui/search.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "util/hash.h"

enum : uint8_t {
    FB_FIELD = 7, /* Find What's columns */
    FB_WIDTH = 24,
    FB_COUNT = 33,
    FB_CASE = 46,
    FB_WORD = 58,
    FB_ESC = 70,
    SCAN_W = 0x11,
    SCAN_C = 0x2E,
    SCAN_F3 = SCAN_F1 + 2,
    SCAN_SHIFT_F3 = SCAN_SHIFT_F1 + 2,
};

static const char s_find[] = "Find:", s_case[] = "Alt+C=Case", s_word[] = "Alt+W=Word",
                  s_esc[] = "Esc=Close", s_none[] = "No match", s_of[] = " of ";

static struct {
    Widget w;
    SrchAt from; /* where the search began */
    bool fresh;  /* Find What as the bar opened with it: a character replaces it */
} fb;

/* From fb.from, the first match selected; none: the cursor back there. */
static void find(Session *const S)
{
    SrchAt at = fb.from;

    if (srch_find(S, &at, false)) {
        srch_show(S, at);
        return;
    }

    edit_load(S, view_goline(S, fb.from.line));
    uimn.edline.col = fb.from.col;
    uimn.ed.sel_on = false;
    edit_ensure();
}

/* The next match, or with back the one before, selected: the search's start; a beep for none. */
static void next(Session *const S, const bool back)
{
    SrchAt at = srch_from(back);

    if (!srch_find(S, &at, back)) {
        pscr_beep();
        return;
    }

    srch_show(S, at);
    fb.from = at;
}

/* An option switched: the search again. */
static void flip(Session *const S, bool *const option)
{
    *option = !*option;
    find(S);
}

/* Find What's hash: whether a key changed it. */
static uint32_t what_hash(void)
{
    return fnv(FNV_SEED ^ uimn.find_what.len, uimn.find_what.text, uimn.find_what.len);
}

static bool fb_key(Widget *const w, Session *const S, const uint16_t key)
{
    const bool shift = uimn.keyflags & PKEY_SHIFT;

    switch (key) {
    case SCAN_ESC << 8 | CH_ESC:
        fbar_close();
        return true;

    case SCAN_ENTER << 8 | CH_ENTER:
    case SCAN_F3 << 8:
        next(S, shift);
        return true;

    case SCAN_SHIFT_F3 << 8:
        next(S, true);
        return true;

    case SCAN_C << 8:
        flip(S, &uimn.match_case);
        return true;

    case SCAN_W << 8:
        flip(S, &uimn.whole_word);
        return true;

    default:
        break;
    }

    const uint32_t was = what_hash();

    if (fb.fresh && (uint8_t)key >= ' ' && (uint8_t)key != CH_E0)
        uimn.find_what.len = uimn.find_what.col = 0;

    if (!dlog_ledit(&uimn.find_what, key)) { /* not the bar's: it goes, the key acts */
        fbar_close();
        uimn_key(S, key);
        return true;
    }

    fb.fresh = false;

    if (what_hash() != was)
        find(S);

    return true;
}

static bool fb_mouse(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    if (uimn.mouse.event != MS_PRESS)
        return true;

    if (row != status_row()) { /* elsewhere: the bar goes, the click acts */
        fbar_close();
        tree_mouse(&uimn.tree.screen, S, row, col);
        return true;
    }

    if (col >= FB_CASE && col < FB_CASE + sizeof s_case - 1)
        flip(S, &uimn.match_case);
    else if (col >= FB_WORD && col < FB_WORD + sizeof s_word - 1)
        flip(S, &uimn.whole_word);

    return true;
}

/* Find What's first column shown: the cursor in the field. */
static uint16_t field_left(void)
{
    const uint16_t col = uimn.find_what.col;
    return col >= FB_WIDTH ? (uint16_t)(col - FB_WIDTH + 1) : 0;
}

/* "3 of 12", the matches without one at the cursor, or none. */
static void draw_count(Session *const S, const uint8_t row)
{
    const SrchAt at = {uimn.text.first + uimn.ed.line, uimn.ed.sel_col};

    uint32_t nth = 0;
    const uint32_t n = srch_count(S, at, &nth);

    if (!n) {
        scrn_str(row, FB_COUNT, s_none, SL_SMSG, SL_SMSG);
        return;
    }

    char text[24];
    char *d = text;

    if (uimn.ed.sel_on && nth) {
        d = uimn_dec(d, (uint16_t)(nth < UINT16_MAX ? nth : UINT16_MAX));
        memcpy(d, s_of, sizeof s_of - 1);
        d += sizeof s_of - 1;
    }

    d = uimn_dec(d, (uint16_t)(n < UINT16_MAX ? n : UINT16_MAX));
    *d = 0;
    scrn_str(row, FB_COUNT, text, SL_STAT, SL_STAT);
}

static void fb_draw(Widget *const w, Session *const S)
{
    const uint8_t row = status_row();
    const LineBuf *const f = &uimn.find_what;
    const uint16_t left = field_left(), shown = f->len > left ? f->len - left : 0;
    scrn_fill(row, 0, SL_STAT, ' ', PSCR_COLS);
    scrn_str(row, 1, s_find, SL_STAT, SL_STAT);
    scrn_fill(row, FB_FIELD, SL_ED, ' ', FB_WIDTH);
    scrn_strn(
        row,
        FB_FIELD,
        f->text + left,
        shown < FB_WIDTH ? shown : FB_WIDTH,
        fb.fresh ? SL_SEL : SL_ED
    );

    if (f->len)
        draw_count(S, row);

    scrn_str(row, FB_CASE, s_case, uimn.match_case ? SL_MSEL : SL_STAT, SL_STAT);
    scrn_str(row, FB_WORD, s_word, uimn.whole_word ? SL_MSEL : SL_STAT, SL_STAT);
    scrn_str(row, FB_ESC, s_esc, SL_STAT, SL_STAT);
}

static void fb_cursor(Widget *const w)
{
    scrn_shape(CURSOR_INSERT);
    scrn_cursor(status_row(), (uint8_t)(FB_FIELD + uimn.find_what.col - field_left()));
}

static const WidgetOps fb_ops = {
    .draw = fb_draw,
    .key = fb_key,
    .mouse = fb_mouse,
    .cursor = fb_cursor,
};

void fbar_build(Widget *const screen)
{
    fb.w = (Widget){.ops = &fb_ops, .rect = {0, 0, 1, PSCR_COLS}, .modal = true};

    tree_add(screen, &fb.w);
}

void fbar_open(Session *const S)
{
    edit_commit(S);
    uint16_t from;
    const bool given = srch_default(&from);
    fb.from = (SrchAt){uimn.text.first + uimn.ed.line, given ? from : uimn.edline.col};

    uimn.find_what.col = uimn.find_what.len;
    fb.fresh = uimn.find_what.len > 0;
    fb.w.rect.row = status_row();
    fb.w.visible = true;
    find(S);
}

void fbar_close(void) { fb.w.visible = false; }

void fbar_paint(const uint8_t row, const uint8_t col, const uint8_t *const text, const uint32_t len)
{
    const uint32_t n = uimn.find_what.len, left = uimn.ed.left, right = left + uimn.ed.width;

    if (!fb.w.visible || !n)
        return;

    for (uint32_t c = left >= n ? left - n + 1 : 0; c < right && c < len; c++) {
        if (!srch_match(text, (uint16_t)len, (uint16_t)c))
            continue;

        const uint32_t a = c > left ? c : left, b = c + n < right ? c + n : right;
        scrn_attr(row, (uint8_t)(col + a - left), (uint16_t)(b - a), SL_FIND);
    }
}
