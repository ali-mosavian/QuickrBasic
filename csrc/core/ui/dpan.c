/* The debug pane (src/ui/dpane.asm). */
#include "ui/dpan.h"
#include "session.h"
#include "comp/blok.h"
#include "io/emsg.h"
#include "mem/allo.h"
#include "mem/scop.h"
#include "mem/symb.h"
#include "num/dubl.h"
#include "ui/dlog.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/lmap.h"
#include "ui/mous.h"
#include "ui/scrn.h"
#include "ui/text.h"
#include "ui/uimn.h"
#include "ui/view.h"
#include "ui/wind.h"

enum {
    NDP = 3,     /* its windows: Watch, Locals, Call stack */
    DP_WAT = 0,
    DP_LOC = 1,
    DP_STK = 2,
    NDFRM = 16,  /* the call stack's frames shown at most */
    NDVAR = 64,  /* Locals' variables shown at most */
    NWATCH = 8,  /* watches at most */
    WXLEN = 41,  /* a watch's expression, 0-ended */
    ROWSZ = 256, /* a row being made: an expression, " = ", a value or a message */
    VIEW_NONE = 0xFF,
    FRAME_NAME = 15, /* a frame's row: its view's name, 15 characters at most, */
    FRAME_LINE = 16, /*   "line n" from column 16 */
    VK_FN = 3,       /* a DEF FN's kind (Var.info) */
};

/* TDFRM: a call stack's frame. */
typedef struct {
    int32_t line;    /* df_ln: its text line, -1 the immediate's */
    DebugFrame at;
    uint8_t view;    /* df_vw: its view, VIEW_NONE none */
    char text[DP_TW + 1];
} PaneFrame;

/* TDPANE (dp_on, tt_ms are uimn's). */
static struct {
    uint8_t focus;   /* dp_foc: the window with the focus */
    uint8_t frame;   /* dp_frm: the frame selected, 0 the stop's */
    uint8_t nframes; /* dp_nf */
    uint8_t sel[NDP], top[NDP]; /* each window's row selected, its first row shown */
    uint8_t row[NDP], h[NDP];   /* its title's row, its rows with it */
    uint8_t nwatch;  /* dp_nw: kept across runs */
    char wx[NWATCH][WXLEN];
    char wv[NWATCH][DP_TW];
    uint8_t tip_row, tip_col; /* tt_pos */
    uint16_t vid[NDVAR]; /* Locals' variables' ids */
    uint8_t nvars;
    char vt[NDVAR][DP_TW];
    PaneFrame fr[NDFRM];
    uint8_t proc;    /* g_dpp: Locals' procedure, 0 the module */
    char row_text[ROWSZ]; /* g_dprow */
    char tip[DP_TW];      /* g_dptip: a module variable's tooltip */
} dp;

static uint8_t fld_text[DLBUF];
static LineBuf fld = {.text = fld_text}; /* dp_fld: Ins's expression */

static DlogCtl add_ctl[] = {
    {.kind = CT_FLD, .row = 1, .col = 14, .cols = 34, .rows = 1, .field = &fld,
     .label = "Expression:", .lrow = 2, .lcol = 2},
    {.kind = CT_BTN, .row = 6, .col = 12, .cols = 6, .text = uimn_ok},
    {.kind = CT_BTN, .row = 6, .col = 30, .cols = 10, .text = uimn_cancel},
};
static Dlog add_dlog = {.row = 8, .col = 14, .rows = 8, .cols = 52, .title = "Add Watch",
                        .n = 3, .focus = 0, .sep = 5, .def = 1, .ctl = add_ctl};

static const char *const titles[NDP] = {"Watch", "Locals", "Call stack"};
static const char sfx_char[] = "%&!#$";  /* g_dpsfn: a suffix, */
static const uint8_t sfx_type[] = {ET_I2, ET_I4, ET_R4, ET_R8, ET_SD}; /* its type */
static const char sfx_of[] = {0, '%', '&', 0, '#', '$'}; /* by ET_*: the suffix shown */

/* dp_cpy: s at d, its 0 left out; past it. */
static char *cpy(char *const d, const char *const s)
{
    uint32_t i = 0;
    for (; s[i]; i++)
        d[i] = s[i];
    return d + i;
}

