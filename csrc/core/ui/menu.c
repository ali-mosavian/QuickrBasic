/* The menu bar, its menus, their commands (src/ui/menu.asm). */
#include "ui/menu.h"
#include "plat/pscr.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/mous.h"
#include "ui/scrn.h"
#include "ui/synt.h"
#include "ui/uimn.h"
#include "vm/dbug.h"

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
static const uint8_t alt_scan[NMENU] = {0x21, 0x12, 0x2F, 0x1F, 0x13, 0x20, 0x18, 0x23};
static const char alt_letter[] = "QWERTYUIOP    ASDFGHJKL     ZXCVBNM";
/* Alt+key's scan code: the plain key's scan code and character (mn_alta). */
static const uint8_t alt_plain[][3] = {
    {0x98, 0x48, 0}, {0xA0, 0x50, 0}, {0x9B, 0x4B, 0},    {0x9D, 0x4D, 0},
    {0x1C, 0x1C, 13}, {0xA6, 0x1C, 13}, {0x01, 0x01, 0x1B},
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
    int8_t d;      /* mn_move's step */
    uint8_t q;     /* a hotkey's search */
    bool sub;      /* a sub-menu open */
    Level box, saved;
} mn;

static char help_line[80]; /* bf.ui_hbuf */

static const Item *item(uint8_t i) { return &menus[mn.box.m][i]; }

static void swap(void)
{
    Level t = mn.box;
    mn.box = mn.saved;
    mn.saved = t;
}

