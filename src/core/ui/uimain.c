/* The editor: its start, the key loop, the menu commands and running the program.
 *
 * # How it works
 * Page 0 is the editor's, page 1 the program's output. uimn_start sets the editor up and enters the
 * key loop, which never returns.
 *
 * A run enters the text again when it changed (repl_relink); an error, BREAK or STOP comes back to
 * the editor at its line. The editor runs only when standard input and output are both the console.
 *
 * Open reads the program and the modules it imports (repl_open), then checks again the lines kept
 * as typed, which may call them. Save writes the program and each module changed (text_unsaved);
 * Save As names the program.
 *
 * Each key's command runs under repl_call, and a run under its own: an error or a run's end comes
 * back to its caller as a Status, the C stack below it unwound.
 *
 * # asm structure (retired/asm32/ui/ui.asm)
 * | asm | here | |
 * |---|---|---|
 * | ui_start | uimn_start | MIRROR |
 * | ui_ready, ui_err | ended, erred | MIRROR; static, returning |
 * | ui_loop | loop | MIRROR; noreturn |
 * | ui_getkey, ui_key, ui_act | uimn_getkey, uimn_key, uimn_act | MIRROR |
 * | ui_stopped, ui_cancont | uimn_stopped, uimn_cancont | MIRROR; D1 |
 * | ui_runp, ui_contp | uimn_run, uimn_cont | MIRROR; returning |
 * | ui_selw, ui_dec | uimn_selword, uimn_dec | MIRROR |
 * | ui_lin | psys_self (plat/psys.h) | MOVED |
 * | hl_load, ui_win, ui_nohelp, im_key, ed_key, ui_syntax, ui_about, ui_asksave, ui_reset, ui_new, ui_open, ui_load, ui_save, ui_saveas, ui_exit, ui_view, ui_in, ui_out, ui_outclr, ui_sync, ui_atline, ui_exec, ui_dxrun, ui_find3, ui_search, ui_change | statics | MIRROR |
 * | ui_find | findbar.c's fbar_open | MOVED; a bar, not a box |
 * | ui_match, ui_wordc, ui_fdef | search.c's srch_match, srch_wordc, srch_default | MOVED |
 * | ed_ws, ed_wsq, ed_moves, ui_acts | static tables | MIRROR |
 * | u_kedit, u_krun, u_kimm | uimn_keys_edit, uimn_keys_run, uimn_keys_imm (StatusKeys) | MIRROR |
 * | u_ok, u_cancel, u_bhelp, u_untl, u_mbar, u_dlg | uimn_ok, uimn_cancel, uimn_helpbtn, uimn_untitled, uimn_menu_help, uimn_dlog_help | MIRROR |
 * | u_imm, u_f1, u_f1s | draw.c's statics | MOVED |
 * | u_open .. u_star, bt_ok, bt_ync | dialog.c's statics | MOVED |
 * | u_oom, u_range, u_order | - | dropped, unused |
 * | u_notfound, u_overw, u_nomatch, u_chdone, u_notsaved, u_access, u_wordp, u_nohelp, u_about, u_welcome, u_anykey, u_esc, bt_wel | statics | MIRROR |
 * | g_hlpb | uimn.help_text | MOVED (ui.h) |
 * | g_ui.lp.ideon, ui_foc, ui_help, ui_kf | uimn.on, focus, help, keyflags | MOVED (ui.h) |
 * | g_ui.lp.alt_t, ui_rf, ui_cpos, ui_rpos, ui_ecode, ui_eln, ul_new | statics | MIRROR; ui_wrap dropped, F3 always wraps and Change never |
 * | g_ui.fd.uc_all, uc_f, uc_sl, uc_sc, us_l, us_c | statics | MIRROR |
 * | g_ui.bf.uc_line, sy_buf, ui_fn, ui_ebuf | uimn.line, build, file, errmsg | MOVED (ui.h) |
 * | VIDEO | the dos32 screen platform's | MOVED |
 *
 * - D1. A flag result is a bool, true the success: uimn_stopped true when stopped, uimn_cancont
 *   true when it can.
 */
#include "ui/uimain.h"
#include "session.h"
#include "io/console.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/module.h"
#include "mem/program.h"
#include "plat/pfil.h"
#include "plat/pkey.h"
#include "plat/pmem.h"
#include "plat/pscr.h"
#include "plat/psys.h"
#include "qbrt/qbrt.h"
#include "ui/complete.h"
#include "ui/dialog.h"
#include "ui/debugpane.h"
#include "ui/draw.h"
#include "ui/closers.h"
#include "ui/edit.h"
#include "ui/errors.h"
#include "ui/findbar.h"
#include "ui/goto.h"
#include "ui/linemap.h"
#include "ui/lines.h"
#include "ui/help.h"
#include "ui/jumps.h"
#include "ui/menu.h"
#include "ui/mouse.h"
#include "ui/pages.h"
#include "ui/pairs.h"
#include "ui/screen.h"
#include "ui/search.h"
#include "ui/highlight.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/undo.h"
#include "ui/debugui.h"
#include "ui/definition.h"
#include "ui/view.h"
#include "ui/window.h"
#include "vm/exec.h"

Uimn uimn; /* zeroed: its line buffers' texts are set by uimn_start */

/* ui.alt. */
enum : uint8_t { ALT_NONE = 0, ALT_DOWN = 1, ALT_HELD = 2 };

#define CTRL(c) ((c) & 0x1F)         /* Ctrl+letter's character */
#define FKEY(f1, n) ((f1) + (n) - 1) /* Fn's scan code, from SCAN_F1, SCAN_SHIFT_F1, .. */

static struct {
    uint8_t alt;         /* alt_t: ALT_DOWN Alt went down alone, ALT_HELD it was down at the call */
    bool run_wait;       /* ui_rf: a run: always wait for a key at its end */
    uint16_t key;        /* the key take_key gives its command */
    uint8_t crow, ccol;  /* ui_cpos: the output's cursor before an immediate line */
    uint8_t ecode;       /* ui_ecode */
    bool at_error;       /* ui_eln: the cursor is at the error's line */
    bool load_new;       /* ul_new: a file not found is a new one (the command line's) */
    bool change_all;     /* uc_all */
    bool changed_one;    /* uc_f: Change found one */
    uint32_t start_line; /* uc_sl, uc_sc: Change All's cursor, back after */
    uint16_t start_col;
    uint32_t at_line; /* us_l, us_c: the search's place */
    uint16_t at_col;
} ui;

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
static const char s_dialect[] = "The new program's dialect:";
static const char s_access[] = "Path/File access error";
static const char s_about[] = "QuickrBasic||A full-screen editor";
static const char s_welcome[] = "Welcome to QuickrBasic||"
                                "Lines may have numbers or not; GOTO and|"
                                "GOSUB need them. F5 runs the program.";
