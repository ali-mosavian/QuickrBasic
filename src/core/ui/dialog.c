/* Dialog boxes: the engine, message boxes, Open, Save As, Find and Change.
 *
 * # How it works
 * A dialog (Dlog) has controls (DlogCtl); a control's place is from the dialog's corner. A field
 * edits a LineBuf (FLDMAX characters), a list a DlogList, a check box flips a bool, a button ends
 * the dialog with its index unless its hook keeps it.
 *
 * The keys: Tab, Shift+Tab the focus; Esc; Enter the focused button or the default; Alt+letter, or
 * a letter outside a field, a control's hotkey; the mouse. The editor is drawn under the box.
 * uimn.help is the dialog's status line meanwhile.
 *
 * A message box splits its text's lines at '|' and wraps them to 44 columns. The Open and Save As
 * lists, the message box and each dialog's state are static. The dialog tables are not const: focus
 * and row change.
 *
 * # asm structure (retired/asm32/ui/dlg.asm)
 * | asm | here | |
 * |---|---|---|
 * | dl_run | dlog_run | MIRROR |
 * | dl_msg, dl_ok, dl_ync | dlog_msg, dlog_ok, dlog_ync | MIRROR |
 * | dl_err | dlog_err | MIRROR; writes a 0 at its first '|' |
 * | dl_wrap, wr_ind | dlog_wrap, its indent | MIRROR; D1 |
 * | dl_open, dl_saveas | dlog_open, dlog_saveas | MIRROR; D2 |
 * | dl_find, dg_find | findbar.c's bar | MOVED; a bar, not a box |
 * | dl_conf, dl_change | dlog_conf, dlog_change | MIRROR |
 * | dl_fok | dlog_fileok | MIRROR; D2 |
 * | dl_bhelp | dlog_nohelp | MIRROR; D2 |
 * | ui_ledit | dlog_ledit | MIRROR |
 * | dl_ctl, dl_pos, dl_draw, dl_one, dl_dfld, dl_dlst, dl_dbtn, dl_hk, dl_acc, dl_dchk, dl_hit, dl_step, dl_run0, dl_bw, dl_files, dl_fsel, dl_dsel, dl_name1 | statics | MIRROR |
 * | dg_open, dg_save, dg_conf, dg_chg | static Dlogs | MIRROR |
 * | TDLG, TCTL, TLIST | Dlog, DlogCtl, DlogList | D3 |
 * | TBTN, bt_ok, bt_ync | MsgButton; static lists | MIRROR |
 * | TMSGD, g_ui.dg.*, bf.msg_txt, msg_d, ls_fl, ls_dl, dl_mask, dl_path, dl_dta | statics | D4 |
 * | g_ui.bf.dl_fn, dl_name | uimn.fname, uimn.chosen | MOVED (ui.h) |
 * | g_ui.bf.fd_what, fd_to, g_ui.fd.fd_case, fd_whole | uimn.find_what, change_to, match_case, whole_word | MOVED (ui.h) |
 * | CT_*, CF_NOF, DO_OK, DS_OK, DC_FV | same (dialog.h); DLOG_ESC 0xFF | MIRROR |
 * | u_open, u_saveas, u_find, u_change, u_fname, u_files, u_dirs, u_skip, u_chg1, u_fwhat, u_cto, u_case, u_whole, u_fverify, u_chall, u_mbas, u_bas, u_star, u_yes, u_no | statics | MIRROR |
 * | DL_WRAP | static (44) | MIRROR |
 * | LSMAX | none: the lists grow | D5 |
 *
 * - D1. A parameter, not a global set before the call.
 * - D2. A flag result is a bool, true the success. A hook's true keeps the box: the asm's CF set
 *   for a button, clear for a list's Enter.
 * - D3. A control's cb is hook, or a check box's check; TDLG's controls are the ctl array.
 * - D4. ls_f and ls_d are initialised statically with their item buffers: the asm's start-up setup.
 * - D5. No limit on a list's names: as many as memory holds.
 */
#include "ui/dialog.h"
#include "util/grow.h"
#include "session.h"
#include "plat/pfil.h"
#include "plat/pkey.h"
#include "plat/pscr.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/help.h"
#include "ui/menu.h"
#include "ui/mouse.h"
#include "ui/screen.h"
#include "ui/uimain.h"

enum {
    NAMESZ = 13, /* a list's item: NAME.EXT and 0 */
    DL_WRAP = 44,
    NMSGCTL = 16,
    FRAME = 2,      /* a box's two sides */
    LIST_GAP = 2,   /* between a list's columns */
    FIELD_ROWS = 3, /* a field's box */
    BTN_PAD = 4,    /* a button's width past its text when none given: "< " and " >" */
};

/* A message box's layout (msgbox.c): its lines from MSG_TOP, the separator below them, the
   buttons MSG_GAP apart under it. */
enum { MSG_TOP = 2, MSG_TCOL = 3, MSG_GAP = 3, MSG_PAD = 6 };