/* mn_off: item it gray: always, or for want of what it needs. */
static bool gray(const Item *it)
{
    uint8_t f = it->flags;
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
static bool checked(const Item *it)
{
    uint8_t a = it->act;
    if (a == ACT_TRACE)
        return dbug.trace;
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
    scrn_fill(0, 0, SL_BAR, ' ', 80);
    for (uint8_t b = 0; b < NMENU; b++) {
        uint8_t col = menu_x[b];
        if (mn.on && b == mn.c) {
            col = scrn_fill(0, col - 1, SL_MSEL, ' ', 1);
            col = scrn_str(0, col, menu_title[b], SL_MSEL, SL_MHOT);
            scrn_fill(0, col, SL_MSEL, ' ', 1);
        } else {
            scrn_str(0, col, menu_title[b], SL_BAR, SL_HOT);
        }
    }
}

/* mn_help: item i's help on the status bar: its line in BASIC.HLP, the lines in item order over
   all the menus, separators not counted. */
static void help(uint8_t i)
{
    uint32_t line = 0;
    for (uint8_t m = 0;; m++) {
        const Item *it = menus[m];
        for (uint8_t k = 0;; k++, it++) {
            if (m == mn.box.m && k == i)
                goto found;
            if (!it->act)
                break;
            if (it->flags != MF_SEP)
                line++;
        }
    }
found:;
    const char *s = uimn.help_text;
    uint32_t left = uimn.help_len;
    while (line && left) {
        left--;
        if (*s++ == 10)
            line--;
    }
    char *d = help_line;
    for (uint32_t n = 76; left && n; n--) {
        char c = *s++;
        left--;
        if (c == 13 || c == 10)
            break;
        *d++ = c;
    }
    *d = 0;
    uimn.help = help_line;
}

/* mn_size: the box's items and width; no sub-menu open, the bar's menu's. */
static void size(void)
{
    if (!mn.sub)
        mn.box.m = mn.c;
    uint8_t cnt = 0, widest = 0;
    for (const Item *it = item(0); it->act; it++) {
        cnt++;
        if (it->flags == MF_SEP)
            continue;
        uint8_t w = (uint8_t)scrn_slen(it->text);
        uint16_t k = scrn_slen(it->key);
        if (k)
            w += (uint8_t)(k + 3);
        if (it->flags & MF_SUB)
            w += 2;
        if (w > widest)
            widest = w;
    }
    mn.box.cnt = cnt;
    mn.box.w = widest + 4;
}

/* mn_box: the box at its column and row, the lit item's help on the status bar. */
static void box(void)
{
    Level *b = &mn.box;
    scrn_box(b->top, b->l, b->cnt + 2, b->w, SL_BAR, nullptr);
    scrn_shadow(b->top, b->l, b->cnt + 2, b->w);
    for (uint8_t i = 0; i < b->cnt; i++) {
        uint8_t row = b->top + 1 + i;
        const Item *it = item(i);
        if (it->flags == MF_SEP) {
            scrn_sep(row, b->l, b->w, SL_BAR);
            continue;
        }
        uint8_t slot = SL_BAR, hot = SL_HOT;
        bool off = gray(it);
        if (off)
            slot = hot = SL_GRAY;
        if (i == b->i) {
            slot = SL_MSEL;
            hot = off ? SL_MSEL : SL_MHOT;
            help(i);
        }
        scrn_fill(row, b->l + 1, slot, ' ', b->w - 2);
        if ((it->flags & MF_CHK) && checked(it))
            scrn_fill(row, b->l + 1, slot, 7, 1);
        scrn_str(row, b->l + 2, it->text, slot, hot);
        uint16_t k = scrn_slen(it->key);
        if (k)
            scrn_str(row, (uint8_t)(b->l + b->w - 2 - k), it->key, slot, slot);
        if (it->flags & MF_SUB)
            scrn_fill(row, b->l + b->w - 3, slot, 0x10, 1);
    }
}

/* mn_drop: the bar's menu's box under its title. */
static void drop(void)
{
    size();
    uint8_t l = menu_x[mn.c] - 2;
    if ((uint8_t)(l + mn.box.w) > 79)
        l = 79 - mn.box.w;
    mn.box.l = l;
    mn.box.top = 1;
    box();
}

/* mn_move: the item lit stepped by d past separators. */
static void move(int8_t d)
{
    mn.d = d;
    do {
        int i = (int8_t)(mn.box.i + mn.d);
        if (i < 0)
            i = mn.box.cnt - 1;
        if (i >= mn.box.cnt)
            i = 0;
        mn.box.i = (uint8_t)i;
    } while (item(mn.box.i)->flags == MF_SEP);
}

/* mn_unalt: Alt still held from Alt+F: its arrows and letters as plain ones. */
static uint16_t unalt(uint16_t key)
{
    if (key & 0xFF)
        return key;
    uint8_t scan = (uint8_t)(key >> 8);
    for (uint32_t i = 0; i < countof(alt_plain); i++)
        if (alt_plain[i][0] == scan)
            return (uint16_t)(alt_plain[i][1] << 8 | alt_plain[i][2]);
    uint8_t c = menu_altchar(scan);
    if (!c)
        return key;
    return (uint16_t)(scan << 8 | (c == ' ' ? 0 : c));
}

uint8_t menu_altchar(uint8_t scan)
{
    if (scan < 0x10 || scan > 0x32)
        return 0;
    return (uint8_t)alt_letter[scan - 0x10];
}

/* mn_find: the menu whose title's key is c; -1 none. */
static int find(uint8_t c)
{
    for (int b = 0; b < NMENU; b++) {
        const char *amp = strchr(menu_title[b], '&');
        if (amp && (uint8_t)amp[1] == c)
            return b;
    }
    return -1;
}

/* An item's hotkey: the letter after '&', else its text's 0. */
static uint8_t hotkey(const Item *it)
{
    const char *s = it->text;
    while (*s && *s != '&')
        s++;
    if (*s)
        s++;
    return uimn_upper((uint8_t)*s);
}

/* The menus' loop's next step. */
enum { NEXT, OPEN, CHOSEN, QUIT };

/* @mnr_so: item it's sub-menu, its first item lit, by its parent's: right of the box, else
   left. */
static void sub_open(const Item *it)
{
    uint8_t m = it->act;
    swap();
    mn.box.m = m;
    mn.sub = true;
    mn.box.i = 0;
    size();
    mn.box.top = mn.saved.i + mn.saved.top;
    uint8_t l = mn.saved.l + mn.saved.w;
    if ((uint8_t)(l + mn.box.w) > 79)
        l = mn.saved.l - mn.box.w;
    mn.box.l = l;
}

/* @mnr_go: item it chosen: a command, or its sub-menu opened. */
static int go(const Item *it)
{
    if (it->flags & MF_SUB) {
        sub_open(it);
        return NEXT;
    }
    return CHOSEN;
}

/* The mouse's event in the menus (@mnr_ms). */
static int mouse(const Item **chosen)
{
    for (;;) {
        uint8_t row = uimn.mouse.row, col = uimn.mouse.col;
        if (!row) {
            uint8_t t;
            if (uimn.mouse.event == MS_UP || !mous_title(col, &t))
                return NEXT;
            if (mn.open && t == mn.c)
                return NEXT;
            mn.c = t;
            return OPEN;
        }
        if (mn.open) {
            uint8_t a = row - mn.box.top, x = col - mn.box.l;
            if (row > mn.box.top && (uint8_t)(a - 1) < mn.box.cnt && col > mn.box.l &&
                (uint8_t)(x + 1) < mn.box.w) {
                const Item *it = item(a - 1);
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
        swap(); /* off the sub-menu: the parent's click */
        mn.sub = false;
    }
}

void menu_run(uint8_t m, bool open)
{
    edit_commit();
    mn.c = m;
    mn.open = open;
    mn.on = true;
    mn.sub = false;
    mn.box.i = 0;
    size();
    for (;;) {
        uimn.help = uimn_menu_help;
        draw_all();
        if (mn.open) {
            if (!mn.sub) {
                drop();
            } else {
                swap();
                drop();
                swap();
                box();
            }
            draw_status();
        }
        scrn_shape(PSCR_HIDDEN);
        uint16_t key = uimn_getkey();
        int next = NEXT;
        const Item *chosen = nullptr;
        if (key == KEY_ALT) {
            next = QUIT;
        } else if (key == KEY_MOUSE) {
            next = mouse(&chosen);
        } else {
            key = unalt(key);
            uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);
            if (ch == 0x1B) {
                if (!mn.sub) {
                    next = QUIT;
                } else { /* back to the parent */
                    swap();
                    mn.sub = false;
                }
            } else if (ch == 0x0D) {
                if (!mn.open) {
                    next = OPEN;
                } else {
                    chosen = item(mn.box.i);
                    if (gray(chosen))
                        pscr_beep();
                    else
                        next = go(chosen);
                }
            } else if (scan == 0x4B) {
                if (mn.sub) {
                    swap();
                    mn.sub = false;
                } else {
                    mn.c = mn.c ? mn.c - 1 : NMENU - 1;
                    mn.sub = false;
                    mn.box.i = 0;
                    size();
                }
            } else if (scan == 0x4D) {
                const Item *it = item(mn.box.i);
                if (!mn.sub && mn.open && (it->flags & MF_SUB) && !gray(it)) {
                    sub_open(it);
                } else {
                    mn.c = mn.c + 1 < NMENU ? mn.c + 1 : 0;
                    mn.sub = false;
                    mn.box.i = 0;
                    size();
                }
            } else if (scan == 0x48 || scan == 0x50) {
                if (!mn.open)
                    next = OPEN;
                else
                    move(scan == 0x48 ? -1 : 1);
            } else if (scan == 0x44) {
                next = QUIT;
            } else if (ch) {
                uint8_t c = uimn_upper(ch);
                if (!mn.open) {
                    int t = find(c); /* a title's key opens it */
                    if (t >= 0) {
                        mn.c = (uint8_t)t;
                        next = OPEN;
                    }
                } else { /* an item's key runs it */
                    for (mn.q = 0; mn.q < mn.box.cnt; mn.q++) {
                        const Item *it = item(mn.q);
                        if (it->flags == MF_SEP || hotkey(it) != c)
                            continue;
                        if (!gray(it)) {
                            chosen = it;
                            next = go(it);
                        } else { /* gray: lit, a beep (COW menu.c) */
                            mn.box.i = mn.q;
                            pscr_beep();
                        }
                        break;
                    }
                }
            }
        }
        if (next == OPEN) {
            mn.open = true;
            mn.sub = false;
            mn.box.i = 0;
            size();
        } else if (next == QUIT) {
            mn.on = false;
            uimn.help = nullptr;
            return;
        } else if (next == CHOSEN) {
            mn.on = false;
            uimn.help = nullptr;
            uimn_act(chosen->act);
            return;
        }
    }
}

bool menu_alt(uint8_t scan)
{
    for (uint8_t b = 0; b < NMENU; b++) {
        if (alt_scan[b] == scan) {
            menu_run(b, true);
            return true;
        }
    }
    return false;
}
