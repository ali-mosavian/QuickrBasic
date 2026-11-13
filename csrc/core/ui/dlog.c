/* Dialog boxes: the engine, message boxes, Open, Save As, Find, Change (src/ui/dlg.asm). */
#include "ui/dlog.h"
#include "plat/pfil.h"
#include "plat/pkey.h"
#include "plat/pscr.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/menu.h"
#include "ui/mous.h"
#include "ui/scrn.h"
#include "ui/uimn.h"

enum {
    LSMAX = 128, /* a list's names at most */
    NAMESZ = 13, /* a list's item: NAME.EXT and 0 */
    DL_WRAP = 44,
    NMSGCTL = 16,
    FRAME = 2,       /* a box's two sides */
    FIELD_ROWS = 3,  /* a field's box */
    BTN_PAD = 4,     /* a button's width past its text when none given: "< " and " >" */
};

/* A message box's layout (msgbox.c): its lines from MSG_TOP, the separator below them, the
   buttons MSG_GAP apart under it. */
enum { MSG_TOP = 2, MSG_TCOL = 3, MSG_GAP = 3, MSG_PAD = 6 };

static const char s_open[] = "Open", s_saveas[] = "Save As", s_find[] = "Find",
                  s_change[] = "Change", s_fname[] = "File &Name:", s_files[] = "&Files",
                  s_dirs[] = "&Dirs/Drives", s_yes[] = "&Yes", s_no[] = "&No", s_skip[] = "&Skip",
                  s_chg1[] = "&Change", s_fwhat[] = "&Find What:", s_cto[] = "Change &To:",
                  s_case[] = "&Match Upper/Lowercase", s_whole[] = "&Whole Word",
                  s_fverify[] = "Find and &Verify", s_chall[] = "&Change All";

static const MsgButton bt_ok[] = {{uimn_ok, 8}, {uimn_helpbtn, 8}, {}};
static const MsgButton bt_ync[] = {{s_yes, 7}, {s_no, 8}, {uimn_cancel, 8}, {uimn_helpbtn, 8}, {}};

static struct {
    Dlog *d;        /* dl_bx: the dialog running */
    uint16_t cur;   /* dl_cur: the cursor's place, row << 8 | col; PSCR_HIDDEN none */
    bool focused;   /* dl_hf: the control drawing has the focus */
    bool fresh;     /* dl_new: the focus just came: a field's text selected till a key */
    bool alt;       /* dl_alt: Alt was down: the hotkeys lit */
    uint8_t hitrow; /* dl_hr: dl_hit's row in the control */
    uint8_t nlines; /* the message box's: its lines */
    uint8_t b0;     /*   its first button's control */
    bool adjust;    /*   msg_adj: off the cursor's line */
    const char *title; /* msg_ttl: its title, nullptr none: its lines then left-aligned */
} dl;

static char files[LSMAX * NAMESZ], dirs[LSMAX * NAMESZ];
static DlogList ls_files = {.items = files, .size = NAMESZ};
static DlogList ls_dirs = {.items = dirs, .size = NAMESZ};
static char mask[80];    /* dl_mask: the Open box's */
static char path[80];    /* dl_path: the current directory */
static PfilFind dta;     /* dl_dta */
static char msg_text[640];
static DlogCtl msg_ctl[NMSGCTL];
static Dlog msg_dlog = {.ctl = msg_ctl};

static bool files_pick(uint8_t why);
static bool dirs_pick(uint8_t why);

