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
};

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
static uint8_t hotslot(uint8_t slot) { return dl.alt ? SL_DFOC : slot; }

static void draw_field(const DlogCtl *c, uint8_t row, uint8_t col)
{
    scrn_box(row, col, 3, c->cols + 2, SL_DLG, nullptr);
    row++;
    col++;
    const LineBuf *b = c->field;
    uint16_t first = b->col >= c->cols ? b->col - c->cols + 1 : 0; /* the first column shown */
    uint16_t n = b->len >= first ? b->len - first : 0;
    if (n > c->cols)
        n = c->cols;
    scrn_strn(row, col, b->text + first, n, SL_DLG);
    if (!dl.focused)
        return;
    if (dl.fresh)
        scrn_attr(row, col, n, SL_DSEL);
    dl.cur = (uint16_t)(row << 8 | (uint8_t)(col + (b->col - first)));
}

static void draw_list(const DlogCtl *c, uint8_t row, uint8_t col)
{
    scrn_box(row, col, c->rows + 2, c->cols + 2, SL_DLG, nullptr);
    row++;
    col++;
    DlogList *l = c->list;
    if (l->sel < l->top) /* the first shown keeps it in view */
        l->top = l->sel;
    uint16_t end = l->top + c->rows;
    if (l->sel >= end)
        l->top += l->sel - end + 1;
    for (uint8_t r = 0; r < c->rows; r++, row++) {
        uint16_t i = l->top + r;
        if (i >= l->n)
            continue;
        scrn_str(row, col, l->items + (uint32_t)i * l->size, SL_DLG, SL_DLG);
        if (i == l->sel && dl.focused) {
            scrn_attr(row, col, c->cols, SL_DSEL);
            dl.cur = (uint16_t)(row << 8 | col);
        }
    }
}

/* dl_bw's for a control: its width, its text's + 4 when none. */
static uint8_t width(const char *text, uint8_t cols)
{
    return cols ? cols : (uint8_t)(scrn_slen(text) + 4);
}

static void draw_button(const DlogCtl *c, uint8_t row, uint8_t col)
{
    uint8_t len = (uint8_t)scrn_slen(c->text);
    uint8_t inner = (uint8_t)(width(c->text, c->cols) - 2 - len); /* the text centred */
    uint8_t left = (uint8_t)(inner + 1) >> 1, right = inner - left;
    uint8_t edge = dl.focused ? SL_DFOC : SL_DLG;
    col = scrn_fill(row, col, edge, '<', 1);
    col = scrn_fill(row, col, SL_DLG, ' ', left);
    if (dl.focused)
        dl.cur = (uint16_t)(row << 8 | col);
    col = scrn_str(row, col, c->text, SL_DLG, hotslot(SL_DLG));
    col = scrn_fill(row, col, SL_DLG, ' ', right);
    scrn_fill(row, col, edge, '>', 1);
}

static void draw_check(const DlogCtl *c, uint8_t row, uint8_t col)
{
    col = scrn_fill(row, col, SL_DLG, '[', 1);
    if (dl.focused)
        dl.cur = (uint16_t)(row << 8 | col);
    col = scrn_fill(row, col, SL_DLG, *c->check ? 'X' : ' ', 1);
    col = scrn_fill(row, col, SL_DLG, ']', 1);
    col = scrn_fill(row, col, SL_DLG, ' ', 1);
    scrn_str(row, col, c->text, SL_DLG, hotslot(SL_DLG));
}

/* dl_one */
static void draw_ctl(const Dlog *d, const DlogCtl *c)
{
    if (c->label)
        scrn_str(d->row + c->lrow, d->col + c->lcol, c->label, SL_DLG, hotslot(SL_DLG));
    uint8_t row = d->row + c->row, col = d->col + c->col;
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
static void draw(const Dlog *d)
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
    scrn_shape(0x0607);
    scrn_cursor((uint8_t)(dl.cur >> 8), (uint8_t)dl.cur);
}

/* ---- the engine */

/* dl_acc: s's hotkey, upper case: the letter after '&'; 0 none. */
static uint8_t hotkey(const char *s)
{
    if (!s)
        return 0;
    for (;;) {
        uint8_t c = (uint8_t)*s++;
        if (!c)
            return 0;
        if (c == '&')
            return uimn_upper((uint8_t)*s);
    }
}

