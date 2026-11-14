/* The menu bar, its menus, their commands (src/ui/menu.asm). */
#include "ui/menu.h"
#include "session.h"
#include "plat/pscr.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/mous.h"
#include "ui/scrn.h"
#include "ui/synt.h"
#include "ui/uimn.h"

/* An item: its command, flags, text ('&' before its hotkey), its key's name ("" none). A
   separator is {ACT_NONE, MF_SEP}; act 0 ends a menu. */
typedef struct {
    uint8_t act, flags;
    const char *text, *key;
} Item;

#define SEP {ACT_NONE, MF_SEP, "", ""}

static const Item m_file[] = {
    {ACT_NEW, 0, "&New", ""},
    {ACT_OPEN, 0, "&Open...", ""},
    {ACT_SAVE, 0, "&Save", ""},
    {ACT_SAVEAS, 0, "Save &As...", ""},
    SEP,
    {ACT_NONE, MF_GRAY, "&Print...", ""},
    SEP,
    {ACT_EXIT, 0, "E&xit", ""},
    {},
};
static const Item m_edit[] = {
    {ACT_CUT, MF_SEL, "Cu&t", "Shift+Del"},
    {ACT_COPY, MF_SEL, "&Copy", "Ctrl+Ins"},
    {ACT_PASTE, MF_CLP, "&Paste", "Shift+Ins"},
    {ACT_CLEAR, MF_SEL, "Cl&ear", "Del"},
    SEP,
    {ACT_NSUB, 0, "New &SUB...", ""},
    {ACT_NFN, 0, "New &FUNCTION...", ""},
    {},
};
static const Item m_view[] = {
    {ACT_SUBS, 0, "&SUBs...", "F2"},
    {ACT_SPLIT, 0, "S&plit", ""},
    {ACT_VSPLIT, 0, "Split &Vertically", ""},
    {ACT_OUT, 0, "Output &Screen", "F4"},
    {},
};
static const Item m_srch[] = {
    {ACT_FIND, MF_ED, "&Find...", ""},
    {ACT_FIND3, 0, "&Repeat Last Find", "F3"},
    {ACT_CHANGE, MF_ED, "&Change...", ""},
    {},
};
static const Item m_run[] = {
    {ACT_START, 0, "&Start", "Shift+F5"},
    {ACT_RESTART, 0, "&Restart", ""},
    {ACT_CONT, 0, "Co&ntinue", "F5"},
    {},
};
static const Item m_dbg[] = {
    {ACT_STEP, 0, "&Step", "F8"},
    {ACT_PSTEP, 0, "Step &Over", "F7"},
    {ACT_SOUT, 0, "Step O&ut", "Shift+F8"},
    SEP,
    {ACT_TRACE, MF_CHK, "&Trace On", ""},
    SEP,
    {ACT_BP, MF_ED, "Toggle &Breakpoint", "F9"},
    {ACT_BPCLR, 0, "&Clear All Breakpoints", ""},
    {ACT_SETNX, MF_ED, "Set &Next Statement", ""},
    {},
};
static const Item m_opt[] = {
    {NMENU, MF_SUB, "&Theme", ""},
    {ACT_SYNHL, MF_CHK, "Syntax &Highlighting", ""},
    SEP,
    {ACT_NONE, MF_GRAY, "Help &Path...", ""},
    {ACT_SYNTAX, MF_CHK, "&Syntax Checking", ""},
    {},
};
static const Item m_theme[] = {
    {ACT_THQB, MF_CHK, "&QBasic Colors", ""},
    {ACT_THDK, MF_CHK, "One &Dark", ""},
    {ACT_TH2, MF_CHK, "One &Light", ""},
    {ACT_TH2 + 1, MF_CHK, "&Ayu Mirage", ""},
    {ACT_TH2 + 2, MF_CHK, "Ay&u Light", ""},
    {ACT_TH2 + 3, MF_CHK, "&Gruvbox Dark Soft", ""},
    {ACT_TH2 + 4, MF_CHK, "Gruv&box Light", ""},
    {ACT_TH2 + 5, MF_CHK, "&JetBrains Dark", ""},
    {ACT_TH2 + 6, MF_CHK, "JetBrains Ligh&t", ""},
    {},
};
static const Item m_help[] = {
    {ACT_NONE, MF_GRAY, "&Index", ""},
    {ACT_NONE, MF_GRAY, "&Contents", ""},
    {ACT_NONE, MF_GRAY, "&Topic:", "F1"},
    {ACT_NONE, MF_GRAY, "&Using Help", "Shift+F1"},
    SEP,
    {ACT_ABOUT, 0, "&About...", ""},
    {},
};
static_assert(ACT_TH2 + 6 == ACT_SOUT - 1);