static const char s_anykey[] = "Press any key to continue";
static const MsgButton bt_welcome[] = {{"Press Esc to clear this dialog box", 0}, {}};

[[noreturn]]
static void loop(Session *S);
static void exec_line(Session *S);
static void ran(Session *S, Status st);

/* ui_win: F6. */
static void next_window(Session *const S) { wind_next(S); }

/* QBasic's status lines (qbasmsgs.txt) less the keys we lack; their keys for the mouse. */
const StatusKeys uimn_keys_edit = {
    "<Shift+F1=Help> <F6=Window> <F5=Run> <F8=Step>",
    {{15, help_using}, {27, next_window}, {36, uimn_cont}, {46, udbg_step}},
};
const StatusKeys uimn_keys_run = {
    "<F5=Continue> <F7=Over> <F8=Step> <Shift+F8=Out> <F9=Bkpt>",
    {{13, uimn_cont}, {23, udbg_pstep}, {33, udbg_step}, {48, udbg_sout}, {58, udbg_bp}},
};
const StatusKeys uimn_keys_imm = {
    "<Shift+F1=Help> <F6=Window> <Enter=Execute Line>",
    {{15, help_using}, {27, next_window}, {48, exec_line}},
};

char *uimn_dec(char *const dst, uint16_t v)
{
    char digits[5];
    int n = 0;

    do {
        digits[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);

    for (int i = 0; i < n; i++)
        dst[i] = digits[n - 1 - i];

    return dst + n;
}

/* ---- keys */

/* k as a key: an extended key's E0h character 0. */
static uint16_t key_of(const PkeyKey k)
{
    const uint8_t ch = k.ch == CH_E0 && k.scan ? 0 : k.ch;
    return (uint16_t)(k.scan << 8 | ch);
}

/* The bar's letters lit while Alt is held, as QBasic's; shown now when no menu or dialog is up. */
static void bar_letters(const bool on)
{
    const Widget *const sc = &uimn.tree.screen;
    menu_letters(on);

    for (uint8_t i = 0; i < sc->nchildren; i++) {
        if (sc->children[i]->modal && sc->children[i]->visible)
            return;
    }

    menu_bar();
    scrn_flip();
}

/* A key, the mouse, or Alt let go after going down alone. */
static uint16_t wait_key(void)
{
    for (;;) {
        if (pkey_ready())
            return key_of(pkey_read());
        if (mous_poll())
            return KEY_MOUSE;
        if (pkey_shift() & PKEY_ALT) {
            if (ui.alt == ALT_NONE) {
                ui.alt = ALT_DOWN;
                bar_letters(true);
            }

            continue;
        }

        const bool tap = ui.alt == ALT_DOWN;

        if (ui.alt != ALT_NONE)
            bar_letters(false);

        ui.alt = ALT_NONE;

        if (tap)
            return KEY_ALT;
    }
}

uint16_t uimn_getkey(void)
{
    scrn_flip();
    mous_show();
    ui.alt = pkey_shift() & PKEY_ALT ? ALT_HELD : ALT_NONE;
    const uint16_t key = wait_key();
    uimn.keyflags = pkey_shift();
    mous_hide();
    return key;
}

/* im_key: a key in the immediate window. */
static bool imm_key(Widget *const w, Session *const S, const uint16_t key)
{
    const uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);

    if (ch == CH_ENTER) {
        exec_line(S);
        return true;
    }

    if (!ch) {
        if (scan == SCAN_UP) {
            edit_imm_up();
            return true;
        }

        if (scan == SCAN_DOWN) {
            edit_imm_down();
            return true;
        }
    }

    if (ch >= ' ' && uimn.immline.len >= ED_COLS - 1)
        return true;

    return dlog_ledit(&uimn.immline, key);
}

/* Ctrl+A to Ctrl+Z as WordStar's: the scan code of the key they stand for (COW edittbls.inc). */
static const uint8_t wordstar[26] = {
    [CTRL('A') - 1] = SCAN_CTRL_LEFT,
    [CTRL('C') - 1] = SCAN_PGDN,
    [CTRL('D') - 1] = SCAN_RIGHT,
    [CTRL('E') - 1] = SCAN_UP,
    [CTRL('F') - 1] = SCAN_CTRL_RIGHT,
    [CTRL('G') - 1] = SCAN_DEL,
    [CTRL('R') - 1] = SCAN_PGUP,
    [CTRL('S') - 1] = SCAN_LEFT,
    [CTRL('V') - 1] = SCAN_INS,
    [CTRL('X') - 1] = SCAN_DOWN,
};
/* Ctrl+Q's second key: the letter's code, the scan code (0 erases to the line's end). */
static const struct {
    uint8_t ch, scan;
} wordstar_q[] = {
    {CTRL('S'), SCAN_HOME},
    {CTRL('D'), SCAN_END},
    {CTRL('R'), SCAN_CTRL_HOME},
    {CTRL('C'), SCAN_CTRL_END},
    {CTRL('Y'), 0},
};

static void lb_left(Session *const S) { edit_lb_left(&uimn.edline); }
static void lb_right(Session *const S) { edit_lb_right(&uimn.edline); }
static void lb_home(Session *const S) { edit_lb_home(&uimn.edline); }
static void lb_end(Session *const S) { edit_lb_end(&uimn.edline); }
static void lb_wleft(Session *const S) { edit_lb_wleft(&uimn.edline); }
static void lb_wright(Session *const S) { edit_lb_wright(&uimn.edline); }

