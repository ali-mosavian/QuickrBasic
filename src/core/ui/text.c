/* The editor's text: the program store's lines, the clipboard, and reading and writing files.
 *
 * # How it works
 * The text is the program store: a line a record, uimn.text.all of them, at least one, in order,
 * each found by the line map (linemap.h). A line is its record's pre bytes (its indent and number
 * as typed), then its body as LIST spells it. An LF_ASIS line is its pre alone: a line with a
 * syntax error, kept as typed; uimn.text.asis asks it of the next line stored. A line's mark (a
 * breakpoint) is its LF_BP.
 *
 * The editor sees a view: uimn.text.n lines from line uimn.text.first, numbered from 0 (a
 * procedure's lines, view.h). Lines below are the view's unless said. A line added or removed goes
 * to uimn.text.hook (the text's line, 1 or -1). Storing a line can raise Out of memory.
 *
 * The text is each of the program's files, the main's first (mem/module.h): a line is its
 * record's file's, its names its module's. A line stored in place of another is that one's file's;
 * one added is its view's neighbour's, the line it goes before, past the view's end its last. A
 * module changed since saved is unsaved (text_unsaved); uimn.text.unsaved: any is.
 *
 * The clipboard is uimn.text.clip_len bytes of uimn.text.clip: whole lines (uimn.text.clip_lines,
 * each ended by NL) or a piece of one.
 *
 * text_read and text_write go RDSZ bytes at a time. Reading splits physical lines as the source
 * reader does (CR LF, LF, '_' continuation).
 *
 * # asm structure (retired/asm32/ui/text.asm)
 * | asm | here | |
 * |---|---|---|
 * | tx_init | text_init | MIRROR |
 * | tx_whole, tx_view | text_whole, text_view | MIRROR |
 * | tx_rec, tx_get | text_rec, text_get | MIRROR |
 * | tx_repl, tx_insl, tx_dell | text_repl, text_insert, text_delete | MIRROR |
 * | tx_mark, tx_mtog, tx_mclr | text_mark, text_toggle, text_unmark_all | MIRROR |
 * | cb_put, cb_lines | text_clip_put, text_clip_lines | MIRROR |
 * | tx_read, tx_write | text_read, text_write | MIRROR; D1 |
 * | tx_text, tx_make, tx_put, tx_add, tx_tell, rd_peek, rd_byte, wr_flush, wr_out | statics | MIRROR |
 * | g_rdh, g_rderr, g_rdpos, g_rdcnt, g_rdn, g_rdb, RDSZ | statics, RDSZ | MIRROR |
 * | cn_init, cn_byte, cn_eol | srcr_phys_start, srcr_phys_byte, srcr_phys_end (io) | MOVED |
 * | g_ui.tx.*, g_clpb, g_ui.tx.hl_len | uimn.text.*, uimn.help_len (ui.h) | MOVED; D2 |
 *
 * - D1. They return PFIL_OK or the DOS error, not CF and AX.
 * - D2. The clipboard is uimn.text.clip, not carved from the arena: no Addr reaches it.
 */
#include "ui/text.h"
#include "util/grow.h"
#include "session.h"
#include "ui/edit.h"
#include "ui/linemap.h"
#include "comp/comp.h"
#include "comp/lexer.h"
#include "comp/listing.h"
#include "io/console.h"
#include "io/source.h"
#include "mem/module.h"
#include "mem/program.h"
#include "vm/exec.h"
#include "plat/pfil.h"

enum { RDSZ = 4096 };                      /* a file's bytes read, written at a time */
enum : uint8_t { LF = 10, CH_EOF = 0x1A }; /* NL is CR */

static struct {
    PfilHandle h;
    uint16_t err; /* text_write's DOS error, 0 none */
    uint32_t pos; /* buf's next byte, its bytes */
    uint32_t cnt;
    uint32_t lines; /* text_read's */
    uint8_t buf[RDSZ];
} io;

static struct {
    bool *unsaved; /* by module */
    uint32_t cap;
} mods;

/* A record's made by make(): its number, body (in comp_cbuf), its flags. */
typedef struct {
    uint16_t num;
    uint32_t len;
    uint8_t flags;
} Made;

void text_whole(void)
{
    uimn.text.first = 0;
    uimn.text.n = uimn.text.all;
}

void text_view(const uint32_t first, const uint32_t n)
{
    uimn.text.first = first;
    uimn.text.n = n;
}

uint32_t text_rec(Session *const S, const uint32_t line)
{
    return lmap_off(S, line + uimn.text.first);
}