/* The boxes as QBasic's (uifile.des, uisearch.des). */
static DlogCtl open_ctl[] = {
    {.kind = CT_FLD, .row = 1, .col = 13, .cols = 50, .rows = 1, .field = &uimn.fname,
     .label = s_fname, .lrow = 2, .lcol = 2},
    {.kind = CT_TXT, .row = 4, .col = 2, .flags = CF_NOF, .text = path},
    {.kind = CT_LST, .row = 6, .col = 2, .cols = 43, .rows = 8, .list = &ls_files,
     .hook = files_pick, .label = s_files, .lrow = 5, .lcol = 22},
    {.kind = CT_LST, .row = 6, .col = 49, .cols = 14, .rows = 8, .list = &ls_dirs,
     .hook = dirs_pick, .label = s_dirs, .lrow = 5, .lcol = 51},
    {.kind = CT_BTN, .row = 18, .col = 12, .cols = 6, .text = uimn_ok, .hook = dlog_fileok},
    {.kind = CT_BTN, .row = 18, .col = 28, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 18, .col = 48, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_open = {2, 6, 20, 67, s_open, 7, 0, 17, DO_OK, open_ctl};

static DlogCtl save_ctl[] = {
    {.kind = CT_FLD, .row = 1, .col = 13, .cols = 25, .rows = 1, .field = &uimn.fname,
     .label = s_fname, .lrow = 2, .lcol = 2},
    {.kind = CT_TXT, .row = 4, .col = 2, .flags = CF_NOF, .text = path},
    {.kind = CT_LST, .row = 6, .col = 13, .cols = 14, .rows = 7, .list = &ls_dirs,
     .hook = dirs_pick, .label = s_dirs, .lrow = 5, .lcol = 15},
    {.kind = CT_BTN, .row = 16, .col = 7, .cols = 6, .text = uimn_ok, .hook = dlog_fileok},
    {.kind = CT_BTN, .row = 16, .col = 17, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 16, .col = 31, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_save = {3, 19, 18, 42, s_saveas, 6, 0, 15, DS_OK, save_ctl};

static DlogCtl find_ctl[] = {
    {.kind = CT_FLD, .row = 1, .col = 13, .cols = 42, .rows = 1, .field = &uimn.find_what,
     .label = s_fwhat, .lrow = 2, .lcol = 2},
    {.kind = CT_CHK, .row = 6, .col = 5, .text = s_case, .check = &uimn.match_case},
    {.kind = CT_CHK, .row = 6, .col = 38, .text = s_whole, .check = &uimn.whole_word},
    {.kind = CT_BTN, .row = 9, .col = 9, .cols = 6, .text = uimn_ok},
    {.kind = CT_BTN, .row = 9, .col = 24, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 9, .col = 42, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_find = {7, 10, 11, 59, s_find, 6, 0, 8, DF_OK, find_ctl};

static DlogCtl conf_ctl[] = { /* Find and Verify's question (uisearch.des dlgConfirm) */
    {.kind = CT_BTN, .row = 3, .col = 4, .cols = 10, .text = s_chg1},
    {.kind = CT_BTN, .row = 3, .col = 16, .cols = 8, .text = s_skip},
    {.kind = CT_BTN, .row = 3, .col = 28, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 3, .col = 41, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_conf = {19, 13, 5, 53, s_change, 4, 0, 2, 0, conf_ctl};
enum { CONF_LOW = 19, CONF_HIGH = 2 }; /* its rows: low unless the cursor's line is there */

static DlogCtl chg_ctl[] = {
    {.kind = CT_FLD, .row = 1, .col = 13, .cols = 41, .rows = 1, .field = &uimn.find_what,
     .label = s_fwhat, .lrow = 2, .lcol = 2},
    {.kind = CT_FLD, .row = 4, .col = 13, .cols = 41, .rows = 1, .field = &uimn.change_to,
     .label = s_cto, .lrow = 5, .lcol = 2},
    {.kind = CT_CHK, .row = 9, .col = 5, .text = s_case, .check = &uimn.match_case},
    {.kind = CT_CHK, .row = 9, .col = 37, .text = s_whole, .check = &uimn.whole_word},
    {.kind = CT_BTN, .row = 12, .col = 2, .cols = 19, .text = s_fverify},
    {.kind = CT_BTN, .row = 12, .col = 22, .cols = 14, .text = s_chall},
    {.kind = CT_BTN, .row = 12, .col = 37, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 12, .col = 48, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_chg = {5, 11, 14, 58, s_change, 8, 0, 11, DC_FV, chg_ctl};

/* ---- drawing */

/* dl_hk: the hotkeys' slot: lit once Alt was down in the dialog, as COW's; else slot. */
static uint8_t hotslot(const uint8_t slot) { return dl.alt ? SL_DFOC : slot; }

static uint16_t place(const uint8_t row, const uint8_t col) { return (uint16_t)(row << 8 | col); }

static void draw_field(const DlogCtl *const c, const uint8_t top, const uint8_t left)
{
    scrn_box(top, left, FIELD_ROWS, c->cols + FRAME, SL_DLG, nullptr);
    const uint8_t row = top + 1, col = left + 1;
    const LineBuf *const b = c->field;
    const uint16_t first = b->col >= c->cols ? b->col - c->cols + 1 : 0; /* the first column shown */
    const uint16_t rest = b->len >= first ? b->len - first : 0;
    const uint16_t n = rest > c->cols ? c->cols : rest;
    scrn_strn(row, col, b->text + first, n, SL_DLG);
    if (!dl.focused)
        return;
    if (dl.fresh)
        scrn_attr(row, col, n, SL_DSEL);
    dl.cur = place(row, (uint8_t)(col + (b->col - first)));
}

static void draw_list(const DlogCtl *const c, const uint8_t top, const uint8_t left)
{
    scrn_box(top, left, c->rows + FRAME, c->cols + FRAME, SL_DLG, nullptr);
    const uint8_t col = left + 1;
    DlogList *const l = c->list;
    if (l->sel < l->top) /* the first shown keeps it in view */
        l->top = l->sel;
    const uint16_t end = l->top + c->rows;
    if (l->sel >= end)
        l->top += l->sel - end + 1;
    for (uint8_t r = 0; r < c->rows; r++) {
        const uint8_t row = (uint8_t)(top + 1 + r);
        const uint16_t i = l->top + r;
        if (i >= l->n)
            continue;
        scrn_str(row, col, l->items + (uint32_t)i * l->size, SL_DLG, SL_DLG);
        if (i == l->sel && dl.focused) {
            scrn_attr(row, col, c->cols, SL_DSEL);
            dl.cur = place(row, col);
        }
    }
}

/* dl_bw's for a control: its width, its text's + BTN_PAD when none. */
static uint8_t width(const char *const text, const uint8_t cols)
{
    return cols ? cols : (uint8_t)(scrn_slen(text) + BTN_PAD);
}

static void draw_button(const DlogCtl *const c, const uint8_t row, uint8_t col)
{
    const uint8_t len = (uint8_t)scrn_slen(c->text);
    const uint8_t inner = (uint8_t)(width(c->text, c->cols) - 2 - len); /* the text centred */
    const uint8_t left = (uint8_t)(inner + 1) >> 1, right = inner - left;
    const uint8_t edge = dl.focused ? SL_DFOC : SL_DLG;
    col = scrn_fill(row, col, edge, '<', 1);
    col = scrn_fill(row, col, SL_DLG, ' ', left);
    if (dl.focused)
        dl.cur = place(row, col);
    col = scrn_str(row, col, c->text, SL_DLG, hotslot(SL_DLG));
    col = scrn_fill(row, col, SL_DLG, ' ', right);
    scrn_fill(row, col, edge, '>', 1);
}

static void draw_check(const DlogCtl *const c, const uint8_t row, uint8_t col)
{
    col = scrn_fill(row, col, SL_DLG, '[', 1);
    if (dl.focused)
        dl.cur = place(row, col);
    col = scrn_fill(row, col, SL_DLG, *c->check ? 'X' : ' ', 1);
    col = scrn_fill(row, col, SL_DLG, ']', 1);
    col = scrn_fill(row, col, SL_DLG, ' ', 1);
    scrn_str(row, col, c->text, SL_DLG, hotslot(SL_DLG));
}

/* dl_one */
static void draw_ctl(const Dlog *const d, const DlogCtl *const c)
{
    if (c->label)
        scrn_str(d->row + c->lrow, d->col + c->lcol, c->label, SL_DLG, hotslot(SL_DLG));
    const uint8_t row = d->row + c->row, col = d->col + c->col;
    switch (c->kind) {
    case CT_FLD:
        draw_field(c, row, col);
        break;
    case CT_LST:
        draw_list(c, row, col);
        break;
    case CT_BTN:
        draw_button(c, row, col);
        break;
    case CT_CHK:
        draw_check(c, row, col);
        break;
    default:
        scrn_str(row, col, c->text, SL_DLG, SL_DLG);
    }
}

/* dl_draw: the frame, the shadow, the separator, the controls, the cursor. */
static void draw(const Dlog *const d)
{
    scrn_box(d->row, d->col, d->rows, d->cols, SL_DLG, d->title);
    scrn_shadow(d->row, d->col, d->rows, d->cols);
    if (d->sep)
        scrn_sep(d->row + d->sep, d->col, d->cols, SL_DLG);
    dl.cur = PSCR_HIDDEN;
    for (uint8_t i = 0; i < d->n; i++) {
        dl.focused = i == d->focus;
        draw_ctl(d, &d->ctl[i]);
    }
    if (dl.cur == PSCR_HIDDEN) {
        scrn_shape(PSCR_HIDDEN);
        return;
    }
    scrn_shape(CURSOR_INSERT);
    scrn_cursor((uint8_t)(dl.cur >> 8), (uint8_t)dl.cur);
}

/* ---- the engine */

/* dl_acc: s's hotkey, upper case: the letter after '&'; 0 none. */
static uint8_t hotkey(const char *const s)
{
    if (!s)
        return 0;
    for (size_t i = 0; s[i]; i++)
        if (s[i] == '&')
            return upcase((uint8_t)s[i + 1]);
    return 0;
}

/* The text c's hotkey is in: a button's, a check box's own; a text's none; else its label. */
static const char *hot_text(const DlogCtl *const c)
{
    if (c->kind == CT_BTN || c->kind == CT_CHK)
        return c->text;
    return c->kind == CT_TXT ? nullptr : c->label;
}

/* dl_hit: the mouse on control c? dl.hitrow its row in it. */
static bool hit(const Dlog *const d, const DlogCtl *const c)
{
    const int r = uimn.mouse.row - d->row - c->row;
    const int x = uimn.mouse.col - d->col - c->col;
    if (r < 0 || x < 0)
        return false;
    int h, w;
    switch (c->kind) {
    case CT_FLD:
        h = FIELD_ROWS;
        w = c->cols + FRAME;
        break;
    case CT_LST:
        h = c->rows + FRAME;
        w = c->cols + FRAME;
        break;
    case CT_TXT:
        return false;
    default:
        h = 1;
        w = (uint8_t)(scrn_slen(c->text) + BTN_PAD);
    }
    if (r >= h || x >= w)
        return false;
    dl.hitrow = (uint8_t)r;
    return true;
}

/* dl_step: the focus to the next control that takes it, dir 1, or the one before, -1. */
static void step(Dlog *const d, const int dir)
{
    int i = d->focus;
    do {
        i += dir;
        if (i < 0)
            i = d->n - 1;
        if (i >= d->n)
            i = 0;
    } while (d->ctl[i].flags & CF_NOF);
    d->focus = (uint8_t)i;
    dl.fresh = true;
}

void dlog_ledit(LineBuf *const b, const uint16_t key)
{
    const uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);
    if (ch == CH_BACKSPACE) {
        edit_lb_bs(b);
        return;
    }
    if (ch && ch != CH_E0) {
        if (ch >= ' ')
            edit_lb_type(b, ch, FLDMAX);
        return;
    }
    switch (scan) {
    case SCAN_LEFT: edit_lb_left(b); break;
    case SCAN_RIGHT: edit_lb_right(b); break;
    case SCAN_HOME: edit_lb_home(b); break;
    case SCAN_END: edit_lb_end(b); break;
    case SCAN_DEL: edit_lb_del(b); break;
    case SCAN_CTRL_LEFT: edit_lb_wleft(b); break;
    case SCAN_CTRL_RIGHT: edit_lb_wright(b); break;
    default: break;
    }
}

/* What a key or a click leads to: GO_LOOP nothing more; GO_ESC the box ends. */
enum : uint8_t { GO_LOOP, GO_PRESS, GO_ENTER, GO_CHECK, GO_SET, GO_ESC };

/* A click on a list's line: once selects (*sel), on the selected one opens. */
static uint8_t list_click(const DlogCtl *const c, uint16_t *const sel)
{
    const int r = dl.hitrow - 1;
    if (r < 0 || r >= c->rows)
        return GO_LOOP;
    const DlogList *const l = c->list;
    const uint16_t at = (uint16_t)(l->top + r);
    if (at >= l->n)
        return GO_LOOP;
    if (at == l->sel)
        return GO_ENTER;
    *sel = at;
    return GO_SET;
}

/* A press on control i (*c): a button pressed, else the focus. */
static uint8_t click(Dlog *const d, const uint8_t i, DlogCtl **const pc, uint16_t *const sel)
{
    DlogCtl *const c = &d->ctl[i];
    *pc = c;
    if (c->flags & CF_NOF)
        return GO_LOOP;
    if (c->kind == CT_BTN)
        return GO_PRESS;
    d->focus = i;
    dl.fresh = false;
    if (c->kind == CT_CHK)
        return GO_CHECK;
    if (c->kind == CT_LST)
        return list_click(c, sel);
    return GO_LOOP;
}

static uint8_t on_mouse(Dlog *const d, DlogCtl **const pc, uint16_t *const sel)
{
    if (uimn.mouse.event != MS_PRESS)
        return GO_LOOP;
    for (uint8_t i = 0; i < d->n; i++)
        if (hit(d, &d->ctl[i]))
            return click(d, i, pc, sel);
    return GO_LOOP;
}

/* A hotkey, from the next control on (COW dlgcore.c): a button pressed (*pc), a box checked,
   else the focus; none: beep if asked. */
static uint8_t on_hotkey(Dlog *const d, const uint8_t key, const bool beep, DlogCtl **const pc)
{
    const uint8_t letter = upcase(key);
    uint8_t i = d->focus;
    for (uint8_t k = 0; k < d->n; k++) {
        if (++i >= d->n)
            i = 0;
        DlogCtl *const c = &d->ctl[i];
        if (hotkey(hot_text(c)) != letter)
            continue;
        *pc = c;
        if (c->kind == CT_BTN)
            return GO_PRESS;
        d->focus = i;
        dl.fresh = true;
        return c->kind == CT_CHK ? GO_CHECK : GO_LOOP;
    }
    if (beep)
        pscr_beep();
    return GO_LOOP;
}

/* A key in a field: a character replaces the text selected. */
static void field_key(LineBuf *const b, const uint16_t key)
{
    if (dl.fresh) {
        dl.fresh = false;
        if ((uint8_t)key >= ' ')
            b->len = b->col = 0;
    }
    dlog_ledit(b, key);
}

/* A key in a list: the selection to move to (*sel). */
static uint8_t list_key(const DlogCtl *const c, const uint8_t scan, uint16_t *const sel)
{
    const uint16_t s = c->list->sel;
    switch (scan) {
    case SCAN_UP:
        if (!s)
            return GO_LOOP;
        *sel = s - 1;
        break;
    case SCAN_DOWN: *sel = (uint16_t)(s + 1); break;
    case SCAN_PGUP: *sel = s >= c->rows ? s - c->rows : 0; break;
    case SCAN_HOME: *sel = 0; break;
    case SCAN_PGDN: *sel = (uint16_t)(s + c->rows); break;
    case SCAN_END: *sel = 0xFFFF; break;
    default: return GO_LOOP;
    }
    return GO_SET;
}

static uint8_t on_key(Dlog *const d, uint16_t key, DlogCtl **const pc, uint16_t *const sel)
{
    const uint8_t ch = (uint8_t)key;
    uint8_t scan = (uint8_t)(key >> 8);
    if (uimn.keyflags & PKEY_ALT) { /* Alt: the letters lit; Alt+letter */
        dl.alt = true;
        if (!ch) {
            const uint8_t alt_scan = scan;
            scan -= SCAN_ALT_FIRST;
            key = (uint16_t)(scan << 8 | ch);
            if (alt_scan >= SCAN_ALT_FIRST && alt_scan <= SCAN_ALT_LAST) /* unmatched beeps */
                return on_hotkey(d, menu_altchar(alt_scan), true, pc);
        }
    }
    DlogCtl *const c = *pc;
    if (key == KEY_ALT) {
        dl.alt = true;
        return GO_LOOP;
    }
    if (ch == CH_ESC)
        return GO_ESC;
    if (ch == CH_TAB) {
        step(d, 1);
        return GO_LOOP;
    }
    if (key == KEY_SHIFT_TAB) {
        step(d, -1);
        return GO_LOOP;
    }
    if (ch == CH_ENTER)
        return GO_ENTER;
    if (c->kind == CT_FLD) {
        field_key(c->field, key);
        return GO_LOOP;
    }
    if (c->kind == CT_LST)
        return list_key(c, scan, sel);
    if (ch != ' ')
        return on_hotkey(d, ch, false, pc);
    if (c->kind == CT_CHK)
        return GO_CHECK;
    if (c->kind == CT_BTN)
        return GO_PRESS;
    return GO_LOOP;
}

/* A list's selection to sel, at most its last. */
static void list_set(const DlogCtl *const c, const uint16_t sel)
{
    DlogList *const l = c->list;
    if (!l->n)
        return;
    l->sel = sel >= l->n ? l->n - 1 : sel;
    if (c->hook)
        c->hook(DLOG_MOVED);
}

/* go done on c; true: the box ends with *r, the button pressed. */
static bool act(const Dlog *const d, const uint8_t go, DlogCtl *c, const uint16_t sel,
                uint8_t *const r)
{
    switch (go) {
    case GO_CHECK:
        *c->check = !*c->check;
        return false;
    case GO_SET:
        list_set(c, sel);
        return false;
    case GO_ENTER:
        if (c->kind != CT_BTN) {
            if (c->kind == CT_LST && c->hook && c->hook(DLOG_ENTER))
                return false;
            c = &d->ctl[d->def];
        }
        [[fallthrough]];
    case GO_PRESS:
        if (c->hook && c->hook(DLOG_PRESS))
            return false;
        *r = (uint8_t)(c - d->ctl);
        return true;
    default:
        return false;
    }
}

/* dl_run0 */
static uint8_t run(Dlog *const d)
{
    dl.d = d;
    dl.alt = pkey_shift() & PKEY_ALT; /* Alt down already: its letters lit */
    dl.fresh = true;
    for (;;) {
        draw_all();
        draw(d);
        const uint16_t key = uimn_getkey();
        DlogCtl *c = &d->ctl[d->focus];
        uint16_t sel = 0; /* GO_SET's */
        const uint8_t go = key == KEY_MOUSE ? on_mouse(d, &c, &sel) : on_key(d, key, &c, &sel);
        if (go == GO_ESC)
            return DLOG_ESC;
        uint8_t r;
        if (act(d, go, c, sel, &r))
            return r;
    }
}

uint8_t dlog_run(Dlog *const d)
{
    uimn.help = uimn_dlog_help;
    const uint8_t r = run(d);
    uimn.help = nullptr;
    return r;
}

/* ---- message boxes */

/* text's lines, split at '|', to msg_text, 0 each; their count, *widest the widest's width. */
static uint8_t split_lines(const char *const text, uint8_t *const widest)
{
    uint8_t nl = 0, w = 0;
    *widest = 0;
    for (size_t i = 0;; i++) {
        const char c = text[i] == '|' ? 0 : text[i];
        msg_text[i] = c;
        if (c) {
            w++;
            continue;
        }
        nl++;
        if (w > *widest)
            *widest = w;
        w = 0;
        if (!text[i])
            return nl;
    }
}

/* The buttons' row: MSG_GAP apart. */
static uint16_t buttons_width(const MsgButton *const buttons)
{
    uint16_t bw = (uint16_t)-MSG_GAP;
    for (size_t i = 0; buttons[i].text; i++)
        bw += width(buttons[i].text, buttons[i].cols) + MSG_GAP;
    return bw;
}

/* The box's top row: centred; at the foot when over the error's line. */
static uint8_t msg_row(const uint8_t rows)
{
    const uint8_t row = (uint8_t)(PSCR_ROWS - rows) >> 1;
    if (!dl.adjust)
        return row;
    edit_ensure();
    const uint8_t at = (uint8_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0);
    if (at >= row && at < (uint8_t)(row + rows))
        return (uint8_t)(PSCR_ROWS - 1 - rows);
    return row;
}

/* The lines as text controls, from msg_ctl[0]. */
static void msg_lines(const Dlog *const m, const uint8_t nl)
{
    const char *s = msg_text;
    for (uint8_t i = 0; i < nl; i++) {
        DlogCtl *const c = &msg_ctl[i];
        *c = (DlogCtl){.kind = CT_TXT, .row = MSG_TOP + i, .flags = CF_NOF, .text = s};
        c->col = dl.title ? MSG_TCOL : (uint8_t)(m->cols - scrn_slen(s)) >> 1;
        s += strlen(s) + 1;
    }
}

/* The buttons as controls, from msg_ctl[i], on row, centred inside; past the last. */
static uint8_t msg_buttons(const Dlog *const m, const MsgButton *const buttons, uint8_t i,
                           const uint8_t row, const uint16_t bw)
{
    uint8_t col = (uint8_t)((uint8_t)(m->cols - 2 - (uint8_t)bw) >> 1) + 1;
    for (size_t k = 0; buttons[k].text; k++, i++) {
        const MsgButton *const b = &buttons[k];
        msg_ctl[i] = (DlogCtl){.kind = CT_BTN, .row = row, .col = col, .cols = b->cols,
                               .text = b->text,
                               .hook = b->text == uimn_helpbtn ? dlog_nohelp : nullptr};
        col += width(b->text, b->cols) + MSG_GAP;
    }
    return i;
}

uint8_t dlog_msg(const char *const text, const MsgButton *const buttons)
{
    uint8_t widest;
    const uint8_t nl = split_lines(text, &widest);
    dl.nlines = nl;
    const uint16_t bw = buttons_width(buttons);
    if (dl.title) { /* the title: its spaces too */
        const uint16_t t = scrn_slen(dl.title) + 2;
        if (widest < t)
            widest = (uint8_t)t;
    }
    const uint16_t cols = (widest >= bw ? widest : bw) + MSG_PAD; /* the wider (msgbox.c) */
    Dlog *const m = &msg_dlog;
    m->cols = (uint8_t)cols;
    m->col = (uint8_t)(PSCR_COLS - m->cols) >> 1;
    m->rows = MSG_TOP + nl + 4;
    m->row = msg_row(m->rows);
    m->title = dl.title;
    m->sep = MSG_TOP + nl + 1;
    msg_lines(m, nl);
    m->focus = m->def = dl.b0 = nl;
    m->n = msg_buttons(m, buttons, nl, MSG_TOP + nl + 2, bw);
    uint8_t r = dlog_run(m);
    if (r != DLOG_ESC)
        r -= dl.b0;
    uimn.help = nullptr;
    return r;
}

bool dlog_nohelp(const uint8_t why)
{
    pscr_beep();
    return true;
}

void dlog_err(char *text)
{
    dl.adjust = true;
    char *const bar = strchr(text, '|');
    if (bar) {
        *bar = 0;
        dl.title = text;
        text = bar + 1;
    }
    dlog_ok(text);
    dl.title = nullptr;
    dl.adjust = false;
    uimn.ed.sel_on = false; /* the error's word, shown till now */
}

uint8_t *dlog_wrap(uint8_t *dst, const uint8_t *const src, const uint32_t len,
                   const uint8_t indent)
{
    memset(dst, ' ', indent);
    dst += indent;
    uint32_t col = indent;
    uint8_t *space = nullptr; /* the last space */
    for (uint32_t i = 0; i < len; i++) {
        const uint8_t c = src[i] == '|' ? GLYPH_V : src[i];
        *dst++ = c;
        col++;
        if (c == ' ')
            space = dst - 1;
        if (col < DL_WRAP || !space)
            continue;
        *space = '|'; /* the rest of the line past the indent */
        const uint32_t n = (uint32_t)(dst - space - 1);
        memmove(space + 1 + indent, space + 1, n);
        memset(space + 1, ' ', indent);
        dst += indent;
        col = (uint32_t)(dst - space - 1);
        space = nullptr;
    }
    return dst;
}

void dlog_ok(const char *const text) { dlog_msg(text, bt_ok); }

uint8_t dlog_ync(const char *const text)
{
    const uint8_t r = dlog_msg(text, bt_ync);
    return r == DLOG_ESC ? 2 : r;
}

/* ---- Open, Save As */

/* The names matching pattern with attrs (PFIL_ADIR: directories only, "." left out) to list's
   items; their count. */
static uint16_t find(char *const items, const char *const pattern, const uint8_t attrs)
{
    uint16_t n = 0;
    for (uint16_t e = pfil_first(pattern, attrs, &dta); e == PFIL_OK; e = pfil_next(&dta)) {
        if (attrs == PFIL_ADIR && (!(dta.attr & PFIL_ADIR) || !strcmp(dta.name, ".")))
            continue;
        if (n >= LSMAX)
            break;
        memcpy(items + n * NAMESZ, dta.name, NAMESZ);
        n++;
    }
    return n;
}

/* dl_files: the lists: the files matching the mask, the directories; the current path. */
static void list_files(void)
{
    ls_files.n = find(files, mask, 0);
    ls_dirs.n = find(dirs, "*.*", PFIL_ADIR);
    ls_files.sel = ls_files.top = ls_dirs.sel = ls_dirs.top = 0;
    path[0] = (char)('A' + pfil_curdrive()); /* drive, colon, \, directory */
    path[1] = ':';
    path[2] = '\\';
    pfil_curdir(0, path + 3, sizeof path - 3);
}

/* dl_name1: the field set to s. */
static void set_field(const char *const s)
{
    const uint16_t n = (uint16_t)strlen(s);
    memcpy(uimn.fname.text, s, n);
    uimn.fname.len = uimn.fname.col = n;
}

/* dl_fsel: the Files list: the selected name to the field; Enter too, then OK. */
static bool files_pick(const uint8_t why)
{
    if (!ls_files.n)
        return false;
    set_field(files + ls_files.sel * NAMESZ);
    return why == DLOG_MOVED;
}

/* dl_dsel: the Dirs list: Enter goes into the selected one, the box stays. */
static bool dirs_pick(const uint8_t why)
{
    if (why != DLOG_ENTER || !ls_dirs.n)
        return false;
    pfil_chdir(dirs + ls_dirs.sel * NAMESZ);
    list_files();
    return true;
}

bool dlog_fileok(const uint8_t why)
{
    const uint16_t len = uimn.fname.len;
    if (!len)
        return true;
    char *const d = uimn.chosen;
    bool wild = false, dot = false;
    for (uint16_t i = 0; i < len; i++) {
        const uint8_t c = upcase(uimn.fname.text[i]);
        if (c == '*' || c == '?')
            wild = true;
        if (c == '.')
            dot = true;
        if (c == '\\')
            dot = false;
        d[i] = (char)c;
    }
    uint16_t end = len;
    if (!dot) {
        memcpy(d + end, ".BAS", 4);
        end += 4;
    }
    d[end] = 0;
    if (!wild)
        return false;
    memcpy(mask, uimn.chosen, 64); /* a mask: list it */
    list_files();
    return true;
}

bool dlog_open(void)
{
    memcpy(mask, "*.BAS", 6);
    set_field(mask);
    list_files();
    dg_open.focus = 0;
    return dlog_run(&dg_open) == DO_OK;
}

bool dlog_saveas(void)
{
    set_field(uimn.file);
    memcpy(mask, "*.*", 4);
    list_files();
    dg_save.focus = 0;
    return dlog_run(&dg_save) == DS_OK;
}

/* ---- Find, Change */

bool dlog_find(void)
{
    dg_find.focus = 0;
    return dlog_run(&dg_find) == DF_OK;
}

uint8_t dlog_conf(void)
{
    edit_ensure();
    const uint16_t at = (uint16_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0);
    dg_conf.row = at < CONF_LOW ? CONF_LOW : CONF_HIGH;
    dg_conf.focus = 0;
    return dlog_run(&dg_conf);
}

uint8_t dlog_change(void)
{
    dg_chg.focus = 0;
    return (uint8_t)(dlog_run(&dg_chg) - DC_FV);
}