/* mn_it: the menus, then the sub-menus from NMENU on. */
static const Item *const menus[] = {m_file, m_edit, m_view, m_srch, m_run, m_dbg, m_opt, m_help,
                                    m_theme};

const uint8_t menu_x[NMENU] = {3, 9, 15, 21, 29, 34, 41, 74}; /* QBasic's (uirsrcc.c) */
const char *const menu_title[NMENU] = {"&File", "&Edit",  "&View",    "&Search",
                                       "&Run",  "&Debug", "&Options", "&Help"};
/* Each title's letter's scan code. */
static const uint8_t alt_scan[NMENU] = {0x21, 0x12, 0x2F, 0x1F, 0x13, 0x20, 0x18, 0x23};

/* The letters' keys, SCAN_ALT_FIRST .. SCAN_ALT_LAST. */
static const char alt_letter[] = "QWERTYUIOP    ASDFGHJKL     ZXCVBNM";
static_assert(countof(alt_letter) - 1 == SCAN_ALT_LAST - SCAN_ALT_FIRST + 1);

/* Alt+key's scan code: the plain key's scan code and character (mn_alta). */
static const struct {
    uint8_t alt, scan, ch;
} alt_plain[] = {
    {SCAN_ALT_UP, SCAN_UP, 0},          {SCAN_ALT_DOWN, SCAN_DOWN, 0},
    {SCAN_ALT_LEFT, SCAN_LEFT, 0},      {SCAN_ALT_RIGHT, SCAN_RIGHT, 0},
    {SCAN_ENTER, SCAN_ENTER, CH_ENTER}, {SCAN_ALT_PAD_ENTER, SCAN_ENTER, CH_ENTER},
    {SCAN_ESC, SCAN_ESC, CH_ESC},
};

/* A box's level: the menu shown (its menus[] index), the item lit, its items, width, column,
   top row; a sub-menu's parent's kept in saved (mn_swap). */
typedef struct {
    uint8_t m, i, cnt, w, l, top;
} Level;

static struct {
    uint8_t c;     /* the bar's menu */
    bool open;     /* its box open, else the bar lit */
    bool on;       /* the bar's title lit */
    bool sub;      /* a sub-menu open */
    Level box, saved;
} mn;

enum { HELP_MAX = 76 }; /* a help line's characters at most */
static char help_line[PSCR_COLS]; /* bf.ui_hbuf */

static const Item *item(const uint8_t i) { return &menus[mn.box.m][i]; }

static void swap(void)
{
    const Level t = mn.box;
    mn.box = mn.saved;
    mn.saved = t;
}

/* mn_off: item it gray: always, or for want of what it needs. */
static bool gray(const Item *const it)
{
    const uint8_t f = it->flags;
    if (f & MF_GRAY)
        return true;
    if ((f & MF_SEL) && !uimn.ed.sel_on)
        return true;
    if ((f & MF_CLP) && !uimn.text.clip_len)
        return true;
    if ((f & MF_ED) && uimn.focus)
        return true;
    return false;
}

/* mn_chk: check item it on (mn_flags). */
static bool checked(Session *const S, const Item *const it)
{
    const uint8_t a = it->act;
    if (a == ACT_TRACE)
        return session_tracing(S);
    if (a == ACT_SYNTAX)
        return uimn.text.check;
    if (a == ACT_SYNHL)
        return synt.on;
    if (a == ACT_THQB)
        return them.cur == 0;
    if (a == ACT_THDK)
        return them.cur == 1;
    if (a >= ACT_TH2 && a < ACT_TH2 + NTHEME - 2)
        return them.cur == a - ACT_TH2 + 2;
    return false;
}