uint32_t text_record(Session *const S, const uint32_t rec, LineBuf *const b)
{
    const Line *const p = prog_line(&S->prog, rec);
    const uint32_t pre = p->pre;
    memcpy(edit_lb_need(S, b, pre), at(prog_addr(&S->prog, rec + p->len - pre)), pre);

    if (p->flags & LF_ASIS)
        return pre;

    const uint32_t sblen = S->cons.sblen; /* sbuf's user's, kept */
    const uint8_t capture = S->cons.capture, col = S->cons.col;
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    list_body(&S->comp, at(prog_addr(&S->prog, rec + HDR)));
    const uint32_t n = pre + S->cons.sblen;
    memcpy(edit_lb_need(S, b, n) + pre, S->cons.sbuf, S->cons.sblen);
    S->cons.capture = capture;
    S->cons.col = col;
    S->cons.sblen = sblen;
    return n;
}

uint32_t text_get(Session *const S, const uint32_t line, LineBuf *const b)
{
    return text_record(S, text_rec(S, line), b);
}

/* The line number text's first end characters start with, past blanks: LNUM_LIMIT when past;
   NONUM none. */
static uint16_t number(const uint8_t *const text, const uint32_t end)
{
    uint32_t k = 0;

    while (text[k] == ' ')
        k++;

    const uint32_t digits = k;
    uint32_t num = 0;

    for (; k < end && is_digit(text[k]); k++) {
        num = num * 10 + (text[k] - '0');

        if (num >= LNUM_LIMIT)
            num = LNUM_LIMIT;
    }

    return k != digits ? (uint16_t)num : NONUM;
}

/* LIST's spelling of the body in comp_cbuf, after pre_len characters, fits an editor line. */
static bool spelling_fits(Session *const S, const uint32_t pre_len)
{
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    list_body(&S->comp, S->comp.cbuf);
    S->cons.capture = 0;
    return S->cons.sblen + pre_len <= LNMAX;
}

/* The line of len characters at src made a record's: its body in comp_cbuf, its number, its text
   before the body (prog.pre); as typed when uimn.text.asis (used up) or when LIST would spell it
   past an editor line. src is kept till the record is filled. */
static Made make(Session *const S, const uint8_t *const src, const uint32_t len)
{
    Made m = {};

    S->prog.pre = src;
    uint8_t *const text = edit_lb_need(S, &uimn.line, len); /* a copy, 0-ended, worked on: the
                                                                 lexer changes its text */
    memmove(text, src, len);
    text[len] = 0;
    uint8_t *const body = edit_body(text);
    S->prog.pre_len = (uint32_t)(body - text);
    m.num = number(text, S->prog.pre_len);
    S->vm.line = DLINE;
    m.len = (uint32_t)(lexr_line(&S->lexr, body, S->comp.cbuf) - S->comp.cbuf);
    const bool asis = uimn.text.asis;
    uimn.text.asis = false;

    if (!asis && spelling_fits(S, S->prog.pre_len))
        return m;

    S->prog.pre_len = len;
    m.flags = LF_ASIS;
    return m;
}

/* File file's module changed since saved. Out of memory. */
static void touch(Session *const S, const uint16_t file)
{
    const uint32_t m = modu_of(&S->modu, file);

    if (!GROW(mods.unsaved, mods.cap, m + 1))
        repl_error(S, E_OM);

    mods.unsaved[m] = true;
    uimn.text.unsaved = true;
}

/* The file of a line added before text line line: its view's neighbour's. */
static uint16_t neighbour(Session *const S, const uint32_t line)
{
    const bool past = line >= uimn.text.first + uimn.text.n && line > uimn.text.first;
    const uint32_t near = past ? line - 1 : line;
    return near < uimn.text.all ? prog_line(&S->prog, lmap_off(S, near))->file : 0;
}

/* The line of len characters at src stored as text line line: before it (insert), else in its
   place, its mark, file and source line kept; its names its file's module's. */
static void put(
    Session *const S,
    const uint32_t line,
    const uint8_t *const src,
    const uint32_t len,
    const bool insert
)
{
    const Line *const old = prog_line(&S->prog, lmap_off(S, line));
    const uint16_t file = insert ? neighbour(S, line) : old->file, src_line = insert ? 0 : old->src;
    const uint8_t mark = insert ? 0 : old->flags & LF_BP;
    const uint32_t module = S->repl.module;
    S->repl.module = modu_of(&S->modu, file);
    Made m = make(S, src, len);
    S->repl.module = module;
    touch(S, file);
    const uint32_t rec = lmap_off(S, line);

    if (!insert)
        prog_cut(&S->prog, rec);

    S->prog.add_flags = m.flags | mark;
    S->prog.add_file = file;
    S->prog.add_src = src_line;
    prog_ins(&S->prog, rec, m.num, S->comp.cbuf, m.len);
    S->prog.add_flags = 0;
    S->prog.add_file = 0;
    S->prog.add_src = 0;
    uimn.text.changed = true;
    lmap_build(S);
}