/* The keys that move the cursor. */
static const struct {
    uint8_t scan;
    void (*run)(Session *S);
} moves[] = {
    {SCAN_UP, edit_up},
    {SCAN_DOWN, edit_down},
    {SCAN_LEFT, lb_left},
    {SCAN_RIGHT, lb_right},
    {SCAN_HOME, lb_home},
    {SCAN_END, lb_end},
    {SCAN_PGUP, edit_pgup},
    {SCAN_PGDN, edit_pgdn},
    {SCAN_CTRL_HOME, edit_first},
    {SCAN_CTRL_END, edit_last},
    {SCAN_CTRL_LEFT, lb_wleft},
    {SCAN_CTRL_RIGHT, lb_wright},
};

static bool edit_key(Widget *w, Session *S, uint16_t key);

/* An extended key (character 0) in the program window; false: not one of its keys. */
static bool edit_scan(Session *const S, const uint8_t scan)
{
    const bool shift = uimn.keyflags & PKEY_SHIFT;

    for (uint32_t i = 0; i < countof(moves); i++) {
        if (moves[i].scan != scan)
            continue;
        if (shift)
            edit_sel_mark();
        else
            uimn.ed.sel_on = false;

        moves[i].run(S);
        return true;
    }

    if (scan == SCAN_DEL) {
        if (shift) {
            edit_cut(S);
        } else if (edit_sel()) {
            edit_clear(S);
        } else {
            uimn.ed.sel_on = false;
            edit_del(S);
        }
    } else if (scan == SCAN_INS) {
        if (shift)
            edit_paste(S);
        else
            uimn.ed.insert = !uimn.ed.insert;
    } else if (scan == SCAN_CTRL_INS) {
        edit_copy(S);
    } else if (scan == SCAN_CTRL_DEL) { /* ours */
        if (edit_sel())
            edit_clear(S);
        else
            edit_del_word(S);
    } else if (scan == SCAN_BACKSPACE && uimn.keyflags & PKEY_ALT) { /* QBasic's Undo */
        undo_undo(S);
    } else if (scan == SCAN_B && uimn.keyflags & PKEY_CTRL) { /* Ctrl+Alt+B, ours */
        defn_goto(S, true);
    } else if (scan == SCAN_ALT_LEFT) { /* ours: back, forward, past jumps */
        jumps_back(S);
    } else if (scan == SCAN_ALT_RIGHT) {
        jumps_forward(S);
    } else if (scan == SCAN_ALT_UP || scan == SCAN_ALT_DOWN) { /* ours; with Shift, a copy */
        uimn.ed.sel_on = uimn.ed.sel_on && edit_sel() == SEL_LINES;

        if (shift)
            lines_copy(S, scan == SCAN_ALT_DOWN);
        else
            lines_move(S, scan == SCAN_ALT_DOWN);
    } else
        return false;

    return true;
}

/* Ctrl+Q's second key, ch. */
static void edit_ctrlq(Widget *const w, Session *const S, const uint8_t ch)
{
    LineBuf *const b = &uimn.edline;
    const uint8_t letter = CTRL(ch);

    for (uint32_t i = 0; i < countof(wordstar_q); i++) {
        if (wordstar_q[i].ch != letter)
            continue;
        if (wordstar_q[i].scan) {
            edit_key(w, S, (uint16_t)(wordstar_q[i].scan << 8));
        } else if (!edit_refused(S)) { /* Ctrl+Q,Y: to the line's end out */
            uimn.ed.sel_on = false;
            edit_lb_cut(b, b->col, b->len);
        }

        return;
    }
}

/* A control character ch the window uses; false: not one. */
static bool edit_control(Session *const S, const uint8_t ch)
{
    switch (ch) {
    case CH_ENTER:
        uimn.ed.sel_on = false;
        edit_enter(S);
        return true;

    case CH_BACKSPACE:
        if (edit_sel())
            edit_clear(S);
        else if (!clos_bs(S))
            edit_bs(S);

        return true;

    case CH_TAB:
        uimn.ed.sel_on = false;

        if (edit_refused(S))
            return true;
#if AC

        if (cmpl_tab(S))
            return true;
#endif

        edit_tab();
        return true;

    case CTRL('Y'):
        uimn.ed.sel_on = false;
        edit_ctrly(S);
        return true;

    case CTRL('B'): /* Go to Definition, ours */
        defn_goto(S, false);
        return true;

    case CH_CTRL_BACKSPACE: /* ours */
        if (edit_sel())
            edit_clear(S);
        else
            edit_bs_word(S);

        return true;

    case CTRL('P'): /* ours: Go To */
        goto_run(S);
        return true;

    case CTRL(']'): /* ours: the matching bracket, the block's next keyword */
        pairs_jump(S);
        return true;

    case CH_CTRL_MINUS: /* ours: the lines commented out, or back */
        lines_comment(S);
        return true;

    case CTRL('Z'): /* Undo; with Shift, Redo */
        if (uimn.keyflags & PKEY_SHIFT)
            undo_redo(S);
        else
            undo_undo(S);

        return true;

    case CH_ESC:
        uimn.ed.sel_on = false;
        return true;

    default:
        return false;
    }
}

/* ed_key: a key in the program window: a shifted move selects; false: not the window's. */
static bool edit_key(Widget *const w, Session *const S, const uint16_t key)
{
    const uint8_t ch = (uint8_t)key;

    if (!ch)
        return edit_scan(S, (uint8_t)(key >> 8));

    if (uimn.ed.prefix) {
        uimn.ed.prefix = 0;
        edit_ctrlq(w, S, ch);
        return true;
    }

    if (ch == CTRL('Q')) { /* a second key follows */
        uimn.ed.prefix = ch;
        return true;
    }

    if (ch <= CTRL('Z') && wordstar[ch - 1])
        return edit_key(w, S, (uint16_t)(wordstar[ch - 1] << 8));
    if (edit_control(S, ch))
        return true;
    if (ch < ' ')
        return false;
    if (edit_refused(S))
        return true;
    if (edit_sel() == SEL_PIECE)
        edit_clear(S);

    uimn.ed.sel_on = false;

    if (!clos_type(S, ch))
        edit_lb_type(&uimn.edline, ch, LNMAX);
#if AC
    cmpl_typed(S);
#endif
    return true;
}

/* ---- Search */

