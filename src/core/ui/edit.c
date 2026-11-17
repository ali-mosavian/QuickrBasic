/* Editing: the one-line buffer, the program window's lines, the selection, the clipboard and the
 * immediate window.
 *
 * # How it works
 * The program window edits its line, uimn.ed.line, in uimn.edline and stores it when the cursor
 * leaves (edit_commit): spaces at its end cut, keywords capitalised, the syntax checked. A syntax
 * error is shown and the cursor stays.
 *
 * A selection from (sel_line, sel_col) to the cursor is a piece of one line, or whole lines when it
 * spans several. The insert mode, uimn.ed.insert, is every line buffer's.
 *
 * The immediate window keeps its last lines (uimn.imm); edit_imm_up and edit_imm_down recall them
 * into uimn.immline.
 *
 * # Example
 *   print x   typed, then the cursor leaves    stored as: PRINT x
 *
 * # asm structure (retired/asm32/ui/edit.asm)
 * | asm | here | |
 * |---|---|---|
 * | le_ins, le_insn, le_cut | edit_lb_type, edit_lb_insert, edit_lb_cut | MIRROR; D1 |
 * | le_bs, le_del | edit_lb_bs, edit_lb_del | MIRROR; D1 |
 * | le_home, le_end, le_left, le_right, le_wleft, le_wright | edit_lb_home, edit_lb_end, edit_lb_left, edit_lb_right, edit_lb_wleft, edit_lb_wright | MIRROR |
 * | le_pad, le_indent | pad, spaces (static) | MIRROR |
 * | ed_load, ed_commit, ed_goto | edit_load, edit_commit, edit_goto | MIRROR; D1 |
 * | ed_up, ed_down, ed_pgup, ed_pgdn, ed_first, ed_last | edit_up, edit_down, edit_pgup, edit_pgdn, edit_first, edit_last | MIRROR |
 * | ed_ensure, ed_enter, ed_bs, ed_del, ed_tab, ed_ctrly | edit_ensure, edit_enter, edit_bs, edit_del, edit_tab, edit_ctrly | MIRROR |
 * | sel_mark, sel_get | edit_sel_mark, edit_sel (SEL_NONE, SEL_PIECE, SEL_LINES) | MIRROR |
 * | ed_copy, ed_clear, ed_cut, ed_paste | edit_copy, edit_clear, edit_cut, edit_paste | MIRROR |
 * | im_load, im_up, im_down, im_next | edit_imm_load, edit_imm_up, edit_imm_down, edit_imm_next | MIRROR |
 * | im_store | imm_store (static) | MIRROR |
 * | ui_body, ui_canon, ui_dxerr, ui_emsg | edit_body, edit_canon, edit_dxerr, edit_emsg | MIRROR; D1 |
 * | ui_chkop | chkop (static) | MIRROR; link_walk's each |
 * | g_ui.fd.uc_pre, uc_err, uc_eop, uc_ecol, uc_src, uc_n | static (canon's state) | MIRROR; edit_commit reads uc_ecol |
 * | g_ui.dg.wr_ind | dlog_wrap's indent | D2 |
 * | g_ui.ed.*, g_ui.im.*, bf.im_h | uimn.ed (UiEdit), uimn.imm (UiImm) | MOVED (ui.h) |
 * | g_ui.bf.ed_lb, im_lb | uimn.edline, uimn.immline | MOVED (ui.h) |
 *
 * - D1. A flag result is a bool, true the success; edit_commit false means a syntax error shown.
 * - D2. A parameter, not a global set before the call.
 */
#include "ui/edit.h"
#include "util/grow.h"
#include "ui/indent.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/lexer.h"
#include "comp/link.h"
#include "comp/listing.h"
#include "io/console.h"
#include "io/diag.h"
#include "io/errmsg.h"
#include "ui/dialog.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/view.h"