/* dl_hit: the mouse on control c? dl.hitrow its row in it. */
static bool hit(const Dlog *d, const DlogCtl *c)
{
    int r = uimn.mouse.row - d->row - c->row;
    int x = uimn.mouse.col - d->col - c->col;
    if (r < 0 || x < 0)
        return false;
    int h, w;
    switch (c->kind) {
    case CT_FLD:
        h = 3;
        w = c->cols + 2;
        break;
    case CT_LST:
        h = c->rows + 2;
        w = c->cols + 2;
        break;
    case CT_TXT:
        return false;
    default:
        h = 1;
        w = (uint8_t)(scrn_slen(c->text) + 4);
    }
    if (r >= h || x >= w)
        return false;
    dl.hitrow = (uint8_t)r;
    return true;
}

/* dl_step: the focus to the next control that takes it, dir 1, or the one before, -1. */
static void step(Dlog *d, int dir)
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

void dlog_ledit(LineBuf *b, uint16_t key)
{
    uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);
    if (ch == 8) {
        edit_lb_bs(b);
        return;
    }
    if (ch && ch != 0xE0) {
        if (ch >= ' ')
            edit_lb_type(b, ch, FLDMAX);
        return;
    }
    switch (scan) {
    case 0x4B: edit_lb_left(b); break;
    case 0x4D: edit_lb_right(b); break;
    case 0x47: edit_lb_home(b); break;
    case 0x4F: edit_lb_end(b); break;
    case 0x53: edit_lb_del(b); break;
    case 0x73: edit_lb_wleft(b); break;
    case 0x74: edit_lb_wright(b); break;
    default: break;
    }
}

/* What a key or a click leads to. */
enum { GO_LOOP, GO_PRESS, GO_ENTER, GO_CHECK, GO_SET };

/* dl_run0 */
static uint8_t run(Dlog *d)
{
    dl.d = d;
    dl.alt = pkey_shift() & 8; /* Alt down already: its letters lit */
    dl.fresh = true;
    for (;;) {
        draw_all();
        draw(d);
        uint16_t key = uimn_getkey();
        DlogCtl *c = &d->ctl[d->focus];
        uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);
        int go = GO_LOOP;
        uint16_t sel = 0; /* GO_SET's */
        uint8_t letter = 0;
        bool beep = false, hot = false;
        if (key == KEY_MOUSE) {
            if (uimn.mouse.event != MS_PRESS)
                continue;
            uint8_t i = 0;
            while (i < d->n && !hit(d, &d->ctl[i]))
                i++;
            if (i >= d->n)
                continue;
            c = &d->ctl[i];
            if (c->flags & CF_NOF)
                continue;
            if (c->kind == CT_BTN) {
                go = GO_PRESS;
            } else {
                d->focus = i;
                dl.fresh = false;
                if (c->kind == CT_CHK) {
                    go = GO_CHECK;
                } else if (c->kind == CT_LST) { /* a list's line: once selects, on the
                                                    selected one opens */
                    int r = dl.hitrow - 1;
                    if (r < 0 || r >= c->rows)
                        continue;
                    DlogList *l = c->list;
                    uint16_t at = (uint16_t)(l->top + r);
                    if (at >= l->n)
                        continue;
                    if (at == l->sel) {
                        go = GO_ENTER;
                    } else {
                        go = GO_SET;
                        sel = at;
                    }
                }
            }
        } else {
            if (uimn.keyflags & 8) { /* Alt: the letters lit; Alt+letter */
                dl.alt = true;
                if (!ch) {
                    scan -= 0x10;
                    if ((uint8_t)(scan + 0x10) >= 0x10 && scan <= 0x22) {
                        letter = menu_altchar(scan + 0x10);
                        beep = true; /* unmatched beeps */
                        hot = true;
                    }
                    key = (uint16_t)(scan << 8 | ch);
                }
            }
            if (hot) {
            } else if (key == KEY_ALT) {
                dl.alt = true;
                continue;
            } else if (ch == 0x1B) {
                return DLOG_ESC;
            } else if (ch == 9) {
                step(d, 1);
                continue;
            } else if (key == 0x0F00) {
                step(d, -1);
                continue;
            } else if (ch == 0x0D) {
                go = GO_ENTER;
            } else if (c->kind == CT_FLD) {
                LineBuf *b = c->field;
                if (dl.fresh) { /* a character replaces selected text */
                    dl.fresh = false;
                    if (ch >= ' ')
                        b->len = b->col = 0;
                }
                dlog_ledit(b, key);
                continue;
            } else if (c->kind == CT_LST) {
                DlogList *l = c->list;
                uint16_t s = l->sel;
                switch (scan) {
                case 0x48:
                    if (!s)
                        continue;
                    s--;
                    break;
                case 0x50: s++; break;
                case 0x49: s = s >= c->rows ? s - c->rows : 0; break;
                case 0x47: s = 0; break;
                case 0x51: s += c->rows; break;
                case 0x4F: s = 0xFFFF; break;
                default: continue;
                }
                go = GO_SET;
                sel = s;
            } else if (ch != ' ') {
                hot = true;
                letter = ch;
            } else if (c->kind == CT_CHK) {
                go = GO_CHECK;
            } else if (c->kind == CT_BTN) {
                go = GO_PRESS;
            } else {
                continue;
            }
            if (hot) { /* a hotkey, from the next control on (COW dlgcore.c): a button
                          pressed, a box checked, else the focus */
                letter = uimn_upper(letter);
                uint8_t i = d->focus;
                uint8_t left = d->n;
                for (;;) {
                    if (++i >= d->n)
                        i = 0;
                    c = &d->ctl[i];
                    const char *t = c->kind == CT_BTN || c->kind == CT_CHK ? c->text
                                    : c->kind == CT_TXT                    ? nullptr
                                                                           : c->label;
                    if (hotkey(t) == letter)
                        break;
                    if (!--left)
                        break;
                }
                if (!left) {
                    if (beep)
                        pscr_beep();
                    continue;
                }
                if (c->kind == CT_BTN) {
                    go = GO_PRESS;
                } else {
                    d->focus = i;
                    dl.fresh = true;
                    if (c->kind != CT_CHK)
                        continue;
                    go = GO_CHECK;
                }
            }
        }

        if (go == GO_CHECK) {
            *c->check = !*c->check;
            continue;
        }
        if (go == GO_SET) {
            DlogList *l = c->list;
            if (!l->n)
                continue;
            if (sel >= l->n)
                sel = l->n - 1;
            l->sel = sel;
            if (c->hook)
                c->hook(DLOG_MOVED);
            continue;
        }
        if (go == GO_ENTER) {
            if (c->kind != CT_BTN) {
                if (c->kind == CT_LST && c->hook && c->hook(DLOG_ENTER))
                    continue;
                c = &d->ctl[d->def];
            }
            go = GO_PRESS;
        }
        if (go == GO_PRESS) {
            uint8_t i = (uint8_t)(c - d->ctl);
            if (c->hook && c->hook(DLOG_PRESS))
                continue;
            return i;
        }
    }
}

