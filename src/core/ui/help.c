/* F1 and the help window.
 *
 * # How it works
 * BASIC.HLP is opened once (helpfile.c); a topic is read and cut into lines and links (helpdoc.c)
 * only when shown. A view (HView) is a topic, the first line shown, and the link chosen: Tab and
 * the letters move the choice, Enter follows it, Alt+F1 goes back through the last HIST_MAX topics.
 *
 * The help window is W_HELP, over the edit windows, as QBasic's: it takes rows from ED1, and has
 * the focus (FOC_HELP) while it is open. Esc closes it. The help box is the same view in a modal
 * box, for F1 in a menu, a dialog and an error's message; it leaves the windows as they were.
 */
#include "ui/help.h"
#include "session.h"
#include "plat/pkey.h"
#include "plat/pmem.h"
#include "plat/psys.h"
#include "plat/pscr.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/helpdoc.h"
#include "ui/helpfile.h"
#include "ui/mouse.h"
#include "ui/screen.h"
#include "ui/uimain.h"
#include "ui/text.h"
#include "ui/window.h"

typedef struct {
    HelpDoc doc;
    HelpHist hist;
    uint16_t id; /* the topic shown */
    uint16_t top;
    int32_t sel; /* the link chosen, -1 none */
    bool shown;
} HView;

static struct {
    HelpFile f;
    bool open, tried;
} hf;

static HView wv; /* the window's */
static HView bv; /* the box's */
static Widget box;
static char title[80];

enum { BOX_ROWS = 17, BOX_COLS = 78, KEY_MAX = 64 };

/* ---- the file */

static void status_lines(void)
{
    uint32_t len;
    const uint8_t *const t = hlf_read(&hf.f, hf.f.status, &len);

    if (!t)
        return;

    uimn.help_text = pmem_resize(nullptr, len);

    if (uimn.help_text) {
        memcpy(uimn.help_text, t, len);
        uimn.help_len = (uint16_t)len;
    }
}

void help_init(void)
{
    static const char file[] = "BASIC.HLP";
    char name[PFIL_PATH];
    const uint32_t n = psys_self(name, sizeof name - sizeof file);
    hf.tried = true;

    if (!n)
        return;

    uint32_t dir = 0; /* past the last '\\' (the host's '/') */
    for (uint32_t i = 0; i < n; i++)
        if (name[i] == '\\' || name[i] == '/')
            dir = i + 1;

    memcpy(name + dir, file, sizeof file);
    help_open(name);
}

bool help_open(const char *const path)
{
    if (hf.open)
        hlf_close(&hf.f);

    hf.open = hlf_open(&hf.f, path);

    if (hf.open)
        status_lines();

    return hf.open;
}

/* ---- a view */

/* The text rows of a view in a window of rows rows, its first link's line the first one shown. */
static void reselect(HView *const v, const uint16_t rows)
{
    const HelpDoc *const d = &v->doc;

    if (v->sel >= 0 && d->link[v->sel].line >= v->top && d->link[v->sel].line < v->top + rows)
        return;

    v->sel = -1;

    for (uint16_t k = 0; k < d->nlinks; k++)
        if (d->link[k].line >= v->top && d->link[k].line < v->top + rows) {
            v->sel = k;
            return;
        }
}

/* Topic id in view v; false: not readable. push: kept in the history. */
static bool show(HView *const v, const uint16_t id, const uint16_t rows, const bool push)
{
    uint32_t len;
    const uint8_t *const text = hlf_read(&hf.f, id, &len);
    HelpDoc d;

    if (!text || !hdoc_load(&d, text, len))
        return false;

    hdoc_free(&v->doc);
    v->doc = d;
    v->id = id;
    v->top = 0;
    v->sel = -1;
    v->shown = true;
    reselect(v, rows);

    if (push)
        hhist_push(&v->hist, id);

    return true;
}

static uint16_t last_top(const HView *const v, const uint16_t rows)
{
    return v->doc.nlines > rows ? (uint16_t)(v->doc.nlines - rows) : 0;
}

static void scroll_to(HView *const v, const int32_t top, const uint16_t rows)
{
    const int32_t max = last_top(v, rows);
    v->top = (uint16_t)(top < 0 ? 0 : top > max ? max : top);
}

/* The chosen link in view. */
static void reveal(HView *const v, const uint16_t rows)
{
    if (v->sel < 0)
        return;

    const uint16_t line = v->doc.link[v->sel].line;

    if (line < v->top)
        v->top = line;
    else if (line >= v->top + rows)
        v->top = (uint16_t)(line - rows + 1);
}