/* ui_canon's state (the asm's uc_*): the line, its body's start, the ERR op found and the column
   of the error. */
static struct {
    uint8_t *src;
    uint16_t n;
    const uint8_t *pre;
    uint8_t err;
    const ErrOp *eop;
    uint16_t ecol;
} uc;

/* ed_tab's tab stops */

static void changed(void)
{
    uimn.text.changed = true;
    uimn.text.unsaved = true;
}

/* ---- a line buffer */

enum { LB_TAIL = LNBUF - LNMAX }; /* the bytes past a line's characters: a 0, an LF */

bool edit_lb_room(LineBuf *const b, const uint32_t n)
{
    return n + LB_TAIL <= b->cap || (b->grows && GROW(b->text, b->cap, n + LB_TAIL));
}

uint8_t *edit_lb_need(Session *const S, LineBuf *const b, const uint32_t n)
{
    if (!edit_lb_room(b, n))
        repl_error(S, E_OM);

    return b->text;
}

/* n characters past the cursor (the text padded to it) or past the end, whichever is further. */
static uint32_t beyond(const LineBuf *const b, const uint32_t n)
{
    return (b->col > b->len ? b->col : b->len) + n;
}

/* le_pad: the text padded with spaces to the cursor. */
static void pad(LineBuf *const b)
{
    if (b->col <= b->len)
        return;

    memset(b->text + b->len, ' ', b->col - b->len);
    b->len = b->col;
}

bool edit_lb_type(LineBuf *const b, const uint8_t c, const uint16_t most)
{
    const uint16_t at = b->col;

    if (at >= most || !edit_lb_room(b, beyond(b, 1)))
        return false;

    pad(b);

    if (uimn.ed.insert || at >= b->len) {
        if (b->len >= most)
            return false;

        memmove(b->text + at + 1, b->text + at, b->len - at);
        b->len++;
    }

    b->text[at] = c;
    b->col++;
    b->dirty = true;
    return true;
}

void edit_lb_cut(LineBuf *const b, const uint16_t from, const uint16_t to)
{
    const uint16_t end = to > b->len ? b->len : to;
    b->col = from;

    if (from >= end)
        return;

    memmove(b->text + from, b->text + end, b->len - end);
    b->len -= end - from;
    b->dirty = true;
}

bool edit_lb_bs(LineBuf *const b)
{
    if (!b->col)
        return false;

    const uint16_t c = b->col - 1;
    edit_lb_cut(b, c, c + 1);
    b->col = c;
    return true;
}

bool edit_lb_del(LineBuf *const b)
{
    if (b->col >= b->len)
        return false;

    edit_lb_cut(b, b->col, b->col + 1);
    return true;
}

/* The spaces the n characters at s start with. */
static uint16_t spaces(const uint8_t *const s, const uint16_t n)
{
    for (uint16_t i = 0; i < n; i++)
        if (s[i] != ' ')
            return i;

    return n;
}

/* le_indent: the spaces the text starts with. */
static uint16_t indent(const LineBuf *const b) { return spaces(b->text, b->len); }

void edit_lb_home(LineBuf *const b)
{
    const uint16_t n = indent(b);
    b->col = n == b->col ? 0 : n;
}

void edit_lb_end(LineBuf *const b) { b->col = b->len; }

void edit_lb_left(LineBuf *const b)
{
    if (b->col)
        b->col--;
}

void edit_lb_right(LineBuf *const b)
{
    if (b->col < LNMAX)
        b->col++;
}

void edit_lb_wleft(LineBuf *const b)
{
    uint32_t i = b->col <= b->len ? b->col : b->len;

    while (i) {
        i--;

        if (b->text[i] != ' ')
            break;
    }

    while (i && b->text[i - 1] != ' ')
        i--;

    b->col = (uint16_t)i;
}

void edit_lb_wright(LineBuf *const b)
{
    uint32_t i = b->col;

    while (i < b->len && b->text[i] != ' ')
        i++;
    if (i < b->len)
        i += spaces(b->text + i, b->len - i);

    b->col = (uint16_t)i;
}