uint8_t dlog_run(Dlog *d)
{
    uimn.help = uimn_dlog_help;
    uint8_t r = run(d);
    uimn.help = nullptr;
    return r;
}

/* ---- message boxes */

uint8_t dlog_msg(const char *text, const MsgButton *buttons)
{
    char *d = msg_text; /* the lines, 0 each */
    uint8_t nl = 0, widest = 0, w = 0;
    for (;;) {
        char c = *text++;
        if (c == '|')
            c = 0;
        *d++ = c;
        if (c) {
            w++;
            continue;
        }
        nl++;
        if (w > widest)
            widest = w;
        w = 0;
        if (!text[-1])
            break;
    }
    dl.nlines = nl;
    uint16_t bw = (uint16_t)-3; /* the buttons' row: 3 apart */
    for (const MsgButton *b = buttons; b->text; b++)
        bw += width(b->text, b->cols) + 3;
    if (dl.title) { /* the title: its spaces too */
        uint16_t t = scrn_slen(dl.title) + 2;
        if (widest < t)
            widest = (uint8_t)t;
    }
    uint16_t cols = widest >= bw ? widest : bw; /* the wider, and 6 (msgbox.c) */
    cols += 6;
    Dlog *m = &msg_dlog;
    m->cols = (uint8_t)cols;
    m->col = (uint8_t)(80 - m->cols) >> 1;
    m->rows = nl + 6;
    uint8_t row = (uint8_t)(25 - m->rows) >> 1;
    if (dl.adjust) { /* over the error's line: at the foot */
        edit_ensure();
        uint8_t at = (uint8_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0);
        if (at >= row && at < (uint8_t)(row + m->rows))
            row = 24 - m->rows;
    }
    m->row = row;
    m->title = dl.title;
    m->sep = nl + 3;
    const char *s = msg_text; /* the lines as text controls */
    uint8_t i = 0;
    for (; i < nl; i++) {
        DlogCtl *c = &msg_ctl[i];
        *c = (DlogCtl){.kind = CT_TXT, .row = i + 2, .flags = CF_NOF, .text = s};
        c->col = dl.title ? 3 : (uint8_t)(m->cols - scrn_slen(s)) >> 1;
        s += strlen(s) + 1;
    }
    m->focus = m->def = dl.b0 = i;
    uint8_t col = (uint8_t)((uint8_t)(m->cols - 2 - (uint8_t)bw) >> 1) + 1; /* centred inside */
    for (const MsgButton *b = buttons; b->text; b++, i++) {
        msg_ctl[i] = (DlogCtl){.kind = CT_BTN, .row = nl + 4, .col = col, .cols = b->cols,
                               .text = b->text,
                               .hook = b->text == uimn_helpbtn ? dlog_nohelp : nullptr};
        col += width(b->text, b->cols) + 3;
    }
    m->n = i;
    uint8_t r = dlog_run(m);
    if (r != DLOG_ESC)
        r -= dl.b0;
    uimn.help = nullptr;
    return r;
}