/* The view's first state: the top, the first link in view. */
static bool at_start(HView *const v, const uint16_t rows)
{
    const int32_t sel = v->sel;
    const uint16_t top = v->top;
    v->top = 0;
    v->sel = -1;
    reselect(v, rows);
    const bool same = top == 0 && sel == v->sel;
    return same;
}

/* Alt+F1 or the Back link: a view not at its start goes to it (unless force), else the topic before. */
static void back(HView *const v, const uint16_t rows, const bool force)
{
    if (!force && !at_start(v, rows)) {
        return;
    }

    uint16_t cur, prev;

    if (!hhist_pop(&v->hist, &cur))
        return pscr_beep();
    if (!hhist_pop(&v->hist, &prev)) {
        hhist_push(&v->hist, cur);
        return pscr_beep();
    }

    if (!show(v, prev, rows, true)) {
        hhist_push(&v->hist, prev);
        hhist_push(&v->hist, cur);
    }
}

/* The link k followed. */
static void follow(HView *const v, const uint16_t rows, const uint16_t k)
{
    const uint16_t to = v->doc.link[k].topic;

    if (to == HDOC_BACK)
        back(v, rows, true);
    else if (!show(v, to, rows, true))
        pscr_beep();
}

/* Ctrl+F1 and Shift+Ctrl+F1: the topic after or before this one in the file. */
static void step_topic(HView *const v, const uint16_t rows, const bool previous)
{
    const int32_t to = (int32_t)v->id + (previous ? -1 : 1);

    if (to < 0 || to >= hf.f.nnav || !show(v, (uint16_t)to, rows, true))
        pscr_beep();
}

/* A key in view v of rows text rows; true: it was the view's; *close: Esc. */
static bool view_key(HView *const v, const uint16_t rows, const uint16_t key, bool *const close)
{
    const uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);
    const HelpDoc *const d = &v->doc;

    if (ch == CH_ESC) {
        *close = true;
        return true;
    }

    if (ch == CH_TAB || key == KEY_SHIFT_TAB || (!ch && (scan == SCAN_RIGHT || scan == SCAN_LEFT))) {
        const bool fwd = ch == CH_TAB || scan == SCAN_RIGHT;
        const uint16_t k = hdoc_step(d, v->sel, fwd ? 1 : -1);

        if (k == HDOC_NONE)
            return pscr_beep(), true;

        v->sel = k;
        reveal(v, rows);
        return true;
    }

    if (ch == CH_ENTER) {
        if (v->sel < 0)
            return pscr_beep(), true;

        follow(v, rows, (uint16_t)v->sel);
        return true;
    }

    if (ch) {
        if (ch < ' ' || ch > '~')
            return false;

        const bool upper = ch >= 'A' && ch <= 'Z'; /* a capital looks backwards */
        const uint16_t k = hdoc_letter(d, v->sel, upper ? ch : (uint8_t)(ch - 32 * (ch >= 'a' && ch <= 'z')), upper);

        if (k == HDOC_NONE)
            return pscr_beep(), true;

        v->sel = k;
        reveal(v, rows);
        return true;
    }

    switch (scan) {
    case SCAN_UP:
        scroll_to(v, (int32_t)v->top - 1, rows);
        break;
    case SCAN_DOWN:
        scroll_to(v, (int32_t)v->top + 1, rows);
        break;
    case SCAN_PGUP:
        scroll_to(v, (int32_t)v->top - (rows - 1), rows);
        break;
    case SCAN_PGDN:
        scroll_to(v, (int32_t)v->top + (rows - 1), rows);
        break;
    case SCAN_HOME:
    case SCAN_CTRL_HOME:
        scroll_to(v, 0, rows);
        break;
    case SCAN_END:
    case SCAN_CTRL_END:
        scroll_to(v, last_top(v, rows), rows);
        break;
    case SCAN_F1:
        if (v->sel < 0)
            return pscr_beep(), true;

        follow(v, rows, (uint16_t)v->sel);
        return true;
    case SCAN_ALT_F1:
        back(v, rows, false);
        return true;
    case SCAN_CTRL_F1:
        step_topic(v, rows, pkey_shift() & PKEY_SHIFT);
        return true;
    default:
        return false;
    }

    reselect(v, rows);
    return true;
}