/* The line of len characters at src stored after the rest, the map not made. */
static void add(Session *const S, const uint8_t *const src, const uint32_t len)
{
    const Made m = make(S, src, len);
    S->prog.add_flags = m.flags;
    S->prog.add_at = 0;
    prog_add(&S->prog, m.num, S->comp.cbuf, m.len);
    S->prog.add_flags = 0;
}

void text_init(Session *const S)
{
    uimn.text.changed = true;
    session_new(S);
    add(S, (const uint8_t *)"", 0);
    lmap_build(S);
    text_whole();
}

void text_repl(Session *const S, const uint32_t line, const uint8_t *const src, const uint16_t len)
{
    put(S, line + uimn.text.first, src, len, false);
}

/* A line added (delta 1) or removed (-1): the hook told. */
static void tell(const uint32_t line, const int32_t delta)
{
    if (uimn.text.hook)
        uimn.text.hook(line, delta);
}

void text_insert(
    Session *const S,
    const uint32_t line,
    const uint8_t *const src,
    const uint16_t len
)
{
    const uint32_t in_view = line > uimn.text.n ? uimn.text.n : line; /* past the view: its end */
    const uint32_t whole = in_view + uimn.text.first;
    put(S, whole, src, len, true);
    uimn.text.n++;
    tell(whole, 1);
}

void text_delete(Session *const S, const uint32_t line)
{
    if (uimn.text.n <= 1) { /* the view's last line is emptied */
        text_repl(S, line, (const uint8_t *)"", 0);
        return;
    }

    touch(S, prog_line(&S->prog, text_rec(S, line))->file);
    prog_cut(&S->prog, text_rec(S, line));
    session_drop(S);
    uimn.text.changed = true;
    lmap_build(S);
    uimn.text.n--;
    tell(line + uimn.text.first, -1);
}

uint32_t text_module(Session *const S, const uint32_t line)
{
    return modu_of(&S->modu, prog_line(&S->prog, lmap_off(S, line))->file);
}

bool text_unsaved(const uint32_t module) { return module < mods.cap && mods.unsaved[module]; }

void text_saved(const uint32_t module)
{
    if (module < mods.cap)
        mods.unsaved[module] = false;

    uimn.text.unsaved = false;

    for (uint32_t m = 0; m < mods.cap; m++)
        uimn.text.unsaved |= mods.unsaved[m];
}

void text_clean(void)
{
    if (mods.cap)
        memset(mods.unsaved, 0, mods.cap * sizeof *mods.unsaved);

    uimn.text.unsaved = false;
}

bool text_mark(Session *const S, const uint32_t line)
{
    return prog_line(&S->prog, text_rec(S, line))->flags & LF_BP;
}

void text_toggle(Session *const S, const uint32_t line)
{
    prog_line(&S->prog, text_rec(S, line))->flags ^= LF_BP;
}

void text_unmark_all(Session *const S)
{
    for (uint32_t rec = PROGBASE; prog_line(&S->prog, rec)->len;

         rec += prog_line(&S->prog, rec)->len)
        prog_line(&S->prog, rec)->flags &= ~LF_BP;
}

/* The clipboard holds len bytes. Out of memory. */
static uint8_t *clip_room(Session *const S, const uint32_t len)
{
    if (!GROW(uimn.text.clip, uimn.text.clip_cap, len))
        repl_error(S, E_OM);

    return uimn.text.clip;
}

void text_clip_put(Session *const S, const uint8_t *const src, const uint16_t len, const bool lines)
{
    memmove(clip_room(S, len), src, len);
    uimn.text.clip_lines = lines;
    uimn.text.clip_len = len;
}

void text_clip_lines(Session *const S, const uint32_t from, const uint32_t to)
{
    uint32_t n = 0;

    for (uint32_t line = from; line <= to; line++) {
        const uint32_t len = text_get(S, line, &uimn.build);

        if (n + len + 1 > UINT16_MAX)
            break;

        memcpy(clip_room(S, n + len + 1) + n, uimn.build.text, len);
        n += len;
        uimn.text.clip[n++] = NL;
    }

    uimn.text.clip_len = (uint16_t)n;
    uimn.text.clip_lines = true;
}

