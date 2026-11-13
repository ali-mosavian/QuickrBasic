/* The editor: its start, the key loop, the menu commands, running the program (src/ui/ui.asm). */
#include "ui/uimn.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/prog.h"
#include "plat/pfil.h"
#include "plat/pkey.h"
#include "plat/pscr.h"
#include "plat/psys.h"
#include "ui/cmpl.h"
#include "ui/dlog.h"
#include "ui/dpan.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/lmap.h"
#include "ui/menu.h"
#include "ui/mous.h"
#include "ui/scrn.h"
#include "ui/synt.h"
#include "ui/text.h"
#include "ui/them.h"
#include "ui/udbg.h"
#include "ui/view.h"
#include "ui/wind.h"
#include "vm/exec.h"

Uimn uimn; /* zeroed: its line buffers' texts are set by uimn_start */

static struct {
    uint8_t alt;      /* alt_t: 1 Alt went down alone, 2 it was down at the call */
    bool run_wait;    /* ui_rf: a run: always wait for a key at its end */
    uint8_t crow, ccol; /* ui_cpos: the output's cursor before an immediate line */
    uint8_t ecode;    /* ui_ecode */
    bool at_error;    /* ui_eln: the cursor is at the error's line */
    bool wrap;        /* ui_wrap: a search wraps at the end */
    bool load_new;    /* ul_new: a file not found is a new one (the command line's) */
    bool change_all;  /* uc_all */
    bool changed_one; /* uc_f: Change found one */
    uint32_t start_line; /* uc_sl, uc_sc: Change All's cursor, back after */
    uint16_t start_col;
    uint32_t at_line; /* us_l, us_c: the search's place */
    uint16_t at_col;
} ui = {.wrap = true};

const char uimn_ok[] = "OK";
const char uimn_cancel[] = "Cancel";
const char uimn_helpbtn[] = "&Help";
const char uimn_untitled[] = "Untitled";
const char uimn_menu_help[] = "Enter=Display Menu   Esc=Cancel   Arrow=Next Item";
const char uimn_dlog_help[] = "Enter=Execute   Esc=Cancel   Tab=Next Field   Arrow=Next Item";

static const char s_notfound[] = "File not found";
static const char s_overw[] = "File already exists. Overwrite?";
static const char s_nomatch[] = "Match not found";
static const char s_chdone[] = "Change complete";
static const char s_notsaved[] = "Loaded file is not saved. Save it now?";
static const char s_access[] = "Path/File access error";
static const char s_nohelp[] = "Help is not available";
static const char s_about[] = "QuickrBasic||A full-screen editor";
static const char s_welcome[] = "Welcome to QuickrBasic||"
                                "Lines may have numbers or not; GOTO and|"
                                "GOSUB need them. F5 runs the program.";
static const char s_anykey[] = "Press any key to continue";
static const MsgButton bt_welcome[] = {{"Press Esc to clear this dialog box", 0}, {}};

[[noreturn]] static void loop(void);
static void exec_line(void);

/* ui_win: F6. */
static void next_window(void) { wind_next(); }

/* ui_nohelp: F1. */
static void nohelp(void) { dlog_ok(s_nohelp); }

/* QBasic's status lines (qbasmsgs.txt) less the keys we lack; their keys for the mouse. */
const StatusKeys uimn_keys_edit = {
    "<Shift+F1=Help> <F6=Window> <F5=Run> <F8=Step>",
    {{15, nohelp}, {27, next_window}, {36, uimn_cont}, {46, udbg_step}},
};
const StatusKeys uimn_keys_run = {
    "<F5=Continue> <F7=Over> <F8=Step> <Shift+F8=Out> <F9=Bkpt>",
    {{13, uimn_cont}, {23, udbg_pstep}, {33, udbg_step}, {48, udbg_sout}, {58, udbg_bp}},
};
const StatusKeys uimn_keys_imm = {
    "<Shift+F1=Help> <F6=Window> <Enter=Execute Line>",
    {{15, nohelp}, {27, next_window}, {48, exec_line}},
};