/* The mouse at text row r, column c of view v: a link clicked is followed; the bar's arrows scroll. */
static bool view_mouse(HView *const v, const uint16_t rows, const uint8_t cols, const uint8_t r, const uint8_t c)
{
    if (v->doc.nlines > rows && c == cols - 1) {
        if (r == 0)
            scroll_to(v, (int32_t)v->top - 1, rows);
        else if (r == rows - 1)
            scroll_to(v, (int32_t)v->top + 1, rows);
        else
            scroll_to(v, (int32_t)v->top + (r < rows / 2 ? -(int32_t)(rows - 1) : (int32_t)(rows - 1)), rows);

        reselect(v, rows);
        return true;
    }

    const uint16_t k = hdoc_at(&v->doc, (uint16_t)(v->top + r), c);

    if (k != HDOC_NONE) {
        v->sel = k;
        follow(v, rows, k);
    }

    return true;
}

/* A link's arrows are green and its words as the text, as QBasic's; a heading is bright. */
static uint8_t cell_slot(const uint8_t kind, const uint8_t ch)
{
    if (kind == HK_LINK)
        return ch == 0x10 || ch == 0x11 ? SL_HLINK : SL_HTXT;

    return kind == HK_HEAD ? SL_HHEAD : SL_HTXT;
}

/* View v's text line `line` at row y from column x0, width columns of it, in the help's colours;
   the link chosen lit. */
static void draw_line(const HView *const v, const uint8_t y, const uint8_t x0, const uint8_t width, const uint16_t line)
{
    uint8_t ch[PSCR_COLS_MAX], kind[PSCR_COLS_MAX];
    const HelpDoc *const d = &v->doc;
    const uint16_t n = hdoc_row(d, line, ch, kind, width);
    uint8_t c = x0;

    for (uint16_t i = 0; i < n;) {
        uint16_t j = i;

        const uint8_t slot = cell_slot(kind[i], ch[i]);

        while (j < n && cell_slot(kind[j], ch[j]) == slot)
            j++;

        c = scrn_strn(y, c, ch + i, (uint16_t)(j - i), slot);
        i = j;
    }

    scrn_fill(y, c, SL_HTXT, ' ', (uint16_t)(width - n));

    if (v->sel >= 0 && d->link[v->sel].line == line)
        scrn_attr(y, (uint8_t)(x0 + d->link[v->sel].col), (uint16_t)(d->link[v->sel].ecol - d->link[v->sel].col + 1), SL_HSEL);
}

/* View v's text rows at row, cols wide: no side borders, a scroll bar in the last column when the
   topic is longer than the window. */
static void view_draw(const HView *const v, const uint8_t row, const uint8_t cols, const uint16_t rows)
{
    const HelpDoc *const d = &v->doc;
    const bool bar = d->nlines > rows && rows >= 3;
    const uint8_t text = (uint8_t)(bar ? cols - 1 : cols);

    for (uint16_t r = 0; r < rows; r++)
        draw_line(v, (uint8_t)(row + r), 0, text, (uint16_t)(v->top + r));

    if (!bar)
        return;

    const uint8_t x = (uint8_t)(cols - 1);
    scrn_fill(row, x, SL_BAR, GLYPH_UP, 1);

    for (uint16_t i = 1; i + 1 < rows; i++)
        scrn_fill((uint8_t)(row + i), x, SL_BAR, GLYPH_SHADE, 1);

    scrn_fill((uint8_t)(row + rows - 1), x, SL_BAR, GLYPH_DOWN, 1);
    const uint32_t span = d->nlines - rows ? d->nlines - rows : 1;
    scrn_fill((uint8_t)(row + 1 + v->top * (rows - 3) / span), x, SL_THB, ' ', 1);
}

/* "HELP: " and the view's title, to title. */
static const char *view_title(const HView *const v)
{
    static const char head[] = "HELP: ";
    uint32_t n = v->doc.title_len;

    if (n > sizeof title - sizeof head)
        n = sizeof title - sizeof head;

    memcpy(title, head, sizeof head - 1);
    memcpy(title + sizeof head - 1, v->doc.text, n);
    title[sizeof head - 1 + n] = 0;
    return title;
}

/* ---- the window */

static uint16_t hwin_rows(void) { return uimn.tree.help.rect.rows ? (uint16_t)(uimn.tree.help.rect.rows - 1) : 1; }

/* The window as tall as it goes. */
static void win_fit(void)
{
    wind_help(250); /* as QBasic's: every row there is, ED1 and the rest keeping their least */
}