/* ui_search: Change's: Find What from past the cursor to the view's end: found, it is selected. */
static bool search(Session *const S)
{
    const uint16_t len = uimn.find_what.len;

    if (!len)
        return false;

    edit_commit(S);
    ui.at_line = uimn.ed.line;
    ui.at_col = uimn.edline.col + 1;

    for (uint32_t n = uimn.text.n + 1; n; n--) { /* every line, this one twice */
        const uint16_t got = (uint16_t)text_get(S, ui.at_line, &uimn.line);

        if (got >= len) {
            const uint16_t last = got - len;

            for (uint16_t col = ui.at_col; col <= last; col++) {
                if (!srch_match(uimn.line.text, got, col))
                    continue;

                edit_load(S, ui.at_line);
                uimn.ed.sel_col = col;
                uimn.ed.sel_line = ui.at_line;
                uimn.ed.sel_on = true;
                uimn.edline.col = col + len;
                return true;
            }
        }

        ui.at_col = 0;
        uint32_t next = ui.at_line + 1;

        if (next >= uimn.text.n)
            return false;

        ui.at_line = next;
    }

    return false;
}

/* ui_find3: F3: the text found again, from past the cursor, or with back before it; through
   every view, round. */
static void find_again(Session *const S, const bool back)
{
    edit_commit(S);
    SrchAt at = srch_from(back);

    if (srch_find(S, &at, back)) {
        jumps_push();
        srch_show(S, at);
    } else
        dlog_ok(S, s_nomatch);
}

/* ui_change: Search, Change: Change All, or each one asked. */
static void change(Session *const S)
{
    uint16_t from;
    srch_default(&from);

    if (edit_refused(S))
        return;

    const uint8_t r = dlog_change(S);

    if (r > CHANGE_ALL)
        return;

    ui.change_all = r == CHANGE_ALL;
    ui.changed_one = false;
    edit_commit(S);
    uimn.ed.sel_on = false;
    LineBuf *const b = &uimn.edline;

    if (ui.change_all) { /* from the start, the cursor back after */
        ui.start_line = uimn.ed.line;
        ui.start_col = b->col;
        edit_load(S, 0);
        b->col = UINT16_MAX; /* search's from column 0 */
    }

    while (search(S)) {
        ui.changed_one = true;

        if (!ui.change_all) {
            const uint8_t c = dlog_conf(S);

            if (c == CONF_SKIP)
                continue;
            if (c > CONF_SKIP) /* Cancel: no word more (uisearch.c) */
                return;
        }

        edit_lb_cut(b, uimn.ed.sel_col, b->col);
        edit_lb_insert(b, uimn.change_to.text, uimn.change_to.len);
        uimn.ed.sel_on = false;
        b->col--;
        edit_commit(S);
    }

    uimn.ed.sel_on = false;

    if (ui.change_all) {
        edit_commit(S);
        uint32_t line = ui.start_line;

        if (line > uimn.text.n)
            line = uimn.text.n;

        edit_load(S, line);
        b->col = ui.start_col;
    }

    dlog_ok(S, ui.changed_one ? s_chdone : s_nomatch);
}

/* ---- File */

/* ui_save: File, Save: to its name, else Save As. false: not saved. */
static bool save_as(Session *S);

/* Module module's text to its file name. false: not written. */
static bool write_module(Session *const S, const char *const name, const uint32_t module)
{
    if (text_write(S, name, module) != PFIL_OK) {
        dlog_ok(S, s_access);
        return false;
    }

    text_saved(S, module);
    return true;
}

/* The program to uimn.file, each module changed to its own; a native one's never: its file is a
   .qbx, its text the interface in it. */
static bool write_file(Session *const S)
{
    if (!write_module(S, uimn.file, 0))
        return false;

    for (uint32_t m = 1; m < modu_count(&S->modu); m++)
        if (text_unsaved(S, m) && !modu_mod(&S->modu, m)->native &&
            !write_module(S, modu_module_path(&S->modu, m), m))
            return false;

    return true;
}

/* The program named uimn.file: its imports found from its folder. */
static void named(Session *const S)
{
    memcpy(S->repl.file, uimn.file, sizeof uimn.file);
    uimn.text.changed = true;
}

bool uimn_save(Session *const S)
{
    edit_commit(S);

    if (!uimn.file[0])
        return save_as(S);

    return write_file(S);
}

/* ui_saveas: File, Save As. false: not saved. */
static bool save_as(Session *const S)
{
    edit_commit(S);

    for (;;) {
        if (!dlog_saveas(S))
            return false;
        if (same_caseless(uimn.chosen, uimn.file))
            break;

        uint8_t attr;

        if (pfil_getattr(uimn.chosen, &attr) != PFIL_OK)
            break;

        const uint8_t r = dlog_ync(S, s_overw); /* another file there */
        if (r == YNC_YES)
            break;
        if (r > YNC_NO)
            return false;
    }

    memcpy(uimn.file, uimn.chosen, sizeof uimn.file);
    named(S);
    return write_file(S);
}

/* ui_asksave: a changed text: save it first? Yes saves, No goes on. false: cancelled. */
static bool ask_save(Session *const S)
{
    edit_commit(S);

    if (!uimn.text.unsaved)
        return true;

    const uint8_t r = dlog_ync(S, s_notsaved);

    if (r == YNC_NO)
        return true;
    if (r > YNC_NO)
        return false;

    return uimn_save(S);
}

/* ui_reset: the editor at the text's start: a new or loaded text. */
static void reset(Session *const S)
{
    uimn.ed.top = 0;
    uimn.ed.left = 0;
    uimn.edline.col = 0;
    uimn.ed.sel_on = false;
    uimn.tree.workspace.focus = FOC_EDIT;
    uimn.text.changed = true;
    dpan_close();
    view_reset(S);
    text_clean(S);
    edit_load(S, 0);
}

/* ui_new: File, New, in the dialect asked. */
static void file_new(Session *const S)
{
    if (!ask_save(S))
        return;

    MsgButton bt[DIAL_MAX + 2] = {}; /* each dialect's, Cancel, the end */
    const uint32_t n = dial_count();

    for (uint32_t i = 0; i < n; i++)
        bt[i] = (MsgButton){dial_at(i)->button, 10};

    bt[n] = (MsgButton){uimn_cancel, 10};

    const uint8_t choice = dlog_msg(S, s_dialect, bt);

    if (choice >= n)
        return;

    const Dialect *const d = dial_at(choice);
    text_init(S, d);
    uimn.file[0] = 0;
    named(S);
    reset(S);
    undo_clear();
    jumps_clear();

    if (d->text)
        edit_load(S, 1); /* past the dialect's line */
}