bool dlog_nohelp(uint8_t why)
{
    pscr_beep();
    return true;
}

void dlog_err(char *text)
{
    dl.adjust = true;
    char *bar = strchr(text, '|');
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

uint8_t *dlog_wrap(uint8_t *dst, const uint8_t *src, uint32_t len, uint8_t indent)
{
    memset(dst, ' ', indent);
    dst += indent;
    uint32_t col = indent;
    uint8_t *space = nullptr; /* the last space */
    while (len--) {
        uint8_t c = *src++;
        if (c == '|')
            c = 0xB3;
        *dst++ = c;
        col++;
        if (c == ' ')
            space = dst - 1;
        if (col < DL_WRAP || !space)
            continue;
        *space = '|'; /* the rest of the line past the indent */
        uint32_t n = (uint32_t)(dst - space - 1);
        memmove(space + 1 + indent, space + 1, n);
        memset(space + 1, ' ', indent);
        dst += indent;
        col = (uint32_t)(dst - space - 1);
        space = nullptr;
    }
    return dst;
}

void dlog_ok(const char *text) { dlog_msg(text, bt_ok); }

uint8_t dlog_ync(const char *text)
{
    uint8_t r = dlog_msg(text, bt_ync);
    return r == DLOG_ESC ? 2 : r;
}

/* ---- Open, Save As */

/* The names matching pattern with attrs (PFIL_ADIR: directories only, "." left out) to list's
   items; their count. */
static uint16_t find(char *items, const char *pattern, uint8_t attrs)
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

/* dl_fsel: the Files list: the selected name to the field; Enter too, then OK. */
static bool files_pick(uint8_t why)
{
    if (!ls_files.n)
        return false;
    const char *s = files + ls_files.sel * NAMESZ;
    uint16_t n = (uint16_t)strlen(s);
    memcpy(uimn.fname.text, s, n);
    uimn.fname.len = uimn.fname.col = n;
    return why == DLOG_MOVED;
}

/* dl_dsel: the Dirs list: Enter goes into the selected one, the box stays. */
static bool dirs_pick(uint8_t why)
{
    if (why != DLOG_ENTER || !ls_dirs.n)
        return false;
    pfil_chdir(dirs + ls_dirs.sel * NAMESZ);
    list_files();
    return true;
}

bool dlog_fileok(uint8_t why)
{
    uint16_t len = uimn.fname.len;
    if (!len)
        return true;
    char *d = uimn.chosen;
    bool wild = false, dot = false;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t c = uimn_upper(uimn.fname.text[i]);
        if (c == '*' || c == '?')
            wild = true;
        if (c == '.')
            dot = true;
        if (c == '\\')
            dot = false;
        *d++ = (char)c;
    }
    if (!dot) {
        memcpy(d, ".BAS", 4);
        d += 4;
    }
    *d = 0;
    if (!wild)
        return false;
    memcpy(mask, uimn.chosen, 64); /* a mask: list it */
    list_files();
    return true;
}

/* dl_name1: the field set to s. */
static void set_field(const char *s)
{
    uint16_t n = (uint16_t)strlen(s);
    memcpy(uimn.fname.text, s, n);
    uimn.fname.len = uimn.fname.col = n;
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
    uint16_t at = (uint16_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0);
    dg_conf.row = at < 19 ? 19 : 2;
    dg_conf.focus = 0;
    return dlog_run(&dg_conf);
}

uint8_t dlog_change(void)
{
    dg_chg.focus = 0;
    return (uint8_t)(dlog_run(&dg_chg) - DC_FV);
}