static void win_draw(Widget *const w, Session *const S)
{
    const Rect *const r = &w->rect;

    if (!wv.shown || !r->rows)
        return;

    draw_title_in(
        SL_HTXT, r->row, 0, r->cols, r->row == 1 ? GLYPH_TOP_LEFT : GLYPH_V_RIGHT,
        r->row == 1 ? GLYPH_TOP_RIGHT : GLYPH_V_LEFT, view_title(&wv), uimn.tree.workspace.focus == FOC_HELP, uimn.win.maxed ? GLYPH_UPDOWN : GLYPH_UP,
        SL_HTTL
    );
    view_draw(&wv, (uint8_t)(r->row + 1), r->cols, (uint16_t)(r->rows - 1));
}

static bool win_key(Widget *const w, Session *const S, const uint16_t key)
{
    bool close = false;
    const uint16_t rows = hwin_rows();

    if (!view_key(&wv, rows, key, &close))
        return false;
    if (close)
        help_close();
    else
        win_fit();

    return true;
}

static bool win_mouse(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    const Rect *const r = &w->rect;

    if (uimn.mouse.event != MS_PRESS || row <= r->row)
        return false;

    view_mouse(&wv, hwin_rows(), r->cols, (uint8_t)(row - r->row - 1), col);
    win_fit();
    return true;
}

static void win_cursor(Widget *const w) { scrn_shape(PSCR_HIDDEN); }

static const WidgetOps win_ops = {.draw = win_draw, .key = win_key, .mouse = win_mouse, .cursor = win_cursor};

void help_close(void)
{
    wv.shown = false;
    wind_help(0);

    if (uimn.tree.workspace.focus == FOC_HELP)
        uimn.tree.workspace.focus = FOC_EDIT;
}

/* Topic id in the help window, which takes the focus; false: a beep. */
static bool open_window(Session *const S, const uint16_t id, const bool focus)
{
    if (!hf.open || id == HLF_NONE) {
        pscr_beep();
        return false;
    }

    edit_commit(S);
    const bool was = wv.shown;

    if (!was)
        wind_help(2); /* some rows first: show reads the window's size */
    if (!uimn.tree.help.rect.rows) { /* no room: ED1 is at its least */
        pscr_beep();
        return false;
    }

    if (!show(&wv, id, hwin_rows(), true)) {
        if (!was)
            wind_help(0);

        pscr_beep();
        return false;
    }

    win_fit();
    reselect(&wv, hwin_rows());
    if (focus)
        uimn.tree.workspace.focus = FOC_HELP;
    return true;
}

/* ---- the box */

static void box_rect(Rect *const r)
{
    r->rows = BOX_ROWS;
    r->cols = BOX_COLS;
    r->row = (uint8_t)((uimn.rows - BOX_ROWS) / 2);
    r->col = (uint8_t)((uimn.cols - BOX_COLS) / 2);
}

static void box_draw(Widget *const w, Session *const S)
{
    Rect r;
    box_rect(&r);
    scrn_box(r.row, r.col, r.rows, r.cols, SL_HTXT, view_title(&bv));
    scrn_shadow(r.row, r.col, r.rows, r.cols);

    for (uint16_t i = 0; i + 2 < r.rows; i++)
        draw_line(&bv, (uint8_t)(r.row + 1 + i), (uint8_t)(r.col + 1), (uint8_t)(r.cols - 2), (uint16_t)(bv.top + i));

    scrn_str((uint8_t)(r.row + r.rows - 1), (uint8_t)(r.col + 2), " Esc=Close  Tab=Next link  Enter=Go  Alt+F1=Back ", SL_HTXT, SL_HTXT);
}

static uint16_t box_text_rows(void) { return BOX_ROWS - 2; }

static bool box_key(Widget *const w, Session *const S, const uint16_t key)
{
    bool close = false;
    view_key(&bv, box_text_rows(), key, &close);

    if (close)
        w->visible = false;

    return true; /* a modal box takes every key */
}

static bool box_mouse(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    Rect r;
    box_rect(&r);

    if (uimn.mouse.event != MS_PRESS)
        return true;
    if (row <= r.row || row >= r.row + r.rows - 1 || col <= r.col || col >= r.col + r.cols - 1) {
        w->visible = false; /* a click outside closes it */
        return true;
    }

    const uint16_t k = hdoc_at(&bv.doc, (uint16_t)(bv.top + row - r.row - 1), (uint16_t)(col - r.col - 1));

    if (k != HDOC_NONE)
        follow(&bv, box_text_rows(), k);

    return true;
}