void menu_bar(void)
{
    scrn_fill(0, 0, SL_BAR, ' ', PSCR_COLS);
    for (uint8_t b = 0; b < NMENU; b++) {
        if (mn.on && b == mn.c) {
            const uint8_t title = scrn_fill(0, menu_x[b] - 1, SL_MSEL, ' ', 1);
            const uint8_t after = scrn_str(0, title, menu_title[b], SL_MSEL, SL_MHOT);
            scrn_fill(0, after, SL_MSEL, ' ', 1);
        } else {
            scrn_str(0, menu_x[b], menu_title[b], SL_BAR, SL_HOT);
        }
    }
}

/* The help lines of items' first n, its end at most: one an item, separators not counted. */
static uint32_t help_lines(const Item *const items, const uint32_t n)
{
    uint32_t lines = 0;
    for (uint32_t k = 0; k < n && items[k].act; k++)
        if (items[k].flags != MF_SEP)
            lines++;
    return lines;
}

/* Item i's line in BASIC.HLP: the lines in item order over all the menus. */
static uint32_t help_index(const uint8_t i)
{
    uint32_t line = 0;
    for (uint8_t m = 0; m < mn.box.m; m++)
        line += help_lines(menus[m], UINT32_MAX);
    return line + help_lines(menus[mn.box.m], i);
}

/* mn_help: item i's help on the status bar. */
static void help(const uint8_t i)
{
    const char *const s = uimn.help_text;
    const uint32_t len = uimn.help_len;
    uint32_t pos = 0;
    for (uint32_t line = help_index(i); line && pos < len; pos++)
        if (s[pos] == '\n')
            line--;
    uint32_t n = 0;
    for (; n < HELP_MAX && pos < len; n++, pos++) {
        const char c = s[pos];
        if (c == '\r' || c == '\n')
            break;
        help_line[n] = c;
    }
    help_line[n] = 0;
    uimn.help = help_line;
}

/* An item's room past its text: its key's name's gap, a sub-menu's arrow; a box's past its
   widest item. */
enum { KEY_GAP = 3, SUB_ARROW = 2, BOX_PAD = 4 };

/* mn_size: the box's items and width; no sub-menu open, the bar's menu's. */
static void size(void)
{
    if (!mn.sub)
        mn.box.m = mn.c;
    uint8_t cnt = 0, widest = 0;
    for (; item(cnt)->act; cnt++) {
        const Item *const it = item(cnt);
        if (it->flags == MF_SEP)
            continue;
        uint8_t w = (uint8_t)scrn_slen(it->text);
        const uint16_t k = scrn_slen(it->key);
        if (k)
            w += (uint8_t)(k + KEY_GAP);
        if (it->flags & MF_SUB)
            w += SUB_ARROW;
        if (w > widest)
            widest = w;
    }
    mn.box.cnt = cnt;
    mn.box.w = widest + BOX_PAD;
}

enum : uint8_t { CHECK_MARK = 0x07 }; /* CP437's bullet */

/* Item i of the box on row row. */
static void box_item(Session *const S, const Level *const b, const uint8_t i, const uint8_t row)
{
    const Item *const it = item(i);
    const bool off = gray(it);
    uint8_t slot = off ? SL_GRAY : SL_BAR, hot = off ? SL_GRAY : SL_HOT;
    if (i == b->i) {
        slot = SL_MSEL;
        hot = off ? SL_MSEL : SL_MHOT;
        help(i);
    }
    scrn_fill(row, b->l + 1, slot, ' ', b->w - 2);
    if ((it->flags & MF_CHK) && checked(S, it))
        scrn_fill(row, b->l + 1, slot, CHECK_MARK, 1);
    scrn_str(row, b->l + 2, it->text, slot, hot);
    const uint16_t k = scrn_slen(it->key);
    if (k)
        scrn_str(row, (uint8_t)(b->l + b->w - 2 - k), it->key, slot, slot);
    if (it->flags & MF_SUB)
        scrn_fill(row, b->l + b->w - 3, slot, GLYPH_POINT_RIGHT, 1);
}

/* mn_box: the box at its column and row, the lit item's help on the status bar. */
static void box(Session *const S)
{
    const Level *const b = &mn.box;
    scrn_box(b->top, b->l, b->cnt + 2, b->w, SL_BAR, nullptr);
    scrn_shadow(b->top, b->l, b->cnt + 2, b->w);
    for (uint8_t i = 0; i < b->cnt; i++) {
        const uint8_t row = b->top + 1 + i;
        if (item(i)->flags == MF_SEP)
            scrn_sep(row, b->l, b->w, SL_BAR);
        else
            box_item(S, b, i, row);
    }
}