static const char s_open[] = "Open", s_saveas[] = "Save As", s_change[] = "Change",
                  s_fname[] = "File &Name:", s_files[] = "&Files", s_dirs[] = "&Dirs/Drives",
                  s_yes[] = "&Yes", s_no[] = "&No", s_skip[] = "&Skip", s_chg1[] = "&Change",
                  s_fwhat[] = "&Find What:", s_cto[] = "Change &To:",
                  s_case[] = "&Match Upper/Lowercase", s_whole[] = "&Whole Word",
                  s_fverify[] = "Find and &Verify", s_chall[] = "&Change All";

static const MsgButton bt_ok[] = {{uimn_ok, 8}, {uimn_helpbtn, 8}, {}};
static const MsgButton bt_ync[] = {{s_yes, 7}, {s_no, 8}, {uimn_cancel, 8}, {uimn_helpbtn, 8}, {}};

static struct {
    Widget w;             /* modal, shown while it runs; its focus the focused control */
    Widget kids[NMSGCTL]; /* the controls, ctl's order */
    uint8_t result;       /* the button that ended it, DLOG_ESC */
    Dlog *d;              /* dl_bx: the dialog running */
    uint16_t cur;         /* dl_cur: the cursor's place, row << 8 | col; PSCR_HIDDEN none */
    bool focused;         /* dl_hf: the control drawing has the focus */
    bool fresh;           /* dl_new: the focus just came: a field's text selected till a key */
    bool alt;             /* dl_alt: Alt was down: the hotkeys lit */
    uint8_t nlines;       /* the message box's: its lines */
    uint8_t b0;           /*   its first button's control */
    bool adjust;          /*   msg_adj: off the cursor's line */
    const char *title;    /* msg_ttl: its title, nullptr none: its lines then left-aligned */
} dl;

static DlogList ls_files = {.size = NAMESZ, .columns = true}; /* items grown as names come */
static DlogList ls_dirs = {.size = NAMESZ};
static uint32_t files_cap, dirs_cap; /* their items' room */
static char mask[80];                /* dl_mask: the Open box's */
static char *path;                   /* dl_path: the current directory, grown to fit */
static uint32_t path_cap;
static PfilFind dta; /* dl_dta */
static char msg_text[640];
static DlogCtl msg_ctl[NMSGCTL];
static Dlog msg_dlog = {.ctl = msg_ctl};

static bool files_pick(uint8_t why);
static bool dirs_pick(uint8_t why);