/* fn, a load (repl_open, repl_relink), under its own boundary: the modules it read shown
   (view_modules), *read whether there were; its Status. */
static Status loaded(Session *const S, void (*const fn)(Session *S), bool *const read)
{
    const Status st = repl_call(S, fn);
    *read = view_modules(S);
    return st;
}

/* ui_load: the file uimn.chosen into the text; not found: a new one of that name, from the
   command line, else a box. */
static void load(Session *const S)
{
    if (text_read(S, uimn.chosen) != PFIL_OK) {
        if (!ui.load_new) {
            dlog_ok(S, s_notfound);
            return;
        }

        text_init(S, dial_of_file(uimn.chosen));
    }

    memcpy(uimn.file, uimn.chosen, sizeof uimn.file);
    named(S);
    reset(S);
    bool read;
    const Status st = loaded(S, text_dialect(S)->open, &read);
    edit_recheck(S);
    text_clean(S);
    undo_clear();
    jumps_clear();
    fault_check(&S->fault, st);
}

/* ui_open: File, Open. */
static void open_program(Session *const S)
{
    if (ask_save(S) && dlog_open(S))
        load(S);
}

void uimn_open(Session *const S, const char *const name)
{
    const size_t n = strlen(name);
    const size_t fit = n < sizeof uimn.chosen ? n : sizeof uimn.chosen - 1;
    memcpy(uimn.chosen, name, fit);
    uimn.chosen[fit] = 0;
    load(S);
}

static void file_save(Session *const S) { uimn_save(S); }
static void file_saveas(Session *const S) { save_as(S); }

/* ui_exit: File, Exit: back to DOS. */
static void file_exit(Session *const S)
{
    if (!ask_save(S))
        return;

    pscr_text(PSCR_80X25);
    cons_exit(&S->cons, 0);
}

/* ui_view: F4: the output page until a key. */
static void view_output(Session *const S)
{
    upag_out(S);
    uimn_getkey();
    upag_in(S);
}

/* ui_syntax: Options, Syntax Checking. */
static void syntax(void) { uimn.text.check = !uimn.text.check; }

/* ui_about: Help, About. */
static void about(Session *const S) { dlog_ok(S, s_about); }

/* ---- running */

/* The modules read for it shown, their lines ordered, then entered again. */
void uimn_sync(Session *const S)
{
    edit_commit(S);

    if (!uimn.text.changed)
        return;

    bool read;
    fault_check(&S->fault, loaded(S, repl_relink, &read));

    if (read)
        repl_relink(S);

    lmap_build(S);
    uimn.text.changed = false;
}

/* The cursor at text line line's column col, in its view. */
static void at_col(Session *const S, const uint32_t line, const uint16_t col)
{
    uimn.tree.workspace.focus = FOC_EDIT;
    edit_load(S, view_goline(S, line));
    uimn.edline.col = col;
    uimn.ed.sel_on = false;
    edit_ensure();
}

/* A text dialect's program checked and linked for a run (Dialect.link), the modules it read shown:
   true; else the cursor at its first error, its message in a box. */
static bool built(Session *const S)
{
    edit_commit(S);
    DialErr e = {.msg = uimn.errmsg, .cap = sizeof uimn.errmsg};

    const uint32_t errors = text_dialect(S)->link(S, &e);
    view_modules(S);

    if (!errors) {
        uimn.text.changed = false;
        return true;
    }

    uint32_t line;

    if (lmap_of(S, e.rec, &line))
        at_col(S, line, (uint16_t)e.col);

    dlog_err(S, uimn.errmsg);
    return false;
}

void uimn_run(Session *const S)
{
    const Dialect *const d = text_dialect(S);

    if (d->text && !built(S))
        return;
    if (!d->text)
        uimn_sync(S);

    udbg_prep(S);
    upag_clear(S);
    ui.run_wait = true;

    if (d->text) {
        ran(S, repl_call(S, d->go));
        return;
    }

    static const char cmd[] = "RUN";
    memcpy(S->cons.inbuf, cmd, sizeof cmd - 1);
    repl_enter(S, sizeof cmd - 1);
    ran(S, repl_call(S, repl_run));
}

bool uimn_stopped(void) { return uimn.stopped && !uimn.text.changed; }

bool uimn_cancont(Session *const S)
{
    edit_commit(S);
    return uimn_stopped();
}

void uimn_cont(Session *const S)
{
    if (!uimn_cancont(S)) {
        uimn_run(S);
        return;
    }

    udbg_prep(S);
    upag_out(S);
    ui.run_wait = true;
    ran(S, repl_call(S, repl_cont));
}

/* ui_exec: Enter in the immediate window: its line run on the output page. */
/* An immediate line of text dialect d's, len bytes in cons.inbuf: run as its own at the stopped
   statement, else after the program's last line, linked first; an error said in a box, the stop
   kept. */
static void text_line(Session *const S, const Dialect *const d, const uint16_t len)
{
    char line[INMAX + 1];
    memcpy(line, S->cons.inbuf, len);
    line[len] = 0;

    if (!(uimn_stopped() && d->linked(S)) && !built(S))
        return;

    memcpy(S->cons.inbuf, line, len + 1u);
    S->vm.line = DLINE;
    const Status st = repl_call(S, session_text_line);

    if (st != ST_OK) {
        const char *const why = session_errtext(S, (uint8_t)st);
        const size_t n =
            strlen(why) < sizeof uimn.errmsg - 1 ? strlen(why) : sizeof uimn.errmsg - 1;
        memcpy(uimn.errmsg, why, n);
        uimn.errmsg[n] = 0;
        dlog_err(S, uimn.errmsg);
        return;
    }

    upag_out(S);
    qbrt_where(&ui.crow, &ui.ccol);
    ui.run_wait = false;
    ran(S, repl_call(S, repl_direct));
}

static void exec_line(Session *const S)
{
    uimn_sync(S);
    const uint16_t len = edit_imm_next(S);

    if (!len)
        return;

    const Dialect *const d = uimn_stopped() ? session_dialect(S) : text_dialect(S);

    if (d->text) {
        text_line(S, d, len);
        return;
    }

    upag_out(S);
    qbrt_where(&ui.crow, &ui.ccol);
    ui.run_wait = false;

    if (repl_enter(S, len) == ENTER_DIRECT) {
        ran(S, repl_call(S, repl_run));
        return;
    }

    upag_in(S);
}