bool edit_lb_insert(LineBuf *const b, const uint8_t *const s, const uint16_t n)
{
    if (!edit_lb_room(b, beyond(b, n)))
        return false;

    pad(b);

    if ((uint16_t)(b->len + n) > LNMAX)
        return false;

    memmove(b->text + b->col + n, b->text + b->col, b->len - b->col);
    memcpy(b->text + b->col, s, n);
    b->len += n;
    b->col += n;
    b->dirty = true;
    return true;
}

/* ---- lines as the editor stores them */

static bool is_space(const uint8_t c) { return c == ' '; }

/* Past the characters from text[i] that in holds for (0 ends text). */
static size_t past(const uint8_t *const text, const size_t i, bool (*const in)(uint8_t))
{
    for (size_t j = i;; j++)
        if (!in(text[j]))
            return j;
}

uint8_t *edit_body(uint8_t *const text)
{
    const size_t num = past(text, 0, is_space);
    const size_t gap = past(text, num, is_digit);
    return text + past(text, gap, is_space);
}

/* ui_dxerr's, ui_canon's: what print prints, in cons.sbuf; its length. */
static uint32_t captured(Session *const S, void (*const print)(Session *S))
{
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    print(S);
    S->cons.capture = 0;
    return S->cons.sblen;
}

/* ui_chkop: link_walk's: an ERR op's error. */
static void chkop(const uint8_t op, const uint8_t *const at)
{
    if (op != O_ERR)
        return;

    uc.err = at[1];
    uc.eop = (const ErrOp *)at;
}

/* The body at uc.pre, n characters, tokenized into comp_cbuf, its declarations entered; the
   tokens' length. */
static uint32_t tokenize(Session *const S, const uint32_t n)
{
    memcpy(edit_lb_need(S, &uimn.line, n), uc.pre, n + 1);
    S->vm.line = DLINE;
    const uint32_t len =
        (uint32_t)(lexr_line(&S->lexr, uimn.line.text, S->comp.cbuf) - S->comp.cbuf);
    repl_declare(S, S->comp.cbuf); /* a typed SUB known to the lines after */
    return len;
}

/* The body compiled for its syntax; false: an error, its column in uc.ecol. */
static bool check(Session *const S)
{
    S->comp.check = true;
    comp_line(&S->comp);
    S->comp.check = false;
    uc.err = 0;
    link_walk(S->comp.xbuf, chkop);

    if (!uc.err)
        return true;

    uc.ecol = edit_dxerr(S, uc.src, uc.eop);
    return false;
}

/* The line as LIST shows it: its label and indent as typed, the body listed. */
static void print_line(Session *const S)
{
    const size_t n = (size_t)(uc.pre - uc.src);

    for (size_t i = 0; i < n; i++)
        cons_putc(&S->cons, uc.src[i]);

    list_body(&S->comp, S->comp.cbuf);
}

bool edit_canon(Session *const S, uint8_t *const text, const uint16_t len, uint32_t *const lexed)
{
    uc.src = text;
    uc.n = len;
    text[len] = 0;
    uint8_t *const body = edit_body(text);
    *lexed = 0;

    if (*body) {
        uc.pre = body;
        const uint32_t n = tokenize(S, (uint32_t)(text + len - body));

        if (uimn.text.check && !check(S))
            return false;
        if (captured(S, print_line) <= LNMAX) {
            *lexed = n;
            return true;
        }
    }

    S->cons.sblen = uc.n; /* as typed */
    memcpy(S->cons.sbuf, uc.src, uc.n);
    return true;
}

static void what(Session *const S) { diag_what(&S->diag); }
static void todo(Session *const S) { diag_todo(&S->diag); }
static void shape(Session *const S) { diag_shape(&S->diag); }