/* The boxes as QBasic's (uifile.des, uisearch.des). */
static DlogCtl open_ctl[] = {
    {.kind = CT_FLD,
     .row = 1,
     .col = 13,
     .cols = 50,
     .rows = 1,
     .field = &uimn.fname,
     .label = s_fname,
     .lrow = 2,
     .lcol = 2},
    {.kind = CT_TXT,
     .row = 4,
     .col = 2,
     .cols = 45,
     .flags = CF_NOF | CF_PATH,
     .text = ""}, /* the Files list's width */
    {.kind = CT_LST,
     .row = 6,
     .col = 2,
     .cols = 43,
     .rows = 8,
     .list = &ls_files,
     .hook = files_pick,
     .label = s_files,
     .lrow = 5,
     .lcol = 22},
    {.kind = CT_LST,
     .row = 6,
     .col = 49,
     .cols = 14,
     .rows = 8,
     .list = &ls_dirs,
     .hook = dirs_pick,
     .label = s_dirs,
     .lrow = 5,
     .lcol = 51},
    {.kind = CT_BTN, .row = 18, .col = 12, .cols = 6, .text = uimn_ok, .hook = dlog_fileok},
    {.kind = CT_BTN, .row = 18, .col = 28, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 18, .col = 48, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_open = {2, 6, 20, 67, s_open, 7, 0, 17, DO_OK, open_ctl};

static DlogCtl save_ctl[] = {
    {.kind = CT_FLD,
     .row = 1,
     .col = 13,
     .cols = 25,
     .rows = 1,
     .field = &uimn.fname,
     .label = s_fname,
     .lrow = 2,
     .lcol = 2},
    {.kind = CT_TXT, .row = 4, .col = 2, .flags = CF_NOF | CF_PATH, .text = ""},
    {.kind = CT_LST,
     .row = 6,
     .col = 13,
     .cols = 14,
     .rows = 7,
     .list = &ls_dirs,
     .hook = dirs_pick,
     .label = s_dirs,
     .lrow = 5,
     .lcol = 15},
    {.kind = CT_BTN, .row = 16, .col = 7, .cols = 6, .text = uimn_ok, .hook = dlog_fileok},
    {.kind = CT_BTN, .row = 16, .col = 17, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 16, .col = 31, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_save = {3, 19, 18, 42, s_saveas, 6, 0, 15, DS_OK, save_ctl};

static DlogCtl conf_ctl[] = {
    /* Find and Verify's question (uisearch.des dlgConfirm) */
    {.kind = CT_BTN, .row = 3, .col = 4, .cols = 10, .text = s_chg1},
    {.kind = CT_BTN, .row = 3, .col = 16, .cols = 8, .text = s_skip},
    {.kind = CT_BTN, .row = 3, .col = 28, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 3, .col = 41, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_conf = {19, 13, 5, 53, s_change, 4, 0, 2, 0, conf_ctl};
enum { CONF_LOW = 19, CONF_HIGH = 2 }; /* its rows: low unless the cursor's line is there */

static DlogCtl chg_ctl[] = {
    {.kind = CT_FLD,
     .row = 1,
     .col = 13,
     .cols = 41,
     .rows = 1,
     .field = &uimn.find_what,
     .label = s_fwhat,
     .lrow = 2,
     .lcol = 2},
    {.kind = CT_FLD,
     .row = 4,
     .col = 13,
     .cols = 41,
     .rows = 1,
     .field = &uimn.change_to,
     .label = s_cto,
     .lrow = 5,
     .lcol = 2},
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
    const uint16_t first =
        b->col >= c->cols ? b->col - c->cols + 1 : 0; /* the first column shown */
    const uint16_t rest = b->len >= first ? b->len - first : 0;
    const uint16_t n = rest > c->cols ? c->cols : rest;
    scrn_strn(row, col, b->text + first, n, SL_DLG);

    if (!dl.focused)
        return;
    if (dl.fresh)
        scrn_attr(row, col, n, SL_DSEL);

    dl.cur = place(row, (uint8_t)(col + (b->col - first)));
}

/* c's list's columns shown: one unless the list has columns; then as many of the widest item and
   LIST_GAP as fit. */
static uint8_t list_per(const DlogCtl *const c)
{
    const DlogList *const l = c->list;

    if (!l->columns)
        return 1;

    const uint32_t per = (c->cols + LIST_GAP) / (l->size - 1 + LIST_GAP); /* size: its 0 too */
    return per ? (uint8_t)per : 1;
}

/* What a list's top moves by: a column when it has columns. */
static uint32_t list_step(const DlogCtl *const c) { return c->list->columns ? c->rows : 1; }

/* c's list's items past its box: then its bar, sideways for columns, else down. */
static bool list_over(const DlogCtl *const c)
{
    return c->list->n > (uint32_t)c->rows * list_per(c) && c->cols >= 3 && c->rows >= 3;
}

/* Where the thumb is on c's list's bar, len cells, arrows too: the selection's column, item. */
static uint32_t bar_thumb(const DlogCtl *const c, const uint32_t len)
{
    const DlogList *const l = c->list;

    if (l->columns)
        return l->sel / c->rows * (len - 3) / ((l->n + c->rows - 1) / c->rows);

    return l->sel * (len - 3) / l->n;
}

/* c's list's bar, on its box's bottom or right side: arrows, the track, the thumb. */
static void draw_bar(const DlogCtl *const c, const uint8_t top, const uint8_t left)
{
    if (!list_over(c))
        return;

    if (c->list->columns) {
        const uint8_t row = (uint8_t)(top + c->rows + 1);
        uint8_t x = scrn_fill(row, (uint8_t)(left + 1), SL_BAR, GLYPH_LEFT, 1);
        x = scrn_fill(row, x, SL_BAR, GLYPH_SHADE, c->cols - 2);
        scrn_fill(row, x, SL_BAR, GLYPH_RIGHT, 1);
        scrn_fill(row, (uint8_t)(left + 2 + bar_thumb(c, c->cols)), SL_THB, ' ', 1);
        return;
    }

    const uint8_t col = (uint8_t)(left + c->cols + 1);
    scrn_fill((uint8_t)(top + 1), col, SL_BAR, GLYPH_UP, 1);

    for (uint8_t r = 2; r < c->rows; r++)
        scrn_fill((uint8_t)(top + r), col, SL_BAR, GLYPH_SHADE, 1);

    scrn_fill((uint8_t)(top + c->rows), col, SL_BAR, GLYPH_DOWN, 1);
    scrn_fill((uint8_t)(top + 2 + bar_thumb(c, c->rows)), col, SL_THB, ' ', 1);
}

static void draw_list(const DlogCtl *const c, const uint8_t top, const uint8_t left)
{
    scrn_box(top, left, c->rows + FRAME, c->cols + FRAME, SL_DLG, nullptr);
    DlogList *const l = c->list;
    const uint8_t per = list_per(c);
    const uint8_t stride = c->cols / per;
    const uint8_t wide = per > 1 ? stride - 1 : c->cols; /* an item's cells, its highlight */
    const uint32_t page = (uint32_t)c->rows * per, step = list_step(c);

    if (l->sel < l->top) /* the first shown keeps it in view */
        l->top = l->sel - l->sel % step;
    if (l->sel >= l->top + page)
        l->top = (l->sel / step + 1) * step - page;

    for (uint32_t k = 0; k < page; k++) {
        const uint32_t i = l->top + k;

        if (i >= l->n)
            break;

        const uint8_t row = (uint8_t)(top + 1 + k % c->rows);
        const uint8_t col = (uint8_t)(left + 1 + k / c->rows * stride);
        const char *const item = l->items + i * l->size; /* cut at its cells */
        const size_t len = strlen(item);
        scrn_strn(row, col, (const uint8_t *)item, len < wide ? (uint16_t)len : wide, SL_DLG);

        if (i == l->sel && dl.focused) {
            scrn_attr(row, col, wide, SL_DSEL);
            dl.cur = place(row, col);
        }
    }

    draw_bar(c, top, left);
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
    const Dlog *const d = dl.d; /* the default one lit while no button has the focus (button.c) */
    const bool def = c == &d->ctl[d->def] && d->ctl[dl.w.focus].kind != CT_BTN;
    const uint8_t edge = dl.focused || def ? SL_DFOC : SL_DLG;
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
/* A path to n cells at most: its drive and as many of its last folders as fit, ".." between
   (C:\..\shot\files, COW's SzChopText); its last folder too long, that folder's end. */
static void draw_path(const uint8_t row, const uint8_t col, const char *const s, const uint16_t n)
{
    static const uint8_t dots[] = "..";
    enum : uint16_t { HEAD = 3, DOTS = 2 }; /* "C:\" */

    const size_t len = strlen(s);

    if (len <= n) {
        scrn_strn(row, col, (const uint8_t *)s, (uint16_t)len, SL_DLG);
        return;
    }

    size_t tail = 0; /* the last folders that fit, from their '\' */

    for (size_t i = len; i-- > HEAD;) {
        if (s[i] == '\\' && HEAD + DOTS + (len - i) <= n)
            tail = len - i;
    }

    if (!tail) { /* the last folder's end */
        const uint16_t keep = n > DOTS ? n - DOTS : 0;
        const uint8_t c = scrn_strn(row, col, dots, n < DOTS ? n : DOTS, SL_DLG);
        scrn_strn(row, c, (const uint8_t *)s + len - keep, keep, SL_DLG);
        return;
    }

    uint8_t c = scrn_strn(row, col, (const uint8_t *)s, HEAD, SL_DLG);
    c = scrn_strn(row, c, dots, DOTS, SL_DLG);
    scrn_strn(row, c, (const uint8_t *)s + len - tail, (uint16_t)tail, SL_DLG);
}

/* A text control: cut at its cols, none at the box's right side a space short; a path in its
   middle. */
static void draw_text(
    const Dlog *const d,
    const DlogCtl *const c,
    const uint8_t row,
    const uint8_t col
)
{
    const uint16_t side = d->cols > c->col + 2 ? d->cols - c->col - 2 : 0;
    const uint16_t room = c->cols && c->cols < side ? c->cols : side;

    if (c->flags & CF_PATH)
        draw_path(row, col, c->text, room);
    else if (scrn_slen(c->text) <= room)
        scrn_str(row, col, c->text, SL_DLG, SL_DLG);
    else
        scrn_strn(row, col, (const uint8_t *)c->text, room, SL_DLG);
}

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

    case CT_TXT:
        draw_text(d, c, row, col);
        break;

    default:
        scrn_str(row, col, c->text, SL_DLG, SL_DLG);
    }
}

/* The cursor where draw left it: a field's. */
static void dl_cursor(Widget *const w)
{
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

/* dl_hit's: control c's cells, the box at d. */
static Rect ctl_rect(const Dlog *const d, const DlogCtl *const c)
{
    Rect r = {.row = d->row + c->row, .col = d->col + c->col};

    switch (c->kind) {
    case CT_FLD:
        r.rows = FIELD_ROWS;
        r.cols = c->cols + FRAME;
        break;

    case CT_LST:
        r.rows = c->rows + FRAME;
        r.cols = c->cols + FRAME;
        break;

    case CT_TXT:
        break;

    default:
        r.rows = 1;
        r.cols = (uint8_t)(scrn_slen(c->text) + BTN_PAD);
    }

    return r;
}

/* dl_step: the focus to the next control that takes it, dir 1, or the one before, -1. */
static void step(const Dlog *const d, const int dir)
{
    int i = dl.w.focus;

    do {
        i += dir;

        if (i < 0)
            i = d->n - 1;
        if (i >= d->n)
            i = 0;
    } while (d->ctl[i].flags & CF_NOF);

    dl.w.focus = (uint8_t)i;
    dl.fresh = true;
}

bool dlog_ledit(LineBuf *const b, const uint16_t key)
{
    const uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);

    if (ch == CH_BACKSPACE) {
        edit_lb_bs(b);
        return true;
    }

    if (ch && ch != CH_E0) {
        if (ch < ' ')
            return false;

        edit_lb_type(b, ch, FLDMAX);
        return true;
    }

    switch (scan) {
    case SCAN_LEFT:
        edit_lb_left(b);
        return true;

    case SCAN_RIGHT:
        edit_lb_right(b);
        return true;

    case SCAN_HOME:
        edit_lb_home(b);
        return true;

    case SCAN_END:
        edit_lb_end(b);
        return true;

    case SCAN_DEL:
        edit_lb_del(b);
        return true;

    case SCAN_CTRL_LEFT:
        edit_lb_wleft(b);
        return true;

    case SCAN_CTRL_RIGHT:
        edit_lb_wright(b);
        return true;

    default:
        return false;
    }
}

/* What a key or a click leads to: GO_LOOP nothing more; GO_ESC the box ends. */
enum : uint8_t { GO_LOOP, GO_PRESS, GO_ENTER, GO_CHECK, GO_SET, GO_ESC };

/* A click at p on c's list's bar, len cells: an arrow a column (an item) on or back, the track a
   page towards it. */
static uint8_t bar_click(
    const DlogCtl *const c,
    const uint32_t p,
    const uint32_t len,
    uint32_t *const sel
)
{
    const uint32_t s = c->list->sel, page = (uint32_t)c->rows * list_per(c);
    const bool arrow = !p || p == len - 1;
    const bool back = !p || (!arrow && p < 1 + bar_thumb(c, len));
    const uint32_t by = arrow ? list_step(c) : page;

    *sel = back ? (s >= by ? s - by : 0) : s + by;
    return GO_SET;
}

/* A click on a list's line r, cell x: once selects (*sel), on the selected one opens; on its
   bar, moves. */
static uint8_t list_click(const DlogCtl *const c, const int r, const int x, uint32_t *const sel)
{
    const uint8_t per = list_per(c);
    const int column = x / (c->cols / per);

    if (list_over(c) && c->list->columns && r == c->rows && x >= 0 && x < c->cols)
        return bar_click(c, (uint32_t)x, c->cols, sel);
    if (list_over(c) && !c->list->columns && x == c->cols && r >= 0 && r < c->rows)
        return bar_click(c, (uint32_t)r, c->rows, sel);

    if (r < 0 || r >= c->rows || x < 0 || column >= per)
        return GO_LOOP;

    const DlogList *const l = c->list;
    const uint32_t at = l->top + (uint32_t)column * c->rows + (uint32_t)r;

    if (at >= l->n)
        return GO_LOOP;
    if (at == l->sel)
        return GO_ENTER;

    *sel = at;
    return GO_SET;
}

/* A press on control i (c) at its row r, column x: a button pressed, else the focus. */
static uint8_t click(
    const DlogCtl *const c,
    const uint8_t i,
    const int r,
    const int x,
    uint32_t *const sel
)
{
    if (c->flags & CF_NOF)
        return GO_LOOP;
    if (c->kind == CT_BTN)
        return GO_PRESS;

    dl.w.focus = i;
    dl.fresh = false;

    if (c->kind == CT_CHK)
        return GO_CHECK;
    if (c->kind == CT_LST)
        return list_click(c, r - 1, x - 1, sel);

    return GO_LOOP;
}

/* A hotkey, from the next control on (COW dlgcore.c): a button pressed (*pc), a box checked,
   else the focus; none: beep if asked. */
static uint8_t on_hotkey(Dlog *const d, const uint8_t key, const bool beep, DlogCtl **const pc)
{
    const uint8_t letter = upcase(key);
    uint8_t i = dl.w.focus;

    for (uint8_t k = 0; k < d->n; k++) {
        if (++i >= d->n)
            i = 0;

        DlogCtl *const c = &d->ctl[i];

        if (hotkey(hot_text(c)) != letter)
            continue;

        *pc = c;

        if (c->kind == CT_BTN)
            return GO_PRESS;

        dl.w.focus = i;
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
static uint8_t list_key(const DlogCtl *const c, const uint8_t scan, uint32_t *const sel)
{
    const uint32_t s = c->list->sel;
    const uint32_t page = (uint32_t)c->rows * list_per(c), across = c->list->columns ? c->rows : 0;

    switch (scan) {
    case SCAN_UP:
        if (!s)
            return GO_LOOP;

        *sel = s - 1;
        break;

    case SCAN_DOWN:
        *sel = s + 1;
        break;

    case SCAN_LEFT:
        if (!across || !s)
            return GO_LOOP;

        *sel = s >= across ? s - across : 0;
        break;

    case SCAN_RIGHT:
        if (!across)
            return GO_LOOP;

        *sel = s + across;
        break;

    case SCAN_PGUP:
        *sel = s >= page ? s - page : 0;
        break;

    case SCAN_HOME:
        *sel = 0;
        break;

    case SCAN_PGDN:
        *sel = s + page;
        break;

    case SCAN_END:
        *sel = UINT32_MAX;
        break;

    default:
        return GO_LOOP;
    }

    return GO_SET;
}

/* key an Alt+letter? */
static bool alt_letter(const uint16_t key)
{
    const uint8_t scan = (uint8_t)(key >> 8);

    return uimn.keyflags & PKEY_ALT && !(uint8_t)key && scan >= SCAN_ALT_FIRST &&
           scan <= SCAN_ALT_LAST;
}

/* The box's keys, which a control passes up: Esc, Tab, Shift+Tab, Enter, Alt alone, an
   Alt+letter. Alt held lights the letters; with another key, *key is that key alone. */
static bool dialog_key(uint16_t *const key)
{
    if (uimn.keyflags & PKEY_ALT) {
        dl.alt = true;

        if (alt_letter(*key))
            return true;
        if (!(uint8_t)*key)
            *key = (uint16_t)((uint8_t)((*key >> 8) - SCAN_ALT_FIRST) << 8);
    }

    const uint8_t ch = (uint8_t)*key;

    return *key == KEY_ALT || *key == KEY_SHIFT_TAB || ch == CH_ESC || ch == CH_TAB ||
           ch == CH_ENTER || *key == SCAN_F1 << 8; /* F1: Help */
}

/* A list's selection to sel, at most its last. */
static void list_set(const DlogCtl *const c, const uint32_t sel)
{
    DlogList *const l = c->list;

    if (!l->n)
        return;

    l->sel = sel >= l->n ? l->n - 1 : sel;

    if (c->hook)
        c->hook(DLOG_MOVED);
}

/* go done on c; true: the box ends with *r, the button pressed. */
static bool act(
    const Dlog *const d,
    const uint8_t go,
    DlogCtl *c,
    const uint32_t sel,
    uint8_t *const r
)
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

/* go done on the focused control c: the box hidden when it ends. */
static void ended(const uint8_t go, DlogCtl *const c, const uint32_t sel)
{
    if (go == GO_ESC) {
        dl.result = DLOG_ESC;
        dl.w.visible = false;
        return;
    }

    if (act(dl.d, go, c, sel, &dl.result))
        dl.w.visible = false;
}

static DlogCtl *ctl_of(const Widget *const w) { return &dl.d->ctl[w - dl.kids]; }

static Session *dl_session; /* the box running's, for a Help button's hook */

/* F1 or Help: the message box's first line is the topic's key (an error's message), else
   "Message boxes"; another box's, its title and " Dialog". */
static void dlog_help(Session *const S)
{
    char key[64];
    uint32_t n;

    if (dl.d == &msg_dlog) {
        n = (uint32_t)strlen(msg_text);
        memcpy(key, msg_text, n < sizeof key ? n : sizeof key - 1);
        n = n < sizeof key ? n : sizeof key - 1;

        if (!help_has(key, n)) {
            n = sizeof "message boxes" - 1;
            memcpy(key, "message boxes", n);
        }
    } else {
        const char *const t = dl.d->title ? dl.d->title : "";
        n = (uint32_t)strlen(t);
        n = n < sizeof key - 8 ? n : sizeof key - 8;
        memcpy(key, t, n);
        memcpy(key + n, " Dialog", 7);
        n += 7;
    }

    help_box(S, key, n);
}

/* The box's keys, and the hotkeys a button or a check box passes up. */
static bool dl_key(Widget *const w, Session *const S, const uint16_t key)
{
    DlogCtl *c = &dl.d->ctl[dl.w.focus];
    const uint8_t ch = (uint8_t)key;
    uint16_t k = key;
    uint8_t go = GO_LOOP;

    if (alt_letter(key))
        go = on_hotkey(dl.d, menu_altchar((uint8_t)(key >> 8)), true, &c); /* unmatched beeps */
    else if (!dialog_key(&k))
        go = on_hotkey(dl.d, ch, false, &c);
    else if (k == KEY_ALT)
        dl.alt = true;
    else if (ch == CH_ESC)
        go = GO_ESC;
    else if (!ch && (uint8_t)(key >> 8) == SCAN_F1)
        dlog_help(S);
    else if (ch == CH_TAB)
        step(dl.d, 1);
    else if (k == KEY_SHIFT_TAB)
        step(dl.d, -1);
    else if (ch == CH_ENTER)
        go = GO_ENTER;

    ended(go, c, 0);
    return true;
}

/* The box takes the clicks off its controls. */
static bool dl_mouse(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    return true;
}

/* dl_draw: the frame, the shadow, the separator; the controls after. */
static void dl_draw(Widget *const w, Session *const S)
{
    const Dlog *const d = dl.d;
    scrn_box(d->row, d->col, d->rows, d->cols, SL_DLG, d->title);
    scrn_shadow(d->row, d->col, d->rows, d->cols);

    if (d->sep)
        scrn_sep(d->row + d->sep, d->col, d->cols, SL_DLG);

    dl.cur = PSCR_HIDDEN;
}

static void ctl_draw(Widget *const w, Session *const S)
{
    dl.focused = w - dl.kids == dl.w.focus;
    draw_ctl(dl.d, ctl_of(w));
}

static bool ctl_mouse(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    if (uimn.mouse.event != MS_PRESS)
        return false;

    DlogCtl *const c = ctl_of(w);
    uint32_t sel = 0; /* GO_SET's */
    const uint8_t go = click(c, (uint8_t)(w - dl.kids), row - w->rect.row, col - w->rect.col, &sel);
    ended(go, c, sel);
    return true;
}

/* A field takes every key not the box's. */
static bool fld_key(Widget *const w, Session *const S, uint16_t key)
{
    if (dialog_key(&key))
        return false;

    field_key(ctl_of(w)->field, key);
    return true;
}

/* A list too: its moves. */
static bool lst_key(Widget *const w, Session *const S, uint16_t key)
{
    if (dialog_key(&key))
        return false;

    DlogCtl *const c = ctl_of(w);
    uint32_t sel = 0;
    const uint8_t go = list_key(c, (uint8_t)(key >> 8), &sel);
    ended(go, c, sel);
    return true;
}

/* A button, a check box: Space presses, checks; the rest go up, hotkeys. */
static bool press_key(Widget *const w, Session *const S, uint16_t key)
{
    if (dialog_key(&key) || (uint8_t)key != ' ')
        return false;

    DlogCtl *const c = ctl_of(w);
    ended(c->kind == CT_CHK ? GO_CHECK : GO_PRESS, c, 0);
    return true;
}

static const WidgetOps dl_ops = {.draw = dl_draw, .key = dl_key, .mouse = dl_mouse};
static const WidgetOps fld_ops = {
    .draw = ctl_draw,
    .key = fld_key,
    .mouse = ctl_mouse,
    .cursor = dl_cursor,
};
static const WidgetOps lst_ops = {
    .draw = ctl_draw,
    .key = lst_key,
    .mouse = ctl_mouse,
    .cursor = dl_cursor,
};
static const WidgetOps press_ops = {
    .draw = ctl_draw,
    .key = press_key,
    .mouse = ctl_mouse,
    .cursor = dl_cursor,
};
static const WidgetOps txt_ops = {.draw = ctl_draw};

/* d's controls as the box's children. */
static void bind(const Dlog *const d)
{
    static_assert(NMSGCTL <= TREE_MAX_CHILDREN);
    dl.w.rect = (Rect){d->row, d->col, d->rows, d->cols};

    dl.w.nchildren = 0;
    dl.w.focus = d->focus;

    for (uint8_t i = 0; i < d->n; i++) {
        const DlogCtl *const c = &d->ctl[i];
        const WidgetOps *const ops = c->kind == CT_FLD   ? &fld_ops
                                     : c->kind == CT_LST ? &lst_ops
                                     : c->kind == CT_TXT ? &txt_ops
                                                         : &press_ops;
        dl.kids[i] = (Widget){.ops = ops, .rect = ctl_rect(d, c), .visible = true};

        tree_add(&dl.w, &dl.kids[i]);
    }
}

void dlog_build(Widget *const screen)
{
    dl.w = (Widget){.ops = &dl_ops, .modal = true};

    tree_add(screen, &dl.w);
}

/* dl_run0: a box run inside another's hook gives the outer one back after. */
static uint8_t run(Session *const S, Dlog *const d)
{
    Dlog *const outer = dl.d;
    const bool shown = dl.w.visible;

    if (outer)
        outer->focus = dl.w.focus;

    dl.d = d;
    bind(d);
    dl.alt = pkey_shift() & PKEY_ALT; /* Alt down already: its letters lit */
    dl.fresh = true;
    uimn_modal(S, &dl.w);
    d->focus = dl.w.focus;
    dl.d = outer;

    if (outer)
        bind(outer);

    dl.w.visible = shown;
    return dl.result;
}

uint8_t dlog_run(Session *const S, Dlog *const d)
{
    dl_session = S;
    uimn.help = uimn_dlog_help;
    const uint8_t r = run(S, d);
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
    const uint8_t row = (uint8_t)(uimn.rows - rows) >> 1;

    if (!dl.adjust)
        return row;

    edit_ensure();
    const uint8_t at = (uint8_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0);

    if (at >= row && at < (uint8_t)(row + rows))
        return (uint8_t)(uimn.rows - 1 - rows);

    return row;
}

/* The lines as text controls, from msg_ctl[0]. */
static void msg_lines(const Dlog *const m, const uint8_t nl)
{
    const char *s = msg_text;

    for (uint8_t i = 0; i < nl; i++) {
        DlogCtl *const c = &msg_ctl[i];
        *c = (DlogCtl){
            .kind = CT_TXT,
            .row = MSG_TOP + i,
            .flags = CF_NOF,
            .text = s,
        };

        c->col = dl.title ? MSG_TCOL : (uint8_t)(m->cols - scrn_slen(s)) >> 1;
        s += strlen(s) + 1;
    }
}

/* The buttons as controls, from msg_ctl[i], on row, centred inside; past the last. */
static uint8_t msg_buttons(
    const Dlog *const m,
    const MsgButton *const buttons,
    uint8_t i,
    const uint8_t row,
    const uint16_t bw
)
{
    uint8_t col = (uint8_t)((uint8_t)(m->cols - 2 - (uint8_t)bw) >> 1) + 1;

    for (size_t k = 0; buttons[k].text; k++, i++) {
        const MsgButton *const b = &buttons[k];
        msg_ctl[i] = (DlogCtl){
            .kind = CT_BTN,
            .row = row,
            .col = col,
            .cols = b->cols,
            .text = b->text,
            .hook = b->text == uimn_helpbtn ? dlog_nohelp : nullptr,
        };

        col += width(b->text, b->cols) + MSG_GAP;
    }

    return i;
}

uint8_t dlog_msg(Session *const S, const char *const text, const MsgButton *const buttons)
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
    uint8_t r = dlog_run(S, m);

    if (r != DLOG_ESC)
        r -= dl.b0;

    uimn.help = nullptr;
    return r;
}

bool dlog_nohelp(const uint8_t why)
{
    if (why == DLOG_PRESS && dl_session)
        dlog_help(dl_session);

    return true;
}

void dlog_err(Session *const S, char *text)
{
    dl.adjust = true;
    char *const bar = strchr(text, '|');

    if (bar) {
        *bar = 0;
        dlog_text(S, text, bar + 1);
    } else {
        dlog_ok(S, text);
    }

    dl.adjust = false;
    uimn.ed.sel_on = false; /* the error's word, shown till now */
}

uint8_t *dlog_wrap(uint8_t *dst, const uint8_t *const src, const uint32_t len, const uint8_t indent)
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

void dlog_ok(Session *const S, const char *const text) { dlog_msg(S, text, bt_ok); }

void dlog_text(Session *const S, const char *const title, const char *const text)
{
    dl.title = title;
    dlog_ok(S, text);
    dl.title = nullptr;
}

uint8_t dlog_ync(Session *const S, const char *const text)
{
    const uint8_t r = dlog_msg(S, text, bt_ync);
    return r == DLOG_ESC ? 2 : r;
}

/* ---- Open, Save As */

int dlog_namecmp(const char *const a, const char *const b)
{
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;

    for (; *x && upcase(*x) == upcase(*y); x++, y++) {
    }

    return upcase(*x) - upcase(*y);
}

/* l's n items, NAMESZ each, by name. */
static void sort_names(DlogList *const l, const uint32_t n)
{
    char item[NAMESZ];

    for (uint32_t i = 1; i < n; i++) {
        memcpy(item, l->items + i * NAMESZ, NAMESZ);
        uint32_t at = i;

        for (; at && dlog_namecmp(l->items + (at - 1) * NAMESZ, item) > 0; at--)
            memcpy(l->items + at * NAMESZ, l->items + (at - 1) * NAMESZ, NAMESZ);

        memcpy(l->items + at * NAMESZ, item, NAMESZ);
    }
}

/* The names matching pattern with attrs (PFIL_ADIR: directories only, "." left out) to l's items,
   cap of room, grown, by name; their count, as many as memory holds. */
static uint32_t find(
    DlogList *const l,
    uint32_t *const cap,
    const char *const pattern,
    const uint8_t attrs
)
{
    uint32_t n = 0;

    for (uint16_t e = pfil_first(pattern, attrs, &dta); e == PFIL_OK; e = pfil_next(&dta)) {
        if (attrs == PFIL_ADIR && (!(dta.attr & PFIL_ADIR) || !strcmp(dta.name, ".")))
            continue;
        if (!grow_room(&l->items, cap, n + 1u, NAMESZ))
            break;

        memcpy(l->items + n * NAMESZ, dta.name, NAMESZ);
        n++;
    }

    sort_names(l, n);
    return n;
}

/* dl_path: the current directory, drive, colon and \ first, as long as it is; the Open, Save As
   boxes' text. */
static void set_path(void)
{
    enum : uint32_t { HEAD = 3 }; /* "C:\" */

    for (uint32_t want = 128;; want *= 2) { /* pfil_curdir cuts to its room: more till it fits */
        if (!GROW(path, path_cap, want))
            break;

        path[0] = (char)('A' + pfil_curdrive());
        path[1] = ':';
        path[2] = '\\';

        if (pfil_curdir(0, path + HEAD, path_cap - HEAD) != PFIL_OK) {
            path[HEAD] = 0;
            break;
        }

        if (strlen(path + HEAD) + 1 < path_cap - HEAD)
            break;
    }

    open_ctl[1].text = save_ctl[1].text = path ? path : "";
}

/* dl_files: the lists: the files matching the mask, the directories; the current path. */
static void list_files(void)
{
    ls_files.n = find(&ls_files, &files_cap, mask, 0);
    ls_dirs.n = find(&ls_dirs, &dirs_cap, "*.*", PFIL_ADIR);
    ls_files.sel = ls_files.top = ls_dirs.sel = ls_dirs.top = 0;
    set_path();
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

    set_field(ls_files.items + ls_files.sel * NAMESZ);
    return why == DLOG_MOVED;
}

/* dl_dsel: the Dirs list: Enter goes into the selected one, the box stays. */
static bool dirs_pick(const uint8_t why)
{
    if (why != DLOG_ENTER || !ls_dirs.n)
        return false;

    pfil_chdir(ls_dirs.items + ls_dirs.sel * NAMESZ);
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

bool dlog_open(Session *const S)
{
    memcpy(mask, "*.BAS", 6);
    set_field(mask);
    list_files();
    dg_open.focus = 0;
    return dlog_run(S, &dg_open) == DO_OK;
}

bool dlog_saveas(Session *const S)
{
    set_field(uimn.file);
    memcpy(mask, "*.*", 4);
    list_files();
    dg_save.focus = 0;
    return dlog_run(S, &dg_save) == DS_OK;
}

/* ---- Find, Change */

uint8_t dlog_conf(Session *const S)
{
    edit_ensure();
    const uint16_t at = (uint16_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0);
    dg_conf.row = at < CONF_LOW ? CONF_LOW : CONF_HIGH;
    dg_conf.focus = 0;
    return dlog_run(S, &dg_conf);
}

uint8_t dlog_change(Session *const S)
{
    dg_chg.focus = 0;
    return (uint8_t)(dlog_run(S, &dg_chg) - DC_FV);
}