/* mn_drop: the bar's menu's box under its title. */
static void drop(Session *const S)
{
    size();
    uint8_t l = menu_x[mn.c] - 2;
    if ((uint8_t)(l + mn.box.w) > (PSCR_COLS - 1))
        l = (PSCR_COLS - 1) - mn.box.w;
    mn.box.l = l;
    mn.box.top = 1;
    box(S);
}

/* mn_move: the item lit stepped by d past separators. */
static void move(const int8_t d)
{
    do {
        int i = (int8_t)(mn.box.i + d);
        if (i < 0)
            i = mn.box.cnt - 1;
        if (i >= mn.box.cnt)
            i = 0;
        mn.box.i = (uint8_t)i;
    } while (item(mn.box.i)->flags == MF_SEP);
}

/* mn_unalt: Alt still held from Alt+F: its arrows and letters as plain ones. */
static uint16_t unalt(const uint16_t key)
{
    if ((uint8_t)key)
        return key;
    const uint8_t scan = (uint8_t)(key >> 8);
    for (uint32_t i = 0; i < countof(alt_plain); i++)
        if (alt_plain[i].alt == scan)
            return (uint16_t)(alt_plain[i].scan << 8 | alt_plain[i].ch);
    const uint8_t c = menu_altchar(scan);
    if (!c)
        return key;
    return (uint16_t)(scan << 8 | (c == ' ' ? 0 : c));
}

uint8_t menu_altchar(const uint8_t scan)
{
    if (scan < SCAN_ALT_FIRST || scan > SCAN_ALT_LAST)
        return 0;
    return (uint8_t)alt_letter[scan - SCAN_ALT_FIRST];
}

/* The letter after s's '&', upper case; 0 none. */
static uint8_t hot_letter(const char *const s)
{
    const char *const amp = strchr(s, '&');
    return amp ? upcase((uint8_t)amp[1]) : 0;
}

/* mn_find: the menu whose title's key is c; -1 none. */
static int find(const uint8_t c)
{
    for (int b = 0; b < NMENU; b++)
        if (hot_letter(menu_title[b]) == c)
            return b;
    return -1;
}

/* The menus' loop's next step. */
typedef enum { NEXT, OPEN, CHOSEN, QUIT } Step;

/* The bar's menu's box afresh, its first item lit. */
static void restart(void)
{
    mn.sub = false;
    mn.box.i = 0;
    size();
}

/* Back from the sub-menu to its parent. */
static void sub_close(void)
{
    swap();
    mn.sub = false;
}

/* @mnr_so: item it's sub-menu, its first item lit, by its parent's: right of the box, else
   left. */
static void sub_open(const Item *const it)
{
    swap();
    mn.box.m = it->act;
    mn.sub = true;
    mn.box.i = 0;
    size();
    mn.box.top = mn.saved.i + mn.saved.top;
    uint8_t l = mn.saved.l + mn.saved.w;
    if ((uint8_t)(l + mn.box.w) > (PSCR_COLS - 1))
        l = mn.saved.l - mn.box.w;
    mn.box.l = l;
}

/* @mnr_go: item it chosen: a command, or its sub-menu opened. */
static Step go(const Item *const it)
{
    if (it->flags & MF_SUB) {
        sub_open(it);
        return NEXT;
    }
    return CHOSEN;
}

/* The mouse on the menu bar's title under column col. */
static Step mouse_bar(const uint8_t col)
{
    uint8_t t;
    if (uimn.mouse.event == MS_UP || !mous_title(col, &t))
        return NEXT;
    if (mn.open && t == mn.c)
        return NEXT;
    mn.c = t;
    return OPEN;
}