/* '|', then (and its second, when not 0) sep2, then what print prints, wrapped indent in; nothing
   printed: nothing, the separators dropped (@udx_p). */
static uint8_t *part(
    Session *const S,
    uint8_t *const start,
    const uint8_t sep2,
    void (*const print)(Session *S),
    const uint8_t indent
)
{
    uint8_t *d = start;
    *d++ = '|';

    if (sep2)
        *d++ = sep2;

    const uint32_t n = captured(S, print);

    if (!n)
        return start;

    return dlog_wrap(d, S->cons.sbuf, n, indent);
}

uint16_t edit_dxerr(Session *const S, const uint8_t *const text, const ErrOp *const op)
{
    const uint8_t *const body = edit_body((uint8_t *)text);
    const uint32_t bodycol = (uint32_t)(body - text);
    memcpy(S->cons.inbuf, body, strlen((const char *)body) + 1);
    S->lexr.all = true; /* the body's bytes to columns */
    lexr_line(&S->lexr, S->cons.inbuf, S->comp.cbuf);
    S->lexr.all = false;
    const uint16_t col = lexr_colof(&S->lexr, op->at);
    const uint32_t at = diag_at(
        &S->diag,
        S->vm.err.user,
        (const char *)S->cons.inbuf,
        col,
        op->code,
        op->dg,
        op->state
    );

    edit_emsg(S, op->code); /* its class, the title */
    uint8_t *d = (uint8_t *)uimn.errmsg + strlen(uimn.errmsg);
    *d++ = '|';
    const uint32_t n = captured(S, what);
    d = dlog_wrap(d, S->cons.sbuf, n, 0);
    d = part(S, d, '|', todo, 0);
    d = part(S, d, 0, shape, 2);
    *d = 0;
    return (uint16_t)(at + bodycol);
}

void edit_emsg(Session *const S, const uint8_t code)
{
    const char *const s = emsg_text(S->vm.err.user, code);
    memcpy(uimn.errmsg, s, strlen(s) + 1);
}

/* ---- the program window */

void edit_load(Session *const S, const uint32_t line)
{
    uimn.ed.line = line;
    uimn.edline.len = (uint16_t)text_get(S, line, &uimn.edline);
    uimn.edline.dirty = false;
}

bool edit_commit(Session *const S)
{
    LineBuf *const b = &uimn.edline;

    if (!b->dirty)
        return true;

    b->dirty = false;

    while (b->len && b->text[b->len - 1] == ' ')
        b->len--;

    changed();
    decl_enter(&S->decl, view_proc(S, uimn.ed.view)); /* its declarations its procedure's */
    uint32_t lexed;

    if (!edit_canon(S, b->text, b->len, &lexed)) { /* a syntax error: kept as typed */
        uimn.text.asis = true;
        text_repl(S, uimn.ed.line, b->text, b->len);
        uimn_selword(uc.ecol); /* the error's word */
        dlog_err(S, uimn.errmsg);
        return false;
    }

    uimn.text.asis = false;
    b->len = (uint16_t)S->cons.sblen;
    memcpy(edit_lb_need(S, b, b->len), S->cons.sbuf, S->cons.sblen);
    text_repl(S, uimn.ed.line, b->text, b->len);
    view_commit(S);
    return true;
}

bool edit_goto(Session *const S, const uint32_t line)
{
    if (!edit_commit(S))
        return false;

    edit_load(S, line);
    return true;
}

void edit_up(Session *const S)
{
    if (uimn.ed.line)
        edit_goto(S, uimn.ed.line - 1);
}

void edit_down(Session *const S)
{
    if (uimn.ed.line + 1 < uimn.text.n)
        edit_goto(S, uimn.ed.line + 1);
}

void edit_pgup(Session *const S)
{
    const uint32_t rows = uimn.ed.rows;
    const uint32_t line = uimn.ed.line < rows ? 0 : uimn.ed.line - rows + 1;
    uimn.ed.top = uimn.ed.top < rows ? 0 : uimn.ed.top - rows + 1;
    edit_goto(S, line);
}