/* The prompt on the output's last row through the console, in any mode; or its place blanked. */
static void status_prompt(Session *const S, const bool blank)
{
    qbrt_move((uint8_t)(qbrt_rows() - 1), 0);

    for (uint32_t i = 0; s_anykey[i]; i++)
        cons_putc(&S->cons, blank ? ' ' : (uint8_t)s_anykey[i]);

    cons_flush(&S->cons);
}

/* Press any key, the output's cursor back after. */
static void wait_anykey(Session *const S)
{
    cons_fresh(&S->cons);
    uint8_t row, col;
    qbrt_where(&row, &col); /* output goes on here after the key */
    status_prompt(S, false);
    pkey_read();
    status_prompt(S, true);
    qbrt_move(row, col);
    S->cons.col = col;
}

/* ui_ready: the run ended: the output page until a key, unless an immediate line printed
   nothing and left no graphics mode; then the editor. */
static void ended(Session *const S)
{
    cons_flush(&S->cons);
    S->vm.stop = 0;
    uint8_t row, col;
    qbrt_where(&row, &col);

    if (ui.run_wait || row != ui.crow || col != ui.ccol || qbrt_graphics())
        wait_anykey(S);

    upag_in(S);
}

/* ---- the cursor at a line, an error */

void uimn_selword(const uint16_t col)
{
    uimn.ed.sel_on = false;
    LineBuf *const b = &uimn.edline;
    b->col = col;
    uint16_t end = col;

    while (end < b->len && srch_wordc(b->text[end]))
        end++;
    if (end == col)
        return;

    uimn.ed.sel_col = end;
    uimn.ed.sel_line = uimn.ed.line;
    uimn.ed.sel_on = true;
}

/* ui_atline: the cursor at line's first word, the word selected, as QBasic shows an error's
   statement. */
static void at_line(Session *const S, const uint32_t line)
{
    uimn.tree.workspace.focus = FOC_EDIT;
    edit_load(S, line);
    const LineBuf *const b = &uimn.edline;
    uint16_t col = 0;

    while (col < b->len && b->text[col] == ' ')
        col++;

    uimn_selword(col);
}

/* ui_dxrun: a compile error met running, at the cursor's line: its words in uimn.errmsg, the
   word it is at selected. */
static void run_error_words(Session *const S)
{
    if (!ui.at_error || !S->vm.opip || !exec_is_err(S->vm.opip))
        return;

    LineBuf *const b = &uimn.edline;
    edit_lb_need(S, b, b->len)[b->len] = 0;
    uimn_selword(edit_dxerr(S, b->text, (const ErrOp *)S->vm.opip));
}

/* ui_err: an error code (E_*), or E_BRK for BREAK and STOP: the cursor at its line, the message
   in a box, a stop's debug pane. The stacks are reset already (repl_call). */
static void erred(Session *const S, const uint8_t code)
{
    ui.ecode = code;
    ui.at_error = false;

    if (code != E_BRK) {
        session_drop_stop(S);
        dpan_close();
    }

    cons_flush(&S->cons);
    S->cons.col = 0;
    S->vm.stop = 0;
    upag_in(S);
    S->repl.loading = false; /* repl_relink's, the error ended it */
    S->prog.add_flags = 0;
    S->prog.add_at = 0;
    S->prog.add_src = 0;
    S->prog.add_file = 0;

    if (S->vm.line != DLINE) {
        lmap_build(S); /* the include's lines are no line */
        uint32_t line;

        if (lmap_of(S, S->vm.line, &line)) {
            ui.at_error = true;
            at_line(S, view_goline(S, line));
        }
    }

    if (ui.ecode == E_BRK) {
        udbg_stop(S);
    } else {
        edit_emsg(S, ui.ecode);
        run_error_words(S);
        dlog_err(S, uimn.errmsg);
    }
}

/* What ended a run, st: its end, else its error. */
static void ran(Session *const S, const Status st)
{
    if (st == ST_OK)
        ended(S);
    else
        erred(S, (uint8_t)st);
}

/* ---- the commands */

static void act_theme(const uint8_t action) { them_pick(action); }

void uimn_act(Session *const S, const uint8_t action)
{
    switch (action) {
    case ACT_NEW:
        file_new(S);
        break;

    case ACT_OPEN:
        open_program(S);
        break;

    case ACT_SAVE:
        file_save(S);
        break;

    case ACT_SAVEAS:
        file_saveas(S);
        break;

    case ACT_EXIT:
        file_exit(S);
        break;

    case ACT_UNDO:
        undo_undo(S);
        break;

    case ACT_REDO:
        undo_redo(S);
        break;

    case ACT_CUT:
        edit_cut(S);
        break;

    case ACT_COPY:
        edit_copy(S);
        break;

    case ACT_PASTE:
        edit_paste(S);
        break;

    case ACT_CLEAR:
        edit_clear(S);
        break;

    case ACT_OUT:
        view_output(S);
        break;

    case ACT_FIND:
        fbar_open(S);
        break;

    case ACT_FIND3:
        find_again(S, false);
        break;

    case ACT_CHANGE:
        change(S);
        break;

    case ACT_START:
        uimn_run(S);
        break;

    case ACT_CONT:
        uimn_cont(S);
        break;

    case ACT_SYNTAX:
        syntax();
        break;

    case ACT_ABOUT:
        about(S);
        break;

    case ACT_HINDEX:
        help_index(S);
        break;

    case ACT_HCONT:
        help_contents(S);
        break;

    case ACT_HTOPIC:
        help_f1(S);
        break;

    case ACT_HUSING:
        help_using(S);
        break;

    case ACT_STEP:
        udbg_step(S);
        break;

    case ACT_PSTEP:
        udbg_pstep(S);
        break;

    case ACT_TRACE:
        udbg_tron(S);
        break;

    case ACT_BP:
        udbg_bp(S);
        break;

    case ACT_BPCLR:
        udbg_bpclear(S);
        break;

    case ACT_SETNX:
        udbg_setnext(S);
        break;

    case ACT_RESTART:
        udbg_restart(S);
        break;

    case ACT_SPLIT:
        wind_split(S);
        break;

    case ACT_SUBS:
        view_subs(S);
        break;

    case ACT_NSUB:
        view_newsub(S);
        break;

    case ACT_NFN:
        view_newfn(S);
        break;

    case ACT_THQB:
        them_qbasic();
        break;

    case ACT_THDK:
        them_dark();
        break;

    case ACT_SYNHL:
        synt_toggle();
        break;

    case ACT_VSPLIT:
        wind_vsplit(S);
        break;

    case ACT_SOUT:
        udbg_sout(S);
        break;

    default:
        if (action >= ACT_TH2 && action < ACT_SOUT)
            act_theme(action);
        else if (action >= ACT_SIZE && action < ACT_SIZE + uimn.nsizes)
            uimn_size(uimn.sizes[action - ACT_SIZE]);

        break;
    }
}