/* The mouse's event in the menus (@mnr_ms). */
static Step mouse(const Item **const chosen)
{
    for (;;) {
        const uint8_t row = uimn.mouse.row, col = uimn.mouse.col;
        if (!row)
            return mouse_bar(col);
        if (mn.open) {
            const uint8_t a = row - mn.box.top, x = col - mn.box.l;
            if (row > mn.box.top && (uint8_t)(a - 1) < mn.box.cnt && col > mn.box.l &&
                (uint8_t)(x + 1) < mn.box.w) {
                const Item *const it = item(a - 1);
                if (it->flags & MF_SEP)
                    return NEXT;
                mn.box.i = a - 1;
                if (uimn.mouse.event != MS_UP || gray(it))
                    return NEXT;
                *chosen = it;
                return go(it);
            }
        }
        if (uimn.mouse.event != MS_PRESS)
            return NEXT;
        if (!mn.sub)
            return QUIT;
        sub_close(); /* off the sub-menu: the parent's click */
    }
}

static Step key_esc(void)
{
    if (!mn.sub)
        return QUIT;
    sub_close();
    return NEXT;
}

static Step key_enter(const Item **const chosen)
{
    if (!mn.open)
        return OPEN;
    *chosen = item(mn.box.i);
    if (!gray(*chosen))
        return go(*chosen);
    pscr_beep();
    return NEXT;
}

static void key_left(void)
{
    if (mn.sub) {
        sub_close();
        return;
    }
    mn.c = mn.c ? mn.c - 1 : NMENU - 1;
    restart();
}

static void key_right(void)
{
    const Item *const it = item(mn.box.i);
    if (!mn.sub && mn.open && (it->flags & MF_SUB) && !gray(it)) {
        sub_open(it);
        return;
    }
    mn.c = mn.c + 1 < NMENU ? mn.c + 1 : 0;
    restart();
}

/* Letter c: a title's key opens it; an open box's item's key runs it. */
static Step key_letter(const uint8_t c, const Item **const chosen)
{
    if (!mn.open) {
        const int t = find(c);
        if (t < 0)
            return NEXT;
        mn.c = (uint8_t)t;
        return OPEN;
    }
    for (uint8_t i = 0; i < mn.box.cnt; i++) {
        const Item *const it = item(i);
        if (it->flags == MF_SEP || hot_letter(it->text) != c)
            continue;
        if (!gray(it)) {
            *chosen = it;
            return go(it);
        }
        mn.box.i = i; /* gray: lit, a beep (COW menu.c) */
        pscr_beep();
        return NEXT;
    }
    return NEXT;
}

/* A key in the menus. */
static Step on_key(const uint16_t k, const Item **const chosen)
{
    if (k == KEY_ALT)
        return QUIT;
    if (k == KEY_MOUSE)
        return mouse(chosen);
    const uint16_t key = unalt(k);
    const uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);
    if (ch == CH_ESC)
        return key_esc();
    if (ch == CH_ENTER)
        return key_enter(chosen);
    if (scan == SCAN_LEFT) {
        key_left();
        return NEXT;
    }
    if (scan == SCAN_RIGHT) {
        key_right();
        return NEXT;
    }
    if (scan == SCAN_UP || scan == SCAN_DOWN) {
        if (!mn.open)
            return OPEN;
        move(scan == SCAN_UP ? -1 : 1);
        return NEXT;
    }
    if (scan == SCAN_F10)
        return QUIT;
    if (ch)
        return key_letter(upcase(ch), chosen);
    return NEXT;
}

/* The screen, the open box (a sub-menu's over its parent's), the status bar. */
static void draw(Session *const S)
{
    uimn.help = uimn_menu_help;
    draw_all(S);
    if (!mn.open)
        return;
    if (!mn.sub) {
        drop(S);
    } else {
        swap();
        drop(S);
        swap();
        box(S);
    }
    draw_status();
}

void menu_run(Session *const S, const uint8_t m, const bool open)
{
    edit_commit(S);
    mn.c = m;
    mn.open = open;
    mn.on = true;
    restart();
    for (;;) {
        draw(S);
        scrn_shape(PSCR_HIDDEN);
        const Item *chosen = nullptr;
        const Step next = on_key(uimn_getkey(), &chosen);
        if (next == OPEN) {
            mn.open = true;
            restart();
        } else if (next == QUIT || next == CHOSEN) {
            mn.on = false;
            uimn.help = nullptr;
            if (next == CHOSEN)
                uimn_act(S, chosen->act);
            return;
        }
    }
}

bool menu_alt(Session *const S, const uint8_t scan)
{
    for (uint8_t b = 0; b < NMENU; b++) {
        if (alt_scan[b] == scan) {
            menu_run(S, b, true);
            return true;
        }
    }
    return false;
}