void edit_pgdn(Session *const S)
{
    const uint32_t step = (uint32_t)uimn.ed.rows - 1;
    const uint32_t line = uimn.ed.line + step;
    const uint32_t last = uimn.text.n - 1;
    uimn.ed.top += step;
    edit_goto(S, line > last ? last : line);
}

void edit_first(Session *const S)
{
    uimn.edline.col = 0;
    edit_goto(S, 0);
}

void edit_last(Session *const S)
{
    if (edit_goto(S, uimn.text.n - 1))
        uimn.edline.col = uimn.edline.len;
}

void edit_ensure(void)
{
    UiEdit *const e = &uimn.ed;

    if (e->line < e->top)
        e->top = e->line;
    if (e->line >= e->top + e->rows)
        e->top = e->line - e->rows + 1;
    if (e->top >= uimn.text.n)
        e->top = uimn.text.n - 1;

    const uint16_t col = uimn.edline.col;

    if (col < e->left)
        e->left = col;
    if (col >= (uint16_t)(e->left + e->width))
        e->left = col - e->width + 1;
}

/* edit_enter's: the line split at the cursor, the rest (its spaces dropped) after ind spaces in
   uimn.build; its length. */
static uint16_t split(Session *const S, LineBuf *const b, const uint16_t ind)
{
    uint8_t *const build =
        edit_lb_need(S, &uimn.build, ind + (b->col < b->len ? b->len - b->col : 0));
    memset(build, ' ', ind);
    const uint16_t at = b->col;

    if (at >= b->len)
        return ind;

    const uint16_t rest = b->len - at;
    b->len = at;
    b->dirty = true;
    const uint8_t *const s = b->text + at;
    const uint16_t skip = spaces(s, rest);
    memcpy(build + ind, s + skip, rest - skip);
    return ind + rest - skip;
}

/* indt_enter's: line i of the program window's text. */
static uint16_t text_line(void *const S, const uint32_t i, uint8_t *const dst)
{
    const uint32_t n = text_get(S, i, &uimn.line);
    memcpy(dst, uimn.line.text, n);
    return (uint16_t)n;
}

/* The line's indent made n spaces, the cursor kept on its character. */
static void reindent(LineBuf *const b, const uint16_t n)
{
    const uint16_t lead = indent(b);
    const uint16_t col = b->col;

    if (n == lead)
        return;
    if (n < lead) {
        edit_lb_cut(b, 0, lead - n);
    } else {
        b->col = 0;

        for (uint16_t i = lead; i < n; i++)
            edit_lb_insert(b, (const uint8_t *)" ", 1);
    }

    b->col = (uint16_t)(col + n - lead);
    b->dirty = true;
}

void edit_enter(Session *const S)
{
    LineBuf *const b = &uimn.edline;
    const uint16_t lead = indent(b);
    uint16_t ind = lead > b->col ? b->col : lead;

    if (b->col > lead) { /* past the indent: by the blocks (indent.h) */
        uint16_t self;
        indt_enter(text_line, S, uimn.ed.line, b->text, b->col, &self, &ind);
        reindent(b, self);
    }

    uimn.ed.indent = ind;
    uimn.ed.nlen = split(S, b, ind); /* the new line: the indent, the rest */
    const uint8_t view = uimn.ed.view;
    edit_commit(S);

    if (view != uimn.ed.view) /* a SUB line: its own view, made */
        return;

    const uint32_t line = uimn.ed.line + 1;
    text_insert(S, line, uimn.build.text, uimn.ed.nlen);
    changed();
    edit_load(S, line);
    b->col = uimn.ed.indent;
}