void uimn_key(Session *const S, const uint16_t key)
{
#if AC
    uimn.help = nullptr; /* a completion's form, shown till now */
#endif

    if (key == KEY_ALT) {
        fbar_close();
        menu_run(S, 0, false);
        return;
    }

    if (key == KEY_MOUSE) {
        if (!wind_drag())
            tree_mouse(&uimn.tree.screen, S, uimn.mouse.row, uimn.mouse.col);

        return;
    }

    tree_key(&uimn.tree.screen, S, key);
}

/* The keys the focused window left: Repeat Last Find, the F-keys, the window keys, a menu's Alt
   letter. */
static bool screen_key(Widget *const w, Session *const S, const uint16_t key)
{
    if (key == (SCAN_L << 8 | CTRL('L'))) { /* Repeat Last Find */
        find_again(S, false);
        return true;
    }

    if ((uint8_t)key)
        return false;

    const uint8_t scan = (uint8_t)(key >> 8);

    switch (scan) {
    case FKEY(SCAN_F1, 7):
        udbg_pstep(S);
        return true;

    case FKEY(SCAN_CTRL_F1, 7):
        udbg_until(S);
        return true;

    case FKEY(SCAN_F1, 8):
        udbg_step(S);
        return true;

    case FKEY(SCAN_SHIFT_F1, 8):
        udbg_sout(S);
        return true;

    case FKEY(SCAN_CTRL_F1, 8): /* ours */
        errs_next(S);
        return true;

    case FKEY(SCAN_F1, 9):
        udbg_bp(S);
        return true;

    case FKEY(SCAN_SHIFT_F1, 9):
        dpan_watch(S);
        return true;

    case FKEY(SCAN_CTRL_F1, 9):
        dpan_break(S);
        return true;

    case FKEY(SCAN_F1, 10):
        udbg_pstep(S);
        return true;

    case FKEY(SCAN_F1, 4):
        view_output(S);
        return true;

    case FKEY(SCAN_F1, 5):
        uimn_cont(S);
        return true;

    case FKEY(SCAN_SHIFT_F1, 5):
        uimn_run(S);
        return true;

    case FKEY(SCAN_F1, 6):
        next_window(S);
        return true;

    case FKEY(SCAN_F1, 3):
        find_again(S, false);
        return true;

    case FKEY(SCAN_SHIFT_F1, 3): /* ours */
        find_again(S, true);
        return true;

    case SCAN_F1:
        help_f1(S);
        return true;

    case SCAN_SHIFT_F1:
        help_using(S);
        return true;

    case SCAN_ALT_F1:
        help_back(S);
        return true;

    case SCAN_CTRL_F1:
        help_next(S, pkey_shift() & PKEY_SHIFT);
        return true;

    case FKEY(SCAN_SHIFT_F1, 6):
        wind_prev(S);
        return true; /* the window before */
    case SCAN_ALT_EQUALS:
    case SCAN_GREY_PLUS:
        wind_grow();
        return true; /* a row more */
    case SCAN_ALT_MINUS:
    case SCAN_GREY_MINUS:
        wind_shrink();
        return true; /* a row less */
    case FKEY(SCAN_CTRL_F1, 10):
        wind_max();
        return true;

    case FKEY(SCAN_F1, 2):
        view_subs(S);
        return true; /* the views */
    case FKEY(SCAN_SHIFT_F1, 2):
        view_next(S);
        return true;

    case FKEY(SCAN_CTRL_F1, 2):
        view_prev(S);
        return true;

    default:
        return menu_alt(S, scan);
    }
}

/* ui.key's command. */
static void take_key(Session *const S) { uimn_key(S, ui.key); }

/* ui_loop: draw, a key, its command, its error shown; again. */
static void loop(Session *const S)
{
    for (;;) {
#if !AC
        uimn.help = nullptr; /* else uimn_key: a completion shows it */
#endif
        draw_all(S);
        uimn.mouse.hover = true;
        const uint16_t key = uimn_getkey();
        uimn.mouse.hover = false;

        if (key != KEY_MOUSE)
            uimn.tip_mouse = false; /* a key: the tooltip by the cursor */

        uimn_command(S, key);
    }
}

void uimn_command(Session *const S, const uint16_t key)
{
    undo_begin(S);
    ui.key = key;
    const Status st = repl_call(S, take_key);
    undo_end(S);

    if (st != ST_OK)
        erred(S, (uint8_t)st);
}

/* ---- the start */

static void menubar_draw(Widget *const w, Session *const S) { menu_bar(); }
static void statusbar_draw(Widget *const w, Session *const S) { draw_status(S); }
static const WidgetOps screen_ops = {.key = screen_key, .mouse = mous_screen};
static const WidgetOps menubar_ops = {.draw = menubar_draw, .mouse = mous_menubar};
static const WidgetOps workspace_ops = {.mouse = mous_workspace};
static const WidgetOps statusbar_ops = {.draw = statusbar_draw, .mouse = mous_statusbar};
static const WidgetOps ed_ops = {
    .draw = wind_ed_draw,
    .key = edit_key,
    .mouse = mous_window,
    .cursor = draw_edit_cursor,
};
static const WidgetOps imm_ops = {
    .draw = wind_imm_draw,
    .key = imm_key,
    .mouse = mous_window,
    .cursor = draw_imm_cursor,
};

/* The screen: the menu bar, the workspace and the status bar. The workspace: the editors, the
   immediate window and the pane, in FOC_* order; the editors: ED1, ED2. wind_layout sets the
   windows' rects. */