char *uimn_dec(char *dst, uint16_t v)
{
    char digits[5];
    int n = 0;
    do {
        digits[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        *dst++ = digits[--n];
    return dst;
}

/* ---- pages */

/* ui_in: page 0, the editor, shown. */
static void page_in(void)
{
    pscr_page(0);
    them_in();
}

/* ui_out: page 1, the output, shown. */
static void page_out(void)
{
    pscr_page(1);
    them_out();
}

/* ui_outclr: page 1 shown and cleared. */
static void page_clear(void)
{
    them_out();
    pscr_page(1);
    pscr_scroll(0, 0, 0, 24, 79, 0x07);
    pscr_cursor(1, 0, 0);
    cons.col = 0;
}

/* ---- keys */

uint16_t uimn_getkey(void)
{
    scrn_flip();
    mous_show();
    ui.alt = (pkey_shift() & 8) >> 2; /* 2: the Alt held from before */
    uint16_t key;
    for (;;) {
        if (pkey_ready()) {
            PkeyKey k = pkey_read();
            if (k.ch == 0xE0 && k.scan)
                k.ch = 0;
            key = (uint16_t)(k.scan << 8 | k.ch);
            break;
        }
        if (mous_poll()) {
            key = KEY_MOUSE;
            break;
        }
        if (pkey_shift() & 8) {
            if (!ui.alt)
                ui.alt = 1;
            continue;
        }
        bool tap = ui.alt == 1;
        ui.alt = 0;
        if (tap) {
            key = KEY_ALT;
            break;
        }
    }
    uimn.keyflags = pkey_shift();
    mous_hide();
    return key;
}

/* im_key: a key in the immediate window. */
static void imm_key(uint16_t key)
{
    uint8_t ch = (uint8_t)key;
    if (ch == 0x0D) {
        exec_line();
        return;
    }
    if (!ch) {
        if (key >> 8 == 0x48) {
            edit_imm_up();
            return;
        }
        if (key >> 8 == 0x50) {
            edit_imm_down();
            return;
        }
    }
    if (ch >= 0x20 && uimn.immline.len >= ED_COLS - 1)
        return;
    dlog_ledit(&uimn.immline, key);
}

/* Ctrl+A to Ctrl+Z as WordStar's: the scan code of the key they stand for (COW edittbls.inc). */
static const uint8_t wordstar[26] = {
    0x73, 0, 0x51, 0x4D, 0x48, 0x74, 0x53, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0x49, 0x4B, 0, 0, 0x52, 0, 0x50, 0, 0,
};
/* Ctrl+Q's second key: the letter's code, the scan code (0 erases to the line's end). */
static const uint8_t wordstar_q[][2] = {{0x13, 0x47}, {0x04, 0x4F}, {0x12, 0x77}, {0x03, 0x75},
                                        {0x19, 0}};

static void lb_left(void) { edit_lb_left(&uimn.edline); }
static void lb_right(void) { edit_lb_right(&uimn.edline); }
static void lb_home(void) { edit_lb_home(&uimn.edline); }
static void lb_end(void) { edit_lb_end(&uimn.edline); }
static void lb_wleft(void) { edit_lb_wleft(&uimn.edline); }
static void lb_wright(void) { edit_lb_wright(&uimn.edline); }

/* The keys that move the cursor. */
static const struct {
    uint8_t scan;
    void (*run)(void);
} moves[] = {
    {0x48, edit_up},   {0x50, edit_down},  {0x4B, lb_left},    {0x4D, lb_right},
    {0x47, lb_home},   {0x4F, lb_end},     {0x49, edit_pgup},  {0x51, edit_pgdn},
    {0x77, edit_first}, {0x75, edit_last}, {0x73, lb_wleft},   {0x74, lb_wright},
};

/* ed_key: a key in the program window: a shifted move selects. */
static void edit_key(uint16_t key)
{
    LineBuf *b = &uimn.edline;
    uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);
    bool shift = uimn.keyflags & 3;
    if (!ch) {
        for (uint32_t i = 0; i < countof(moves); i++) {
            if (moves[i].scan != scan)
                continue;
            if (shift)
                edit_sel_mark();
            else
                uimn.ed.sel_on = false;
            moves[i].run();
            return;
        }
        if (scan == 0x53) {
            if (shift) {
                edit_cut();
            } else if (edit_sel()) {
                edit_clear();
            } else {
                uimn.ed.sel_on = false;
                edit_del();
            }
        } else if (scan == 0x52) {
            if (shift)
                edit_paste();
            else
                uimn.ed.insert = !uimn.ed.insert;
        } else if (scan == 0x92) {
            edit_copy();
        }
        return;
    }
    if (uimn.ed.prefix) {
        uimn.ed.prefix = 0;
        ch &= 0x1F;
        for (uint32_t i = 0; i < countof(wordstar_q); i++) {
            if (wordstar_q[i][0] != ch)
                continue;
            if (wordstar_q[i][1]) {
                edit_key((uint16_t)(wordstar_q[i][1] << 8));
            } else { /* Ctrl+Q,Y: to the line's end out */
                uimn.ed.sel_on = false;
                edit_lb_cut(b, b->col, b->len);
            }
            return;
        }
        return;
    }
    if (ch == 0x11) { /* Ctrl+Q: a second key follows */
        uimn.ed.prefix = ch;
        return;
    }
    if (ch <= 0x1A && wordstar[ch - 1]) {
        edit_key((uint16_t)(wordstar[ch - 1] << 8));
        return;
    }
    switch (ch) {
    case 0x0D:
        uimn.ed.sel_on = false;
        edit_enter();
        return;
    case 0x08:
        if (edit_sel())
            edit_clear();
        else
            edit_bs();
        return;
    case 0x09:
        uimn.ed.sel_on = false;
#if AC
        if (cmpl_tab())
            return;
#endif
        edit_tab();
        return;
    case 0x19:
        uimn.ed.sel_on = false;
        edit_ctrly();
        return;
    case 0x1B:
        uimn.ed.sel_on = false;
        return;
    default:
        break;
    }
    if (ch < 0x20)
        return;
    if (edit_sel() == SEL_PIECE)
        edit_clear();
    uimn.ed.sel_on = false;
    edit_lb_type(b, ch, LNMAX);
}

/* ---- Search */

/* ui_wordc: c in a word: a letter, a digit, or one of "!#$%&." (QBasic's wordCharBitMask,
   uiword.asm). */
static bool wordc(uint8_t c)
{
    c = uimn_upper(c);
    if (c < '0')
        return memchr("!#$%&.", c, 6) != nullptr;
    return c <= '9' || (c >= 'A' && c <= 'Z');
}

/* ui_match: Find What at column col of uimn.line. */
static bool match(uint16_t col)
{
    const uint8_t *s = uimn.line + col, *w = uimn.find_what.text;
    uint16_t n = uimn.find_what.len;
    for (uint16_t i = 0; i < n; i++) {
        uint8_t a = s[i], b = w[i];
        if (!uimn.match_case) {
            a = uimn_upper(a);
            b = uimn_upper(b);
        }
        if (a != b)
            return false;
    }
    if (!uimn.whole_word)
        return true;
    if (wordc(s[n])) /* a whole word: no letter or digit on either side */
        return false;
    return !col || !wordc(s[-1]);
}

/* ui_search: Find What from past the cursor, wrapping (ui.wrap): found, it is selected. */
static bool search(void)
{
    uint16_t len = uimn.find_what.len;
    if (!len)
        return false;
    edit_commit();
    ui.at_line = uimn.ed.line;
    ui.at_col = uimn.edline.col + 1;
    for (uint32_t n = uimn.text.n + 1; n; n--) { /* every line, this one twice */
        uint16_t got = (uint16_t)text_get(ui.at_line, uimn.line);
        if (got >= len) {
            uint16_t last = got - len;
            for (uint16_t col = ui.at_col; col <= last; col++) {
                if (!match(col))
                    continue;
                edit_load(ui.at_line);
                uimn.ed.sel_col = col;
                uimn.ed.sel_line = ui.at_line;
                uimn.ed.sel_on = true;
                uimn.edline.col = col + len;
                return true;
            }
        }
        ui.at_col = 0;
        uint32_t next = ui.at_line + 1;
        if (next >= uimn.text.n) {
            if (!ui.wrap)
                return false;
            next = 0;
        }
        ui.at_line = next;
    }
    return false;
}

/* ui_find3: F3: the text found again, from past the cursor; it wraps. */
static void find_again(void)
{
    if (!search())
        dlog_ok(s_nomatch);
}

/* ui_fdef: Find's text to start from: the selection on one line, else the word at the cursor or
   just before it, else the last. */
static void find_default(void)
{
    if (uimn.focus)
        return;
    const uint8_t *t = uimn.edline.text;
    uint16_t to = uimn.edline.col, len = uimn.edline.len, from;
    if (uimn.ed.sel_on) {
        if (uimn.ed.sel_line != uimn.ed.line)
            return;
        from = uimn.ed.sel_col;
        if (from > to) {
            uint16_t x = from;
            from = to;
            to = x;
        }
    } else {
        if (!(to < len && wordc(t[to]))) {
            if (!to)
                return;
            to--;
            if (to >= len || !wordc(t[to]))
                return;
        }
        from = to;
        while (from && wordc(t[from - 1]))
            from--;
        while (to < len && wordc(t[to]))
            to++;
    }
    uint16_t n = to - from;
    if (!n || n > FLDMAX)
        return;
    uimn.find_what.len = uimn.find_what.col = n;
    memcpy(uimn.find_what.text, t + from, n);
}

/* ui_find: Search, Find. */
static void find(void)
{
    find_default();
    if (dlog_find())
        find_again();
}

/* ui_change: Search, Change: Change All, or each one asked. */
static void change(void)
{
    find_default();
    uint8_t r = dlog_change();
    if (r > 1)
        return;
    ui.change_all = r;
    ui.changed_one = false;
    ui.wrap = false;
    edit_commit();
    uimn.ed.sel_on = false;
    LineBuf *b = &uimn.edline;
    if (ui.change_all) { /* from the start, the cursor back after */
        ui.start_line = uimn.ed.line;
        ui.start_col = b->col;
        edit_load(0);
        b->col = 0xFFFF;
    }
    while (search()) {
        ui.changed_one = true;
        if (!ui.change_all) {
            uint8_t c = dlog_conf();
            if (c == 1)
                continue;
            if (c > 1) { /* Cancel: no word more (uisearch.c) */
                ui.wrap = true;
                return;
            }
        }
        edit_lb_cut(b, uimn.ed.sel_col, b->col);
        edit_lb_insert(b, uimn.change_to.text, uimn.change_to.len);
        uimn.ed.sel_on = false;
        b->col--;
        edit_commit();
    }
    ui.wrap = true;
    uimn.ed.sel_on = false;
    if (ui.change_all) {
        edit_commit();
        uint32_t line = ui.start_line;
        if (line > uimn.text.n)
            line = uimn.text.n;
        edit_load(line);
        b->col = ui.start_col;
    }
    dlog_ok(ui.changed_one ? s_chdone : s_nomatch);
}

/* ---- File */

/* ui_save: File, Save: to its name, else Save As. false: not saved. */
static bool save_as(void);

static bool write_file(void)
{
    if (text_write(uimn.file) != PFIL_OK) {
        dlog_ok(s_access);
        return false;
    }
    uimn.text.unsaved = false;
    return true;
}

static bool save(void)
{
    edit_commit();
    if (!uimn.file[0])
        return save_as();
    return write_file();
}

static bool same_name(const char *a, const char *b)
{
    for (;; a++, b++) {
        if (uimn_upper((uint8_t)*a) != uimn_upper((uint8_t)*b))
            return false;
        if (!*a)
            return true;
    }
}

/* ui_saveas: File, Save As. false: not saved. */
static bool save_as(void)
{
    edit_commit();
    for (;;) {
        if (!dlog_saveas())
            return false;
        if (!same_name(uimn.chosen, uimn.file)) { /* another file there: Overwrite? */
            uint8_t attr;
            if (pfil_getattr(uimn.chosen, &attr) == PFIL_OK) {
                uint8_t r = dlog_ync(s_overw);
                if (r == 1)
                    continue;
                if (r > 1)
                    return false;
            }
        }
        break;
    }
    memcpy(uimn.file, uimn.chosen, sizeof uimn.file);
    return write_file();
}

/* ui_asksave: a changed text: save it first? Yes saves, No goes on. false: cancelled. */
static bool ask_save(void)
{
    edit_commit();
    if (!uimn.text.unsaved)
        return true;
    uint8_t r = dlog_ync(s_notsaved);
    if (r == 1)
        return true;
    if (r > 1)
        return false;
    return save();
}

/* ui_reset: the editor at the text's start: a new or loaded text. */
static void reset(void)
{
    uimn.ed.top = 0;
    uimn.ed.left = 0;
    uimn.edline.col = 0;
    uimn.ed.sel_on = false;
    uimn.focus = FOC_EDIT;
    uimn.text.unsaved = false;
    uimn.text.changed = true;
    dpan_close();
    view_reset();
    edit_load(0);
}

/* ui_new: File, New. */
static void file_new(void)
{
    if (!ask_save())
        return;
    text_init();
    uimn.file[0] = 0;
    reset();
}

/* ui_load: the file uimn.chosen into the text; not found: a new one of that name, from the
   command line, else a box. */
static void load(void)
{
    if (text_read(uimn.chosen) != PFIL_OK) {
        if (!ui.load_new) {
            dlog_ok(s_notfound);
            return;
        }
        text_init();
    }
    memcpy(uimn.file, uimn.chosen, sizeof uimn.file);
    reset();
}

/* ui_open: File, Open. */
static void file_open(void)
{
    if (ask_save() && dlog_open())
        load();
}

static void file_save(void) { save(); }
static void file_saveas(void) { save_as(); }

/* ui_exit: File, Exit: back to DOS. */
static void file_exit(void)
{
    if (!ask_save())
        return;
    pscr_mode(3);
    cons_exit(0);
}

/* ui_view: F4: the output page until a key. */
static void view_output(void)
{
    page_out();
    uimn_getkey();
    page_in();
}

/* ui_syntax: Options, Syntax Checking. */
static void syntax(void) { uimn.text.check = !uimn.text.check; }

/* ui_about: Help, About. */
static void about(void) { dlog_ok(s_about); }

/* ---- running */

/* ui_sync: the program entered again when the text changed. Errors: as repl_relink, at its
   line. */
static void sync(void)
{
    edit_commit();
    if (!uimn.text.changed)
        return;
    repl_relink();
    lmap_build();
    uimn.text.changed = false;
}

void uimn_run(void)
{
    sync();
    udbg_prep();
    page_clear();
    ui.run_wait = true;
    memcpy(cons.inbuf, "RUN", 3);
    repl_enter(3);
    repl_run();
}

bool uimn_stopped(void) { return uimn.stopped && !uimn.text.changed; }

bool uimn_cancont(void)
{
    edit_commit();
    return uimn_stopped();
}

void uimn_cont(void)
{
    if (!uimn_cancont())
        uimn_run();
    udbg_prep();
    page_out();
    ui.run_wait = true;
    repl_cont();
}

/* ui_exec: Enter in the immediate window: its line run on the output page. */
static void exec_line(void)
{
    sync();
    uint16_t len = edit_imm_next();
    if (!len)
        return;
    page_out();
    pscr_where(1, &ui.crow, &ui.ccol);
    ui.run_wait = false;
    if (repl_enter(len) == ENTER_DIRECT)
        repl_run();
    page_in();
}

void uimn_ready(void)
{
    cons_flush();
    exec.stop = 0;
    uint8_t row, col;
    pscr_where(1, &row, &col);
    if (ui.run_wait || row != ui.crow || col != ui.ccol) {
        cons_fresh();
        pscr_where(1, &row, &col); /* output goes on here after the key */
        pscr_cursor(1, 24, 0);
        for (const char *s = s_anykey; *s; s++)
            cons_putc((uint8_t)*s);
        cons_flush();
        cons.col = 0;
        pkey_read();
        pscr_cursor(1, 24, 0); /* the prompt off */
        pscr_fill(1, 24, 0, 0x0720, 80);
        pscr_cursor(1, row, col);
    }
    page_in();
    loop();
}

/* ---- the cursor at a line, an error */

void uimn_selword(uint16_t col)
{
    uimn.ed.sel_on = false;
    LineBuf *b = &uimn.edline;
    b->col = col;
    uint16_t end = col;
    while (end < b->len && wordc(b->text[end]))
        end++;
    if (end == col)
        return;
    uimn.ed.sel_col = end;
    uimn.ed.sel_line = uimn.ed.line;
    uimn.ed.sel_on = true;
}

/* ui_atline: the cursor at line's first word, the word selected, as QBasic shows an error's
   statement. */
static void at_line(uint32_t line)
{
    uimn.focus = FOC_EDIT;
    edit_load(line);
    const LineBuf *b = &uimn.edline;
    uint16_t col = 0;
    while (col < b->len && b->text[col] == ' ')
        col++;
    uimn_selword(col);
}

/* ui_dxrun: a compile error met running, at the cursor's line: its words in uimn.errmsg, the
   word it is at selected. */
static void run_error_words(void)
{
    if (!ui.at_error || !exec.opip || *exec.opip != O_ERR)
        return;
    LineBuf *b = &uimn.edline;
    b->text[b->len] = 0;
    uimn_selword(edit_dxerr(b->text, (const ErrOp *)exec.opip));
}

void uimn_error(uint8_t code)
{
    ui.ecode = code;
    ui.at_error = false;
    if (code != E_BRK) {
        exec.cstk = allo.cs_top;
        exec.cont_line = 0;
        dpan_close();
    }
    cons_flush();
    cons.col = 0;
    exec.stop = 0;
    page_in();
    repl.loading = false; /* repl_relink's, the error ended it */
    prog.add_flags = 0;
    prog.add_at = 0;
    if (exec.line != DLINE) {
        lmap_build(); /* the include's lines are no line */
        uint32_t line;
        if (lmap_of(exec.line, &line)) {
            ui.at_error = true;
            at_line(view_goline(line));
        }
    }
    if (ui.ecode == E_BRK) {
        udbg_stop();
    } else {
        edit_emsg(ui.ecode);
        run_error_words();
        dlog_err(uimn.errmsg);
    }
    loop();
}

/* ---- the commands */

static void act_theme(uint8_t action) { them_pick(action); }

void uimn_act(uint8_t action)
{
    switch (action) {
    case ACT_NEW: file_new(); break;
    case ACT_OPEN: file_open(); break;
    case ACT_SAVE: file_save(); break;
    case ACT_SAVEAS: file_saveas(); break;
    case ACT_EXIT: file_exit(); break;
    case ACT_CUT: edit_cut(); break;
    case ACT_COPY: edit_copy(); break;
    case ACT_PASTE: edit_paste(); break;
    case ACT_CLEAR: edit_clear(); break;
    case ACT_OUT: view_output(); break;
    case ACT_FIND: find(); break;
    case ACT_FIND3: find_again(); break;
    case ACT_CHANGE: change(); break;
    case ACT_START: uimn_run();
    case ACT_CONT: uimn_cont();
    case ACT_SYNTAX: syntax(); break;
    case ACT_ABOUT: about(); break;
    case ACT_STEP: udbg_step();
    case ACT_PSTEP: udbg_pstep();
    case ACT_TRACE: udbg_tron(); break;
    case ACT_BP: udbg_bp(); break;
    case ACT_BPCLR: udbg_bpclear(); break;
    case ACT_SETNX: udbg_setnext(); break;
    case ACT_RESTART: udbg_restart();
    case ACT_SPLIT: wind_split(); break;
    case ACT_SUBS: view_subs(); break;
    case ACT_NSUB: view_newsub(); break;
    case ACT_NFN: view_newfn(); break;
    case ACT_THQB: them_qbasic(); break;
    case ACT_THDK: them_dark(); break;
    case ACT_SYNHL: synt_toggle(); break;
    case ACT_VSPLIT: wind_vsplit(); break;
    case ACT_SOUT: udbg_sout();
    default:
        if (action >= ACT_TH2 && action < ACT_SOUT)
            act_theme(action);
        break;
    }
}

void uimn_key(uint16_t key)
{
#if AC
    uimn.help = nullptr; /* a completion's form, shown till now */
#endif
    if (key == KEY_ALT) {
        menu_run(0, false);
        return;
    }
    if (key == KEY_MOUSE) {
        mous_main();
        return;
    }
    if (key == 0x260C) { /* Ctrl+L: Repeat Last Find */
        find_again();
        return;
    }
    if (!(key & 0xFF)) {
        switch (key >> 8) {
        case 0x41: udbg_pstep();
        case 0x64: udbg_until(); return;  /* Ctrl+F7 */
        case 0x42: udbg_step();
        case 0x5B: udbg_sout();           /* Shift+F8 */
        case 0x43: udbg_bp(); return;
        case 0x44: udbg_pstep();
        case 0x3E: view_output(); return;
        case 0x3F: uimn_cont();
        case 0x58: uimn_run();
        case 0x40: next_window(); return;
        case 0x3D: find_again(); return;
        case 0x3B:
        case 0x54: nohelp(); return;      /* F1, Shift+F1 */
        case 0x59: wind_prev(); return;   /* Shift+F6: the window before */
        case 0x83:
        case 0x4E: wind_grow(); return;   /* Alt+=, Alt+grey +: a row more */
        case 0x82:
        case 0x4A: wind_shrink(); return; /* Alt+-, Alt+grey -: a row less */
        case 0x67: wind_max(); return;    /* Ctrl+F10 */
        case 0x3C: view_subs(); return;   /* F2, Shift+F2, Ctrl+F2: the views */
        case 0x55: view_next(); return;
        case 0x5F: view_prev(); return;
        default:
            if (menu_alt((uint8_t)(key >> 8)))
                return;
        }
    }
    if (uimn.focus == FOC_IMM)
        imm_key(key);
    else if (uimn.focus == FOC_PANE)
        dpan_key(key);
    else
        edit_key(key);
}

/* ui_loop: draw, a key, its command; again. */
static void loop(void)
{
    for (;;) {
#if !AC
        uimn.help = nullptr; /* else uimn_key: a completion shows it */
#endif
        draw_all();
        uimn.mouse.hover = true;
        uint16_t key = uimn_getkey();
        uimn.mouse.hover = false;
        if (key != KEY_MOUSE)
            uimn.tip_mouse = false; /* a key: the tooltip by the cursor */
        uimn_key(key);
    }
}

/* ---- the start */

/* hl_load: BASIC.HLP, from the program's own directory. */
static void load_help(void)
{
    char name[160];
    uint32_t n = psys_self(name, sizeof name - 10);
    if (!n)
        return;
    uint32_t dir = 0; /* past the last '\' */
    for (uint32_t i = 0; i < n; i++)
        if (name[i] == '\\')
            dir = i + 1;
    memcpy(name + dir, "BASIC.HLP", 10);
    PfilHandle h;
    if (pfil_open(name, PFIL_READ, &h) != PFIL_OK)
        return;
    uint32_t got;
    if (pfil_read(h, uimn.help_text, HLPSZ, &got) == PFIL_OK)
        uimn.help_len = (uint16_t)got;
    pfil_close(h);
}

void uimn_start(void)
{
    if (!cons.stdin_dev || !pfil_isdev(PFIL_STDOUT))
        return;
    uimn.on = true;
    uimn.edline.text = uimn.edline_text;
    uimn.immline.text = uimn.immline_text;
    uimn.fname.text = uimn.fname_text;
    uimn.find_what.text = uimn.find_what_text;
    uimn.change_to.text = uimn.change_to_text;
    uimn.file[0] = 0;
    uimn.imm.n = 1;
    uimn.text.check = true;
    uimn.ed.insert = true;
    ui.wrap = true;
    udbg_init();
    wind_init();
    mous_init();
    load_help();
    text_init();
    view_reset();
    edit_load(0);
    edit_imm_load();
    pscr_mode(3);
    them_qbasic();
    char tail[128]; /* a file named on the command line */
    psys_cmdline(tail, sizeof tail);
    uint16_t n = 0;
    while ((uint8_t)tail[n] > ' ' && n < FLDMAX) {
        uimn.fname.text[n] = (uint8_t)tail[n];
        n++;
    }
    if (n) {
        uimn.fname.len = n;
        dlog_fileok(DLOG_PRESS);
        ui.load_new = true;
        load();
        ui.load_new = false;
    } else {
        dlog_msg(s_welcome, bt_welcome);
    }
    loop();
}