void edit_bs(Session *const S)
{
    LineBuf *const b = &uimn.edline;

    if (b->col && indent(b) >= b->col) { /* in the indent: back to the step before */
        const uint16_t k = b->col % INDT_STEP ? b->col % INDT_STEP : INDT_STEP;
        edit_lb_cut(b, b->col - k, b->col);
        b->dirty = true;
        return;
    }

    if (edit_lb_bs(b))
        return;

    const uint32_t line = uimn.ed.line;

    if (!line)
        return;

    uimn.ed.nlen = b->len; /* this line, kept */
    memcpy(edit_lb_need(S, &uimn.build, b->len), b->text, b->len);
    const uint16_t above = (uint16_t)text_get(S, line - 1, &uimn.line);

    if ((uint16_t)(above + uimn.ed.nlen) > LNMAX)
        return;

    text_delete(S, line);
    edit_load(S, line - 1);
    b->col = b->len;
    edit_lb_insert(b, uimn.build.text, uimn.ed.nlen);
    b->col = b->len - uimn.ed.nlen;
    b->dirty = true;
    changed();
}

void edit_del(Session *const S)
{
    LineBuf *const b = &uimn.edline;

    if (edit_lb_del(b))
        return;

    const uint32_t next = uimn.ed.line + 1;

    if (next >= uimn.text.n)
        return;

    const uint16_t n = (uint16_t)text_get(S, next, &uimn.build);
    uimn.ed.nlen = n;
    edit_lb_need(S, b, beyond(b, n));
    pad(b);

    if ((uint16_t)(b->len + n) > LNMAX)
        return;

    text_delete(S, next);
    const uint16_t col = b->col;
    edit_lb_insert(b, uimn.build.text, uimn.ed.nlen);
    b->col = col;
    b->dirty = true;
    changed();
}

void edit_tab(void)
{
    LineBuf *const b = &uimn.edline;

    while (edit_lb_type(b, ' ', LNMAX) && b->col % INDT_STEP)
        ;
}

void edit_ctrly(Session *const S)
{
    LineBuf *const b = &uimn.edline;

    if (b->dirty) { /* kept as typed: no syntax check */
        b->dirty = false;
        text_repl(S, uimn.ed.line, b->text, b->len);
    }

    uint32_t line = uimn.ed.line;
    text_clip_lines(S, line, line);
    text_delete(S, line);
    changed();

    if (line >= uimn.text.n)
        line = uimn.text.n - 1;

    edit_load(S, line);
    b->col = 0;
}

/* ---- the selection, the clipboard */

void edit_sel_mark(void)
{
    if (uimn.ed.sel_on)
        return;

    uimn.ed.sel_on = true;
    uimn.ed.sel_line = uimn.ed.line;
    uimn.ed.sel_col = uimn.edline.col;
}

uint8_t edit_sel(void)
{
    UiEdit *const e = &uimn.ed;

    if (!e->sel_on)
        return SEL_NONE;
    if (e->sel_line == e->line) {
        const uint32_t from = e->sel_col, to = uimn.edline.col;
        const uint32_t a = from < to ? from : to, b = from < to ? to : from;
        e->sel_a = a;
        e->sel_b = b;
        return a == b ? SEL_NONE : SEL_PIECE;
    }

    uint32_t a = e->sel_line, b = e->line, col = uimn.edline.col; /* the lower end's column */
    if (a >= b) {
        a = e->line;
        b = e->sel_line;
        col = e->sel_col;
    }

    if (!col)
        b--;

    e->sel_a = a;
    e->sel_b = b;
    return SEL_LINES;
}

uint8_t edit_copy(Session *const S)
{
    const uint8_t s = edit_sel();

    if (s == SEL_PIECE) {
        const LineBuf *const b = &uimn.edline;
        const uint32_t end = uimn.ed.sel_b <= b->len ? uimn.ed.sel_b : b->len;
        const uint32_t n = end >= uimn.ed.sel_a ? end - uimn.ed.sel_a : 0;
        text_clip_put(S, b->text + uimn.ed.sel_a, (uint16_t)n, false);
    } else if (s == SEL_LINES) {
        edit_commit(S);
        text_clip_lines(S, uimn.ed.sel_a, uimn.ed.sel_b);
    }

    return s;
}