static const WidgetOps box_ops = {.draw = box_draw, .key = box_key, .mouse = box_mouse, .cursor = win_cursor};

bool help_box(Session *const S, const char *const key, const uint32_t len)
{
    const uint16_t id = hf.open ? hlf_find(&hf.f, key, len) : HLF_NONE;

    if (id == HLF_NONE || !show(&bv, id, box_text_rows(), true)) {
        pscr_beep();
        return false;
    }

    bv.hist.n = 1;
    bv.hist.id[0] = id;
    uimn_modal(S, &box);
    hdoc_free(&bv.doc);
    bv.shown = false;
    return true;
}

/* ---- the keys */

bool help_has(const char *const key, const uint32_t len)
{
    return hf.open && hlf_find(&hf.f, key, len) != HLF_NONE;
}

uint16_t help_keyword(char *const out, const uint16_t max)
{
    const LineBuf *const lb = uimn.tree.workspace.focus == FOC_IMM ? &uimn.immline : &uimn.edline;

    if (uimn.tree.workspace.focus == FOC_PANE || uimn.tree.workspace.focus == FOC_HELP)
        return 0;

    return hdoc_word(lb->text, lb->len, lb->col, out, max);
}

enum { QUICKR_KEY = 7 }; /* "QUICKR " */

/* A Quickr word's topic, w n characters, its dots kept: "quickr w" (a keyword, a module, a
   module's m.name), else its first part's (a module's); HLF_NONE none. */
static uint16_t quickr_topic(const char *const w, const uint16_t n)
{
    char key[QUICKR_KEY + KEY_MAX];
    memcpy(key, "QUICKR ", QUICKR_KEY);
    memcpy(key + QUICKR_KEY, w, n);
    const uint16_t id = hlf_find(&hf.f, key, QUICKR_KEY + n);
    const char *const dot = memchr(w, '.', n);

    return id != HLF_NONE || !dot ? id : hlf_find(&hf.f, key, QUICKR_KEY + (uint32_t)(dot - w));
}

/* The topic of the word at the cursor, HLF_NONE if there is none. A Quickr program's window:
   Quickr's topics only. */
static uint16_t keyword_topic(Session *const S)
{
    char w[KEY_MAX];
    const uint16_t n = hf.open ? help_keyword(w, sizeof w) : 0;

    if (!n)
        return HLF_NONE;
    if (text_quickr(S) && uimn.tree.workspace.focus == FOC_EDIT)
        return quickr_topic(w, n);

    uint16_t id = hlf_find(&hf.f, w, n);

    if (id == HLF_NONE && strchr("$%&!#", w[n - 1]))
        id = hlf_find(&hf.f, w, (uint32_t)n - 1u); /* a name's suffix, not the keyword's */

    return id;
}

bool help_topic_exists(Session *const S) { return keyword_topic(S) != HLF_NONE; }

void help_f1(Session *const S)
{
    if (uimn.tree.workspace.focus == FOC_HELP) {
        win_key(&uimn.tree.help, S, SCAN_F1 << 8);
        return;
    }

    const uint16_t id = keyword_topic(S);
    const bool quickr = id == HLF_NONE && hf.open && text_quickr(S);
    open_window(S, quickr ? hlf_find(&hf.f, "QUICKR", QUICKR_KEY - 1) : id, true);
}

void help_using(Session *const S) { open_window(S, hf.open ? hf.f.using_help : HLF_NONE, true); }
void help_contents(Session *const S) { open_window(S, hf.open ? hf.f.contents : HLF_NONE, true); }
void help_index(Session *const S) { open_window(S, hf.open ? hf.f.index : HLF_NONE, true); }

void help_back(Session *const S)
{
    if (!wv.shown)
        return pscr_beep();

    back(&wv, hwin_rows(), false);
    win_fit();
}

void help_next(Session *const S, const bool previous)
{
    if (!wv.shown)
        return pscr_beep();

    step_topic(&wv, hwin_rows(), previous);
    win_fit();
}

const StatusKeys help_keys = {
    "<Shift+F1=Help> <F6=Window> <Esc=Cancel> <Ctrl+F1=Next> <Alt+F1=Back>",
    {{9, nullptr}},
};

void help_build(Widget *const workspace, Widget *const screen)
{
    uimn.tree.help = (Widget){.ops = &win_ops};
    tree_add(workspace, &uimn.tree.help);
    box = (Widget){.ops = &box_ops, .modal = true};
    tree_add(screen, &box);
}