static void build_tree(void)
{
    static const struct {
        Widget *w, *parent;
        const WidgetOps *ops;
        Rect rect;
    } tree[] = {
        {&uimn.tree.menubar, &uimn.tree.screen, &menubar_ops, {0, 0, 1, 80}},
        {&uimn.tree.workspace, &uimn.tree.screen, &workspace_ops, {}}, /* sized by uimn_size */
        {&uimn.tree.statusbar, &uimn.tree.screen, &statusbar_ops, {}},
        {&uimn.tree.editors, &uimn.tree.workspace, nullptr, {}},
        {&uimn.tree.imm, &uimn.tree.workspace, &imm_ops, {}},
        {&uimn.tree.pane, &uimn.tree.workspace, nullptr, {}}, /* dpan_build's */
        {&uimn.tree.ed[0], &uimn.tree.editors, &ed_ops, {}},
        {&uimn.tree.ed[1], &uimn.tree.editors, &ed_ops, {}},
    };

    uimn.tree.screen = (Widget){.ops = &screen_ops, .visible = true};

    for (uint32_t i = 0; i < countof(tree); i++) {
        *tree[i].w = (Widget){.ops = tree[i].ops, .rect = tree[i].rect, .visible = true};

        tree_add(tree[i].parent, tree[i].w);
    }

    dpan_build(&uimn.tree.pane, uimn.tree.dp);
    fbar_build(&uimn.tree.screen); /* under the menus and dialogs */
    goto_build(&uimn.tree.screen);
    menu_build(&uimn.tree.screen); /* the modal ones, hidden till run */
    dlog_build(&uimn.tree.screen);
    help_build(&uimn.tree.workspace, &uimn.tree.screen);
    tree_focus(&uimn.tree.ed[0]);
}

void uimn_modal(Session *const S, Widget *const w)
{
    w->visible = true;

    while (w->visible) {
        draw_all(S);
        const uint16_t key = uimn_getkey();

        if (key == KEY_MOUSE) {
            if (!wind_drag())
                tree_mouse(&uimn.tree.screen, S, uimn.mouse.row, uimn.mouse.col);
        } else
            tree_key(&uimn.tree.screen, S, key);
    }
}

void uimn_init(Session *const S)
{
    S->repl.keep_pre = true; /* a module's lines shown as typed */
    uimn.rows = 25;          /* till uimn_size */
    uimn.cols = 80;
    uimn.nsizes = pscr_sizes(uimn.sizes);
    menu_sizes();
    build_tree();
    undo_init();
    jumps_init(); /* after undo's hook */
    uimn.on = true;
    LineBuf *const lines[] = {&uimn.edline, &uimn.line, &uimn.build, &uimn.split};

    for (uint32_t i = 0; i < sizeof lines / sizeof lines[0]; i++) {
        lines[i]->grows = true;
        edit_lb_need(S, lines[i], 0);
    }

    LineBuf *const fields[] = {&uimn.immline, &uimn.fname, &uimn.find_what, &uimn.change_to};

    uint8_t *const texts[] =
        {uimn.immline_text, uimn.fname_text, uimn.find_what_text, uimn.change_to_text};

    for (uint32_t i = 0; i < sizeof fields / sizeof fields[0]; i++)
        *fields[i] = (LineBuf){.text = texts[i], .cap = DLBUF};

    uimn.file[0] = 0;
    uimn.imm.n = 1;
    uimn.text.check = true;
    uimn.ed.insert = true;
    udbg_init(S);
    wind_init();
    mous_init();
    help_init();
    text_init(S, &dialect_qbasic);
    view_reset(S);
    edit_load(S, 0);
    edit_imm_load();
}

/* uimn.fname's text the command line's first word not a switch, FLDMAX at most; its length.
   *most: /H given (QBasic's: the most rows the card has). */
static uint16_t cmdline_file(bool *const most)
{
    char tail[128];
    psys_cmdline(tail, sizeof tail);
    uint16_t n = 0;
    *most = false;

    for (const char *p = tail; *p;) {
        while (*p && (uint8_t)*p <= ' ')
            p++;

        const char *const word = p;

        while ((uint8_t)*p > ' ')
            p++;

        if (word[0] == '/' && (word[1] | 0x20) == 'h' && p == word + 2)
            *most = true;
        else if (!n && p > word && word[0] != '/')
            for (; word + n < p && n < FLDMAX; n++)
                uimn.fname.text[n] = (uint8_t)word[n];
    }

    return n;
}

void uimn_size(const PscrSize size)
{
    const PscrSize s = pscr_text(size);
    uimn.rows = s.rows;
    uimn.cols = s.cols;
    them_in(); /* the mode's change loaded VGA's colours */
    scrn_size(s);
    uimn.tree.screen.rect = (Rect){0, 0, uimn.rows, uimn.cols};

    uimn.tree.menubar.rect.cols = uimn.cols;
    uimn.tree.workspace.rect = (Rect){1, 0, win_rows(), uimn.cols};

    uimn.tree.statusbar.rect = (Rect){status_row(), 0, 1, uimn.cols};

    uimn.dp_col = wind_pane_col();

    wind_fit();
}

void uimn_start(Session *const S)
{
    if (!S->cons.stdin_con || !pfil_iscon(PFIL_STDOUT))
        return;

    uimn_init(S);
    bool most;
    const uint16_t n = cmdline_file(&most);
    uimn_size(most ? (PscrSize){80, 50} : PSCR_80X25); /* /H: VGA's most */
    them_qbasic();

    if (n) {
        uimn.fname.len = n;
        dlog_fileok(DLOG_PRESS);
        ui.load_new = true;
        const Status st = repl_call(S, load);
        ui.load_new = false;

        if (st != ST_OK)
            erred(S, (uint8_t)st);
    } else {
        dlog_msg(S, s_welcome, bt_welcome);
    }

    loop(S);
}

/* CHAIN: Advanced feature unavailable in the editor. */
static void fe_chain(Session *const S)
{
    if (uimn.on)
        repl_error(S, E_AF);
}

/* The lexer's columns for the lit statement, the syntax colours and the tab completion; the
   map's block. */
static void regions(Session *const S)
{
    S->lexr.cols = true;
    lmap_init(S); /* HLMAP: the third block */
    upag_init(S);
}

const FrontEnd uimn_front_end = {
    .heap = 0xE0000, /* C_HEAP and the editor's buffers */
    .regions = regions,
    .start = uimn_start,
    .chain = fe_chain,
};