void edit_clear(Session *const S)
{
    const uint8_t s = edit_sel();
    uimn.ed.sel_on = false;

    if (s == SEL_PIECE) {
        edit_lb_cut(&uimn.edline, (uint16_t)uimn.ed.sel_a, (uint16_t)uimn.ed.sel_b);
        return;
    }

    if (s != SEL_LINES)
        return;

    edit_commit(S);
    uint32_t line = uimn.ed.sel_a;
    const uint32_t n = uimn.ed.sel_b - line + 1;

    for (uint32_t i = 0; i < n; i++)
        text_delete(S, line);

    changed();

    if (line >= uimn.text.n)
        line = uimn.text.n - 1;

    edit_load(S, line);
    uimn.edline.col = 0;
}

void edit_cut(Session *const S)
{
    edit_copy(S);
    edit_clear(S);
}

/* The clipboard's line at s, to its NL (within most bytes), in uimn.build; its length. */
static uint32_t clip_line(Session *const S, const uint8_t *const s, const uint32_t most)
{
    const uint32_t n = (uint32_t)((const uint8_t *)memchr(s, NL, most) - s);
    memcpy(edit_lb_need(S, &uimn.build, n), s, n);
    return n;
}

void edit_paste(Session *const S)
{
    edit_clear(S);
    const uint16_t len = uimn.text.clip_len;

    if (!len)
        return;
    if (!uimn.text.clip_lines) {
        edit_lb_insert(&uimn.edline, uimn.text.clip, len);
        return;
    }

    edit_commit(S);
    uint32_t line = uimn.ed.line;

    for (uint32_t at = 0; (uint16_t)at < len; line++) {
        const uint32_t n = clip_line(S, uimn.text.clip + at, len - at);
        at += n + 1;
        text_insert(S, line, uimn.build.text, (uint16_t)n);
    }

    changed();
    edit_load(S, line);
}

/* ---- the immediate window */

void edit_imm_load(void)
{
    const uint8_t *const h = uimn.imm.hist[uimn.imm.cur];
    uimn.immline.len = h[0];
    memcpy(uimn.immline.text, h + 1, h[0]);
    uimn.immline.dirty = false;
}

/* im_store: the immediate line back to its line, cut to the window. */
static void imm_store(void)
{
    uint8_t *const h = uimn.imm.hist[uimn.imm.cur];
    const uint16_t n = uimn.immline.len <= IMW - 1 ? uimn.immline.len : IMW - 1;
    h[0] = (uint8_t)n;
    memcpy(h + 1, uimn.immline.text, n);
}

void edit_imm_up(void)
{
    if (!uimn.imm.cur)
        return;

    imm_store();
    uimn.imm.cur--;
    edit_imm_load();
}

void edit_imm_down(void)
{
    if ((uint16_t)(uimn.imm.cur + 1) >= uimn.imm.n)
        return;

    imm_store();
    uimn.imm.cur++;
    edit_imm_load();
}

uint16_t edit_imm_next(Session *const S)
{
    imm_store();
    const uint16_t len = uimn.immline.len;
    memcpy(S->cons.inbuf, uimn.immline.text, len);
    uint16_t i = uimn.imm.cur + 1;

    if (i >= uimn.imm.n) {
        if (i < IMN) {
            uimn.imm.n++;
        } else { /* the oldest dropped */
            memmove(uimn.imm.hist[0], uimn.imm.hist[1], (IMN - 1) * IMW);
            i--;
        }

        uimn.imm.hist[i][0] = 0;
    }

    uimn.imm.cur = i;
    edit_imm_load();
    uimn.immline.col = 0;
    return len;
}
