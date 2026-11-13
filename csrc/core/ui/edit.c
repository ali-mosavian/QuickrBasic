/* Editing: a line buffer, the program window, the selection, the clipboard, the immediate window
   (src/ui/edit.asm). */
#include "ui/edit.h"
#include "comp/comp.h"
#include "comp/decl.h"
#include "comp/lexr.h"
#include "comp/link.h"
#include "comp/list.h"
#include "io/cons.h"
#include "io/diag.h"
#include "io/emsg.h"
#include "ui/dlog.h"
#include "ui/text.h"
#include "ui/uimn.h"
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

static void changed(void)
{
    uimn.text.changed = true;
    uimn.text.unsaved = true;
}

/* ---- a line buffer */

/* le_pad: the text padded with spaces to the cursor. */
static void pad(LineBuf *b)
{
    if (b->col <= b->len)
        return;
    memset(b->text + b->len, ' ', b->col - b->len);
    b->len = b->col;
}

bool edit_lb_type(LineBuf *b, uint8_t c, uint16_t most)
{
    uint16_t at = b->col;
    if (at >= most)
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

void edit_lb_cut(LineBuf *b, uint16_t from, uint16_t to)
{
    if (to > b->len)
        to = b->len;
    b->col = from;
    if (from >= to)
        return;
    memmove(b->text + from, b->text + to, b->len - to);
    b->len -= to - from;
    b->dirty = true;
}

bool edit_lb_bs(LineBuf *b)
{
    if (!b->col)
        return false;
    uint16_t c = b->col - 1;
    edit_lb_cut(b, c, c + 1);
    b->col = c;
    return true;
}

bool edit_lb_del(LineBuf *b)
{
    if (b->col >= b->len)
        return false;
    edit_lb_cut(b, b->col, b->col + 1);
    return true;
}

/* le_indent: the spaces the text starts with. */
static uint16_t indent(const LineBuf *b)
{
    uint16_t n = 0;
    while (n < b->len && b->text[n] == ' ')
        n++;
    return n;
}

void edit_lb_home(LineBuf *b)
{
    uint16_t n = indent(b);
    b->col = n == b->col ? 0 : n;
}

void edit_lb_end(LineBuf *b) { b->col = b->len; }

void edit_lb_left(LineBuf *b)
{
    if (b->col)
        b->col--;
}

void edit_lb_right(LineBuf *b)
{
    if (b->col < LNMAX)
        b->col++;
}

void edit_lb_wleft(LineBuf *b)
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

void edit_lb_wright(LineBuf *b)
{
    uint32_t i = b->col;
    while (i < b->len && b->text[i] != ' ')
        i++;
    while (i < b->len && b->text[i] == ' ')
        i++;
    b->col = (uint16_t)i;
}

bool edit_lb_insert(LineBuf *b, const uint8_t *s, uint16_t n)
{
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

uint8_t *edit_body(uint8_t *text)
{
    while (*text == ' ')
        text++;
    while (is_digit(*text))
        text++;
    while (*text == ' ')
        text++;
    return text;
}

/* ui_chkop: link_walk's: an ERR op's error. */
static void chkop(uint8_t op, const uint8_t *at)
{
    if (op != O_ERR)
        return;
    uc.err = at[1];
    uc.eop = (const ErrOp *)at;
}

bool edit_canon(uint8_t *text, uint16_t len)
{
    uc.src = text;
    uc.n = len;
    text[len] = 0;
    uint8_t *body = edit_body(text);
    if (*body) {
        uc.pre = body;
        memcpy(uimn.line, body, strlen((const char *)body) + 1);
        vm.line = DLINE;
        lexr_line(uimn.line, comp_cbuf);
        decl_line(comp_cbuf); /* a typed SUB known to the lines after */
        if (uimn.text.check) {
            comp.check = true;
            comp_line();
            comp.check = false;
            uc.err = 0;
            link_walk(comp_xbuf, chkop);
            if (uc.err) {
                uc.ecol = edit_dxerr(uc.src, uc.eop);
                return false;
            }
        }
        cons.capture = CAP_SBUF;
        cons.sblen = 0;
        for (const uint8_t *p = text; p < uc.pre; p++)
            cons_putc(*p);
        list_body(comp_cbuf);
        cons.capture = 0;
        if (cons.sblen <= LNMAX)
            return true;
    }
    cons.sblen = uc.n; /* as typed */
    memcpy(cons.sbuf, uc.src, uc.n);
    return true;
}

/* ui_dxerr's: what print prints, captured, to dst wrapped; nothing printed: nothing. */
static uint32_t captured(void (*print)(void))
{
    cons.capture = CAP_SBUF;
    cons.sblen = 0;
    print();
    cons.capture = 0;
    return cons.sblen;
}

static void todo(void) { diag_todo(); }
static void shape(void) { diag_shape(); }

/* '|', then (and its second, when not 0) sep2, then what print prints, wrapped indent in; nothing
   printed: nothing, the separators dropped (@udx_p). */
static uint8_t *part(uint8_t *d, uint8_t sep2, void (*print)(void), uint8_t indent)
{
    uint8_t *start = d;
    *d++ = '|';
    if (sep2)
        *d++ = sep2;
    uint32_t n = captured(print);
    if (!n)
        return start;
    return dlog_wrap(d, cons.sbuf, n, indent);
}

uint16_t edit_dxerr(const uint8_t *text, const ErrOp *op)
{
    const uint8_t *body = edit_body((uint8_t *)text);
    uint32_t bodycol = (uint32_t)(body - text);
    memcpy(cons.inbuf, body, strlen((const char *)body) + 1);
    lexr.all = true; /* the body's bytes to columns */
    lexr_line(cons.inbuf, comp_cbuf);
    lexr.all = false;
    uint16_t col = lexr_colof(op->at);
    uint32_t at = diag_at((const char *)cons.inbuf, col, op->code, op->dg, op->state);

    uint8_t *d = (uint8_t *)uimn.errmsg;
    for (const char *s = emsg_text(op->code); *s; s++) /* its class, the title */
        *d++ = (uint8_t)*s;
    *d++ = '|';
    uint32_t n = captured(diag_what);
    d = dlog_wrap(d, cons.sbuf, n, 0);
    d = part(d, '|', todo, 0);
    d = part(d, 0, shape, 2);
    *d = 0;
    return (uint16_t)(at + bodycol);
}

void edit_emsg(uint8_t code)
{
    const char *s = emsg_text(code);
    memcpy(uimn.errmsg, s, strlen(s) + 1);
}

/* ---- the program window */

void edit_load(uint32_t line)
{
    uimn.ed.line = line;
    uimn.edline.len = (uint16_t)text_get(line, uimn.edline.text);
    uimn.edline.dirty = false;
}

bool edit_commit(void)
{
    LineBuf *b = &uimn.edline;
    if (!b->dirty)
        return true;
    b->dirty = false;
    while (b->len && b->text[b->len - 1] == ' ')
        b->len--;
    changed();
    if (!edit_canon(b->text, b->len)) { /* a syntax error: kept as typed */
        uimn.text.asis = true;
        text_repl(uimn.ed.line, b->text, b->len);
        uimn_selword(uc.ecol); /* the error's word */
        dlog_err(uimn.errmsg);
        return false;
    }
    uimn.text.asis = false;
    b->len = (uint16_t)cons.sblen;
    memcpy(b->text, cons.sbuf, cons.sblen);
    text_repl(uimn.ed.line, b->text, b->len);
    view_commit();
    return true;
}

bool edit_goto(uint32_t line)
{
    if (!edit_commit())
        return false;
    edit_load(line);
    return true;
}

void edit_up(void)
{
    if (uimn.ed.line)
        edit_goto(uimn.ed.line - 1);
}

void edit_down(void)
{
    if (uimn.ed.line + 1 < uimn.text.n)
        edit_goto(uimn.ed.line + 1);
}

void edit_pgup(void)
{
    uint32_t rows = uimn.ed.rows;
    uint32_t line = uimn.ed.line < rows ? 0 : uimn.ed.line - rows + 1;
    uimn.ed.top = uimn.ed.top < rows ? 0 : uimn.ed.top - rows + 1;
    edit_goto(line);
}

void edit_pgdn(void)
{
    uint32_t step = (uint32_t)uimn.ed.rows - 1;
    uint32_t line = uimn.ed.line + step;
    if (line > uimn.text.n - 1)
        line = uimn.text.n - 1;
    uimn.ed.top += step;
    edit_goto(line);
}

void edit_first(void)
{
    uimn.edline.col = 0;
    edit_goto(0);
}

void edit_last(void)
{
    if (edit_goto(uimn.text.n - 1))
        uimn.edline.col = uimn.edline.len;
}

void edit_ensure(void)
{
    UiEdit *e = &uimn.ed;
    if (e->line < e->top)
        e->top = e->line;
    if (e->line >= e->top + e->rows)
        e->top = e->line - e->rows + 1;
    if (e->top >= uimn.text.n)
        e->top = uimn.text.n - 1;
    uint16_t col = uimn.edline.col;
    if (col < e->left)
        e->left = col;
    if (col >= (uint16_t)(e->left + e->width))
        e->left = col - e->width + 1;
}

void edit_enter(void)
{
    LineBuf *b = &uimn.edline;
    uint16_t ind = indent(b);
    if (ind > b->col)
        ind = b->col;
    uimn.ed.indent = ind;
    uint8_t *d = uimn.build; /* the new line: the indent, the rest */
    memset(d, ' ', ind);
    d += ind;
    uint16_t at = b->col;
    if (at < b->len) {
        uint16_t n = b->len - at;
        b->len = at;
        b->dirty = true;
        const uint8_t *s = b->text + at;
        while (n && *s == ' ') {
            s++;
            n--;
        }
        memcpy(d, s, n);
        d += n;
    }
    uimn.ed.nlen = (uint16_t)(d - uimn.build);
    uint8_t view = uimn.ed.view;
    edit_commit();
    if (view != uimn.ed.view) /* a SUB line: its own view, made */
        return;
    uint32_t line = uimn.ed.line + 1;
    text_insert(line, uimn.build, uimn.ed.nlen);
    changed();
    edit_load(line);
    b->col = uimn.ed.indent;
}

void edit_bs(void)
{
    LineBuf *b = &uimn.edline;
    if (edit_lb_bs(b))
        return;
    uint32_t line = uimn.ed.line;
    if (!line)
        return;
    uimn.ed.nlen = b->len; /* this line, kept */
    memcpy(uimn.build, b->text, b->len);
    uint16_t above = (uint16_t)text_get(line - 1, uimn.line);
    if ((uint16_t)(above + uimn.ed.nlen) > LNMAX)
        return;
    text_delete(line);
    edit_load(line - 1);
    b->col = b->len;
    edit_lb_insert(b, uimn.build, uimn.ed.nlen);
    b->col = b->len - uimn.ed.nlen;
    b->dirty = true;
    changed();
}

void edit_del(void)
{
    LineBuf *b = &uimn.edline;
    if (edit_lb_del(b))
        return;
    uint32_t next = uimn.ed.line + 1;
    if (next >= uimn.text.n)
        return;
    uint16_t n = (uint16_t)text_get(next, uimn.build);
    uimn.ed.nlen = n;
    pad(b);
    if ((uint16_t)(b->len + n) > LNMAX)
        return;
    text_delete(next);
    uint16_t col = b->col;
    edit_lb_insert(b, uimn.build, uimn.ed.nlen);
    b->col = col;
    b->dirty = true;
    changed();
}

void edit_tab(void)
{
    LineBuf *b = &uimn.edline;
    while (edit_lb_type(b, ' ', LNMAX) && (b->col & 7))
        ;
}

void edit_ctrly(void)
{
    LineBuf *b = &uimn.edline;
    if (b->dirty) { /* kept as typed: no syntax check */
        b->dirty = false;
        text_repl(uimn.ed.line, b->text, b->len);
    }
    uint32_t line = uimn.ed.line;
    text_clip_lines(line, line);
    text_delete(line);
    changed();
    if (line >= uimn.text.n)
        line = uimn.text.n - 1;
    edit_load(line);
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
    UiEdit *e = &uimn.ed;
    if (!e->sel_on)
        return SEL_NONE;
    if (e->sel_line == e->line) {
        uint32_t a = e->sel_col, b = uimn.edline.col;
        if (a > b) {
            uint32_t t = a;
            a = b;
            b = t;
        }
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

uint8_t edit_copy(void)
{
    uint8_t s = edit_sel();
    if (s == SEL_PIECE) {
        LineBuf *b = &uimn.edline;
        uint32_t end = uimn.ed.sel_b <= b->len ? uimn.ed.sel_b : b->len;
        uint32_t n = end >= uimn.ed.sel_a ? end - uimn.ed.sel_a : 0;
        text_clip_put(b->text + uimn.ed.sel_a, (uint16_t)n, false);
    } else if (s == SEL_LINES) {
        edit_commit();
        text_clip_lines(uimn.ed.sel_a, uimn.ed.sel_b);
    }
    return s;
}

void edit_clear(void)
{
    uint8_t s = edit_sel();
    uimn.ed.sel_on = false;
    if (s == SEL_PIECE) {
        edit_lb_cut(&uimn.edline, (uint16_t)uimn.ed.sel_a, (uint16_t)uimn.ed.sel_b);
        return;
    }
    if (s != SEL_LINES)
        return;
    edit_commit();
    uint32_t line = uimn.ed.sel_a;
    for (uint32_t n = uimn.ed.sel_b - line + 1; n; n--)
        text_delete(line);
    changed();
    if (line >= uimn.text.n)
        line = uimn.text.n - 1;
    edit_load(line);
    uimn.edline.col = 0;
}

void edit_cut(void)
{
    edit_copy();
    edit_clear();
}

void edit_paste(void)
{
    edit_clear();
    uint16_t len = uimn.text.clip_len;
    if (!len)
        return;
    if (!uimn.text.clip_lines) {
        memcpy(uimn.build, uimn.text.clip, len);
        edit_lb_insert(&uimn.edline, uimn.build, len);
        return;
    }
    edit_commit();
    uint32_t line = uimn.ed.line;
    const uint8_t *p = uimn.text.clip;
    while ((uint16_t)(p - uimn.text.clip) < len) {
        uint8_t *d = uimn.build;
        while (*p != NL)
            *d++ = *p++;
        p++;
        text_insert(line, uimn.build, (uint16_t)(d - uimn.build));
        line++;
    }
    changed();
    edit_load(line);
}

/* ---- the immediate window */

void edit_imm_load(void)
{
    const uint8_t *h = uimn.imm.hist[uimn.imm.cur];
    uimn.immline.len = h[0];
    memcpy(uimn.immline.text, h + 1, h[0]);
    uimn.immline.dirty = false;
}

/* im_store: the immediate line back to its line, cut to the window. */
static void imm_store(void)
{
    uint8_t *h = uimn.imm.hist[uimn.imm.cur];
    uint16_t n = uimn.immline.len <= IMW - 1 ? uimn.immline.len : IMW - 1;
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

uint16_t edit_imm_next(void)
{
    imm_store();
    uint16_t len = uimn.immline.len;
    memcpy(cons.inbuf, uimn.immline.text, len);
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