/* dp_dec: v in decimal at d, a sign when negative; past it. */
static char *dec(char *d, const int32_t v)
{
    uint32_t u = (uint32_t)v;
    if (v < 0) {
        *d++ = '-';
        u = 0u - u;
    }
    char t[10];
    int n = 0;
    do {
        t[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    for (int i = 0; i < n; i++)
        d[i] = t[n - 1 - i];
    return d + n;
}

/* dp_rowcut: dp.row_text to d, cut to DP_TW-1 characters, 0-ended. */
static void rowcut(char *const d)
{
    uint32_t i = 0;
    for (; i < DP_TW - 1 && dp.row_text[i]; i++)
        d[i] = dp.row_text[i];
    d[i] = 0;
}

/* dp_frm1: the stop's frame at: its text line, view and row. */
static void frame1(Session *const S, PaneFrame *const f, const DebugFrame *const at)
{
    f->at = *at;
    f->line = -1;
    f->view = VIEW_NONE;
    memset(f->text, ' ', DP_TW);
    f->text[DP_TW] = 0;
    uint32_t tl;
    if (at->line == DLINE || !lmap_of(S, at->line, &tl)) {
        cpy(f->text, "Immediate");
        return;
    }
    f->line = (int32_t)tl;
    const uint8_t v = view_of(tl);
    const View *const vw = view_at(v);
    const uint32_t n = tl - vw->first + 1;
    f->view = v;
    const char *const name = v ? vw->name : "module";
    for (int i = 0; i < FRAME_NAME && name[i]; i++)
        f->text[i] = name[i];
    dec(cpy(f->text + FRAME_LINE, "line "), (uint16_t)n);
}

/* dp_frames: the stop's frames, newest first. */
static void frames(Session *const S)
{
    DebugFrame at[NDFRM];
    dp.nframes = (uint8_t)session_frames(S, at, NDFRM);
    for (uint8_t i = 0; i < dp.nframes; i++)
        frame1(S, &dp.fr[i], &at[i]);
}

/* dp_geo: each window's title row and rows: the edit windows' rows, three tenths to Watch and to
   Call stack, the rest to Locals. */
static void geo(void)
{
    const uint8_t all = WIN_ROWS - uimn.win.win[W_IMM].height;
    const uint8_t q = (uint8_t)(all * 3 / 10);
    dp.h[DP_WAT] = q;
    dp.h[DP_STK] = q;
    dp.h[DP_LOC] = all - 2 * q;
    dp.row[DP_WAT] = 1;
    dp.row[DP_LOC] = q + 1;
    dp.row[DP_STK] = q + 1 + dp.h[DP_LOC];
}

/* dp_cnt: window w's items. */
static uint8_t count(const uint8_t w)
{
    return w == DP_WAT ? dp.nwatch : w == DP_LOC ? dp.nvars : dp.nframes;
}

/* dp_item: item i of window w, nullptr none. */
static const char *item(const uint8_t w, const uint32_t i)
{
    if (i >= count(w))
        return nullptr;
    return w == DP_WAT ? dp.wv[i] : w == DP_LOC ? dp.vt[i] : dp.fr[i].text;
}

static bool has_focus(const uint8_t w) { return uimn.focus == FOC_PANE && dp.focus == w; }

/* dp_line: row r of window w: its borders, its item. */
static void line1(const uint8_t w, const uint8_t r)
{
    const uint8_t row = dp.row[w] + 1 + r;
    const uint32_t i = dp.top[w] + r;
    const uint8_t slot = has_focus(w) && (uint8_t)i == dp.sel[w] ? SL_SEL : SL_ED;
    const bool cur = w == DP_STK && (uint8_t)i == dp.frame && (uint8_t)i < dp.nframes;
    uint8_t col = scrn_fill(row, DP_COL, SL_ED, GLYPH_V, 1);
    col = scrn_fill(row, col, slot, cur ? GLYPH_POINT_RIGHT : ' ', 1);
    scrn_fill(row, col, slot, ' ', DP_TW - 1);
    const char *const s = item(w, i);
    if (s) {
        uint16_t n = 0;
        while (s[n] && n < DP_TW - 1)
            n++;
        scrn_strn(row, col, (const uint8_t *)s, n, slot);
    }
    scrn_fill(row, DP_COL + DP_W - 1, SL_ED, GLYPH_V, 1);
}

void dpan_draw(void)
{
    if (!uimn.pane_on)
        return;
    geo();
    for (uint8_t w = 0; w < NDP; w++) {
        const bool top = !w; /* the top window's corners, else joins */
        draw_title(dp.row[w], DP_COL, DP_W, top ? GLYPH_TOP_LEFT : GLYPH_V_RIGHT,
                   top ? GLYPH_TOP_RIGHT : GLYPH_V_LEFT, titles[w], has_focus(w), false);
        for (uint8_t r = 0; r + 1 < dp.h[w]; r++)
            line1(w, r);
    }
}

/* dp_ispar: id is a parameter of procedure dp.proc. */
static bool is_param(Session *const S, const uint16_t id)
{
    if (!dp.proc)
        return false;
    const Proc *const p = blok_proc(&S->blok, dp.proc);
    for (uint16_t k = 0; k < p->npar; k++)
        if (p->params[k].var == id)
            return true;
    return false;
}

/* dp_val: the value of variable id's kind in cell, as text at d: a number as PRINT's without
   its sign's space, a string in quotes. Past it. */
static char *value(Session *const S, char *d, const uint16_t id, const Addr cell)
{
    const Var *const v = symb_var(&S->symb, id);
    uint8_t buf[DUBL_TXT];
    uint32_t n;
    if (v->info & SK_DBL) {
        const double x = load_cell(at(cell)).d;
        n = dubl_fmt(x, buf);
    } else if ((v->info & VK_MASK) == VK_STR) {
        const Desc *const s = (const Desc *)at(cell);
        const uint32_t len = s->len > DP_TW ? DP_TW : s->len;
        *d++ = '"';
        memcpy(d, at(s->ptr), len);
        d += len;
        *d++ = '"';
        return d;
    } else if (v->type == ET_R4 || v->type == ET_IMP) {
        n = dubl_fmtf(rd32(cell), buf);
    } else {
        return dec(d, (int32_t)rd32(cell));
    }
    const uint8_t *s = buf;
    if (*s == ' ') {
        s++;
        n--;
    }
    memcpy(d, s, n);
    return d + n;
}

/* dp_vtext: variable id's row in dp.row_text: its name, " = ", its value as the frame sees it;
   par its parameter's place or -1. */
static void var_text(Session *const S, const uint16_t id, const int32_t par)
{
    const Var *const v = symb_var(&S->symb, id);
    const uint8_t *const name = symb_text(&S->symb, v->name);
    char *d = dp.row_text;
    memcpy(d, name + 1, name[0]);
    d += name[0];
    if (v->type <= ET_SD && sfx_of[v->type])
        *d++ = sfx_of[v->type];
    d = cpy(d, " = ");
    if (v->info & VI_ARR)
        d = cpy(d, "(array)");
    else if (v->type >= ET_REC)
        d = cpy(d, "(record)");
    else
        d = value(S, d, id, session_cell(S, &dp.fr[dp.frame].at, id, par));
    *d = 0;
}

/* dp_var1: Locals' next row: variable id, parameter par of its procedure or -1. */
static void var1(Session *const S, const uint16_t id, const int32_t par)
{
    if (dp.nvars >= NDVAR)
        return;
    dp.vid[dp.nvars] = id;
    var_text(S, id, par);
    rowcut(dp.vt[dp.nvars]);
    dp.nvars++;
}

/* A variable Locals and the tooltip leave out (a DEF FN's). */
static bool skipped(Session *const S, const uint16_t id)
{
    return (symb_var(&S->symb, id)->info & (VI_ARR | VK_MASK)) == VK_FN;
}

/* dp_vars: Locals: the selected frame's procedure's variables, the module's at the module; its
   parameters first, then by id. */
static void vars(Session *const S)
{
    dp.proc = dp.fr[dp.frame].at.proc;
    dp.nvars = 0;
    if (dp.proc) {
        const Proc *const p = blok_proc(&S->blok, dp.proc);
        for (uint16_t k = 0; k < p->npar; k++)
            var1(S, p->params[k].var, k);
    }
    for (uint16_t id = 1; id < S->symb.nvars; id++)
        if (symb_varx(&S->symb, id)->scope == dp.proc && !skipped(S, id) && !is_param(S, id))
            var1(S, id, -1);
}

/* session_eval's output out at d, its spaces and end of line left out; past it. */
static char *eval_text(char *d, const char *const out)
{
    uint32_t a = 0;
    while (out[a] == ' ')
        a++;
    uint32_t n = 0;
    for (; (uint8_t)out[a + n] >= ' '; n++)
        d[n] = out[a + n];
    d += n;
    while (d[-1] == ' ') /* may eat into " = " for an empty string, as the asm */
        d--;
    return d;
}

/* dp_wval: expression expr's value as text at d (in dp.row_text): session_eval's output, its
   spaces and end of line left out; an error's message. A ':' (another statement) or a direct
   line's frame on cstk (its code would go): none. Past it. */
static char *watch_value(Session *const S, char *const d, const char *const expr)
{
    if (strchr(expr, ':'))
        return cpy(d, emsg_text(S, E_SN));
    if (session_in_direct(S))
        return cpy(d, "(not here)");
    char out[80];
    const uint8_t err = session_eval(S, expr, dp.proc, out);
    return err ? cpy(d, emsg_text(S, err - 1)) : eval_text(d, out);
}

/* dp_weval: each watch's row: its expression, " = ", what PRINT shows of it in the stop's
   procedure, or its error's message. */
static void watch_eval(Session *const S)
{
    dp.proc = dp.fr[0].at.proc;
    for (uint8_t i = 0; i < dp.nwatch; i++) {
        char *d = cpy(cpy(dp.row_text, dp.wx[i]), " = ");
        d = watch_value(S, d, dp.wx[i]);
        *d = 0;
        rowcut(dp.wv[i]);
    }
}

void dpan_open(Session *const S)
{
    uimn.pane_on = true;
    dp.focus = 0;
    dp.frame = 0;
    memset(dp.sel, 0, sizeof dp.sel);
    memset(dp.top, 0, sizeof dp.top);
    frames(S);
    vars(S);
    watch_eval(S);
    wind_layout();
}

void dpan_close(void)
{
    if (!uimn.pane_on)
        return;
    uimn.pane_on = false;
    if (uimn.focus == FOC_PANE)
        uimn.focus = FOC_EDIT;
    wind_layout();
}

/* dp_vis: window w's first row shown: its selected row in view. */
static void visible(const uint8_t w)
{
    geo();
    const uint8_t s = dp.sel[w];
    if (s < dp.top[w]) {
        dp.top[w] = s;
        return;
    }
    const uint8_t rows = dp.h[w] - 1;
    if (!rows || (uint8_t)(s - dp.top[w]) < rows)
        return;
    dp.top[w] = s - rows + 1;
}

/* dp_go: frame i selected: the cursor at the start of its line. */
static void go(Session *const S, const uint8_t i)
{
    dp.frame = i;
    vars(S);
    const int32_t line = dp.fr[i].line;
    if (line == -1)
        return;
    edit_commit(S);
    view_goline(S, (uint32_t)line);
    uimn.ed.sel_on = false;
    edit_load(S, (uint32_t)line - uimn.text.first);
    uimn.edline.col = 0;
    edit_ensure();
}

/* dp_wadd: Ins in Watch: an expression asked, added, evaluated. */
static void watch_add(Session *const S)
{
    if (dp.nwatch >= NWATCH)
        return;
    add_dlog.focus = 0;
    fld.len = 0;
    fld.col = 0;
    if (dlog_run(S, &add_dlog) != 1)
        return;
    if (!fld.len)
        return;
    const uint32_t n = fld.len < WXLEN - 1 ? fld.len : WXLEN - 1;
    dp.sel[DP_WAT] = dp.nwatch;
    memcpy(dp.wx[dp.nwatch], fld.text, n);
    dp.wx[dp.nwatch][n] = 0;
    dp.nwatch++;
    watch_eval(S);
    visible(DP_WAT);
}

/* dp_wdel: Del in Watch: the selected watch dropped. */
static void watch_del(void)
{
    const uint8_t i = dp.sel[DP_WAT];
    if (i >= dp.nwatch)
        return;
    dp.nwatch--;
    const uint32_t after = dp.nwatch - i;
    memmove(dp.wx[i], dp.wx[i + 1], after * WXLEN);
    memmove(dp.wv[i], dp.wv[i + 1], after * DP_TW);
    if (i >= dp.nwatch && i > 0)
        dp.sel[DP_WAT] = i - 1;
}

/* Up or Down in window w: its row selected moved by one, kept in view. */
static void sel_step(const uint8_t w, const uint8_t scan)
{
    const bool up = scan == SCAN_UP;
    const int32_t s = dp.sel[w] + (up ? -1 : 1);
    if (up ? s < 0 : s >= count(w))
        return;
    dp.sel[w] = (uint8_t)s;
    visible(w);
}

/* A key with no character (an extended one) in window w. */
static void scan_key(Session *const S, const uint8_t w, const uint8_t scan)
{
    if (w == DP_WAT && scan == SCAN_INS)
        watch_add(S);
    else if (w == DP_WAT && scan == SCAN_DEL)
        watch_del();
    else if (scan == SCAN_UP || scan == SCAN_DOWN)
        sel_step(w, scan);
}

void dpan_key(Session *const S, const uint16_t key)
{
    const uint8_t w = dp.focus;
    const uint8_t c = (uint8_t)key, scan = (uint8_t)(key >> 8);
    if (c == CH_TAB)
        dp.focus = w + 1 < NDP ? w + 1 : 0;
    else if (key == KEY_SHIFT_TAB)
        dp.focus = w ? w - 1 : NDP - 1;
    else if (c == CH_ESC)
        wind_focus(S, uimn.win.active);
    else if (c == CH_ENTER) {
        if (w == DP_STK && dp.sel[w] < dp.nframes)
            go(S, dp.sel[w]);
    } else if (!c)
        scan_key(S, w, scan);
}

void dpan_mouse(Session *const S, const uint8_t row, const uint8_t col)
{
    if (uimn.mouse.event != MS_PRESS)
        return;
    wind_focus(S, W_DBG);
    geo();
    int w = NDP - 1;
    while (w >= 0 && row < dp.row[w])
        w--;
    if (w < 0)
        return;
    dp.focus = (uint8_t)w;
    const uint8_t r = row - dp.row[w];
    if (!r)
        return;
    const uint8_t i = (uint8_t)(r - 1 + dp.top[w]);
    if (i >= count((uint8_t)w))
        return;
    dp.sel[w] = i;
    if (w == DP_STK)
        go(S, i);
}

void dpan_hover(const uint8_t row, const uint8_t col)
{
    uimn.tip_mouse = true;
    dp.tip_row = row;
    dp.tip_col = col;
}

static bool is_name_char(const uint8_t c) { return c == '.' || is_digit(c) || is_letter(c); }

static bool is_suffix(const uint8_t c) { return c && strchr(sfx_char, c); }

/* dp_word: the name at column col of s, len long, not in a string or after a '. *start its
   first column, *n its length (its suffix left out), *sfx its suffix, 0 none. */
static bool word(const uint8_t *const s, const uint32_t len, uint32_t col, uint32_t *const start,
                 uint32_t *const n, uint8_t *const sfx)
{
    if (col >= len)
        return false;
    if (is_suffix(s[col])) { /* on a suffix: the name before it */
        if (!col)
            return false;
        col--;
    }
    if (!is_name_char(s[col]))
        return false;
    uint32_t b = col;
    while (b && is_name_char(s[b - 1]))
        b--;
    if (!is_letter(s[b]))
        return false;
    uint32_t e = col;
    do
        e++;
    while (e < len && is_name_char(s[e]));
    *start = b;
    *n = e - b;
    *sfx = e < len && is_suffix(s[e]) ? s[e] : 0;
    bool quoted = false;
    for (uint32_t i = 0; i < b; i++) {
        if (s[i] == '"')
            quoted = !quoted;
        else if (s[i] == '\'' && !quoted)
            return false;
    }
    return true;
}

/* dp_match: variable id is the name s, n long, suffix sfx: its letters the name's, the suffix,
   if any, its type's. */
static bool match(Session *const S, const uint16_t id, const uint8_t *const s, const uint32_t n, const uint8_t sfx)
{
    const Var *const v = symb_var(&S->symb, id);
    if (sfx && sfx_type[strchr(sfx_char, sfx) - sfx_char] != v->type)
        return false;
    const uint8_t *const t = symb_text(&S->symb, v->name);
    if ((uint8_t)n != t[0])
        return false;
    for (uint32_t i = 0; i < n; i++)
        if (upcase(s[i]) != t[1 + i])
            return false;
    return true;
}

/* dp_seen: module variable id is seen in procedure dp.proc: DIM SHARED, or SHARED there. */
static bool seen(Session *const S, const uint16_t id)
{
    const uint16_t name = symb_var(&S->symb, id)->name;
    const Binding *b = scop_get(S, 0, name);
    if (b && b->flags & BF_SHR)
        return true;
    b = scop_get(S, blok_proc(&S->blok, dp.proc)->name, name);
    return b && b->flags & BF_REF;
}

/* dp_find: the row of the name s, n long, suffix sfx: Locals' variable of it, else a module one
   the frame's procedure sees, SHARED; nullptr none. */
static const char *find(Session *const S, const uint8_t *const s, const uint32_t n, const uint8_t sfx)
{
    for (uint8_t i = 0; i < dp.nvars; i++)
        if (match(S, dp.vid[i], s, n, sfx))
            return dp.vt[i];
    if (!dp.proc) /* the module's: all in Locals */
        return nullptr;
    for (uint16_t id = 1; id < S->symb.nvars; id++) {
        if (symb_varx(&S->symb, id)->scope || skipped(S, id) || !match(S, id, s, n, sfx) || !seen(S, id))
            continue;
        var_text(S, id, -1);
        rowcut(dp.tip);
        return dp.tip;
    }
    return nullptr;
}

/* The tooltip's text line: the cursor's, else the mouse's in the active window; *text, *len,
   *col in it, *line its number. False: none. */
static bool tip_at(Session *const S, const uint8_t **const text, uint32_t *const len, uint32_t *const col,
                   uint32_t *const line)
{
    const UiEdit *const ed = &uimn.ed;
    if (!uimn.tip_mouse) {
        if (uimn.focus != FOC_EDIT)
            return false;
        *col = uimn.edline.col;
        *text = uimn.edline.text;
        *len = uimn.edline.len;
        *line = ed->line;
        return true;
    }
    uint8_t w;
    if (!wind_at(dp.tip_row, dp.tip_col, &w) || w != uimn.win.active)
        return false;
    if (dp.tip_row < ed->row0 || dp.tip_row - ed->row0 >= ed->rows)
        return false;
    const uint32_t l = dp.tip_row - ed->row0 + ed->top;
    if (l >= uimn.text.n || dp.tip_col <= ed->x)
        return false;
    const uint32_t c = dp.tip_col - ed->x - 1u;
    if (c >= ed->width)
        return false;
    *col = c + ed->left;
    *line = (uint16_t)l;
    *len = text_get(S, *line, uimn.line);
    *text = uimn.line;
    return true;
}

/* Tooltip s for the name at column start of line line: under it, else over it, in the window. */
static void tip_show(const char *const s, const uint32_t start, const uint32_t line)
{
    const UiEdit *const ed = &uimn.ed;
    uint16_t r = (uint16_t)(line - ed->top + ed->row0 + 1);
    if (r >= ed->row0 + ed->rows)
        r -= 2;
    const uint32_t sn = strlen(s);
    const uint32_t edge = ed->x + 1u + ed->width; /* past the window */
    const uint32_t c0 = ed->x + 1u + (start > ed->left ? start - ed->left : 0);
    const uint32_t c = c0 + sn + 2 > edge ? edge - sn - 2 : c0;
    const uint8_t row = (uint8_t)r;
    uint8_t x = scrn_fill(row, (uint8_t)c, SL_BAR, ' ', 1);
    x = scrn_strn(row, x, (const uint8_t *)s, (uint16_t)sn, SL_BAR);
    scrn_fill(row, x, SL_BAR, ' ', 1);
}

void dpan_tip(Session *const S)
{
    if (!uimn.pane_on || !uimn_stopped())
        return;
    if (dp.fr[dp.frame].view != uimn.ed.view) /* in the frame's view */
        return;
    const uint8_t *text;
    uint32_t len, col, line;
    if (!tip_at(S, &text, &len, &col, &line))
        return;
    uint32_t start, n;
    uint8_t sfx;
    if (!word(text, len, col, &start, &n, &sfx))
        return;
    const char *const s = find(S, text + start, n, sfx);
    if (s)
        tip_show(s, start, line);
}