/* The file's next byte, not taken; false: the end. */
static bool peek(uint8_t *const c)
{
    if (io.pos >= io.cnt) {
        uint32_t got;

        if (pfil_read(io.h, io.buf, RDSZ, &got) != PFIL_OK || !got)
            return false;

        io.cnt = got;
        io.pos = 0;
    }

    *c = io.buf[io.pos];
    return true;
}

/* The file's next byte, taken. */
static bool next(uint8_t *const c)
{
    if (!peek(c))
        return false;

    io.pos++;
    return true;
}

/* The line read, len long in uimn.build, stored: as edit_canon tokenized and spelled it, else
   made again. */
static void read_line(Session *const S, const uint32_t len)
{
    uint8_t *const text = uimn.build.text;
    uint32_t lexed;

    if (!edit_canon(S, text, (uint16_t)len, &lexed))
        uimn.text.asis = true;

    if (lexed) {
        S->prog.pre = text;
        S->prog.pre_len = (uint32_t)(edit_body(text) - text);
        S->prog.add_at = 0;
        prog_add(&S->prog, number(text, S->prog.pre_len), S->comp.cbuf, lexed);
    } else {
        add(S, text, len);
    }

    io.lines++;
}

/* A line's end c (CR, CR LF, LF) after n characters in uimn.build: the line stored, or, continued
   with '_', the '_' a blank and the next line's indent dropped; the characters kept. */
static uint32_t line_end(Session *const S, const uint8_t c, uint32_t n)
{
    uint8_t *const buf = uimn.build.text;
    uint8_t after;

    if (c == NL && peek(&after) && after == LF)
        io.pos++;

    const bool more = srcr_phys_end(&S->srcr.cont);
    srcr_phys_start(&S->srcr.cont);

    if (!more) {
        read_line(S, n);
        return 0;
    }

    while (n && buf[n - 1] == ' ')
        n--;
    if (n && buf[n - 1] == '_')
        buf[n - 1] = ' ';
    while (peek(&after) && is_blank(after))
        io.pos++;

    return n;
}

/* Character c after n in uimn.build: a tab spaces to the next stop, past LNMAX cut; the
   characters now. */
static uint32_t line_char(Session *const S, const uint8_t c, uint32_t n)
{
    srcr_phys_byte(&S->srcr.cont, c);

    if (n >= LNMAX)
        return n;

    uint8_t *const buf = edit_lb_need(S, &uimn.build, n + TAB_STOP);

    if (c != CH_TAB) {
        buf[n++] = c;
        return n;
    }

    do
        buf[n++] = ' ';
    while (n % TAB_STOP && n < LNMAX);

    return n;
}

uint16_t text_read(Session *const S, const char *const name)
{
    const uint16_t e = pfil_open(name, PFIL_READ, &io.h);

    if (e != PFIL_OK)
        return e;

    io.pos = io.cnt = 0;
    uimn.text.changed = true;
    session_new(S);
    io.lines = 0;
    uint32_t n = 0; /* the line's characters, its column */
    srcr_phys_start(&S->srcr.cont);
    uint8_t c;

    while (next(&c) && c != CH_EOF)
        n = c == LF || c == NL ? line_end(S, c, n) : line_char(S, c, n);
    if (n || !io.lines) /* the last line, unended */
        read_line(S, n);

    pfil_close(io.h);
    lmap_build(S);
    text_whole();
    return PFIL_OK;
}

/* len bytes at p written; a DOS error to io.err. */
static void out(const uint8_t *const p, const uint32_t len)
{
    if (!len)
        return;

    uint32_t wrote;
    const uint16_t e = pfil_write(io.h, p, len, &wrote);

    if (e != PFIL_OK)
        io.err = e;
}

static void flush(void)
{
    const uint32_t n = io.cnt;
    io.cnt = 0;
    out(io.buf, n);
}

uint16_t text_write(Session *const S, const char *const name, const uint32_t module)
{
    const uint16_t e = pfil_create(name, &io.h);

    if (e != PFIL_OK)
        return e;

    io.cnt = 0;
    io.err = 0;

    for (uint32_t line = 0; line < uimn.text.all; line++) {
        if (text_module(S, line) != module)
            continue;

        uint32_t len = text_record(S, lmap_off(S, line), &uimn.build);
        uint8_t *const buf = uimn.build.text;
        buf[len++] = NL;
        buf[len++] = LF;

        if (io.cnt + len > RDSZ) {
            flush();

            if (len > RDSZ) { /* longer than the buffer: as it is */
                out(buf, len);
                continue;
            }
        }

        memcpy(io.buf + io.cnt, buf, len);
        io.cnt += len;
    }

    flush();
    pfil_close(io.h);
    return io.err;
}
