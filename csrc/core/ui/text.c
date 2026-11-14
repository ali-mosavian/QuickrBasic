/* The editor's text, the clipboard, files (src/ui/text.asm). */
#include "ui/text.h"
#include "session.h"
#include "ui/edit.h"
#include "ui/linemap.h"
#include "comp/comp.h"
#include "comp/lexer.h"
#include "comp/listing.h"
#include "io/console.h"
#include "io/source.h"
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

/* The line of record rec into dst; its length. */
static uint32_t text_of(Session *const S, const uint32_t rec, uint8_t *const dst)
{
    const Line *const p = prog_line(&S->prog, rec);
    const uint32_t pre = p->pre;
    memcpy(dst, at(prog_addr(&S->prog, rec + p->len - pre)), pre);

    if (p->flags & LF_ASIS)
        return pre;

    const uint32_t sblen = S->cons.sblen; /* sbuf's user's, kept */
    const uint8_t capture = S->cons.capture, col = S->cons.col;
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    list_body(S, at(prog_addr(&S->prog, rec + HDR)));
    const uint32_t n = pre + S->cons.sblen;
    memcpy(dst + pre, S->cons.sbuf, S->cons.sblen);
    S->cons.capture = capture;
    S->cons.col = col;
    S->cons.sblen = sblen;
    return n;
}

uint32_t text_get(Session *const S, const uint32_t line, uint8_t *const dst)
{
    return text_of(S, text_rec(S, line), dst);
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
    list_body(S, S->comp.cbuf);
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
    uint8_t *const text = uimn.line; /* a copy, 0-ended, worked on: the lexer changes its text */
    memmove(text, src, len);
    text[len] = 0;
    uint8_t *const body = edit_body(text);
    S->prog.pre_len = (uint32_t)(body - text);
    m.num = number(text, S->prog.pre_len);
    S->vm.line = DLINE;
    m.len = (uint32_t)(lexr_line(S, body, S->comp.cbuf) - S->comp.cbuf);
    const bool asis = uimn.text.asis;
    uimn.text.asis = false;

    if (!asis && spelling_fits(S, S->prog.pre_len))
        return m;

    S->prog.pre_len = len;
    m.flags = LF_ASIS;
    return m;
}

/* The line of len characters at src stored as text line line: before it (insert), else in its
   place, its mark kept. */
static void put(
    Session *const S,
    const uint32_t line,
    const uint8_t *const src,
    const uint32_t len,
    const bool insert
)
{
    Made m = make(S, src, len);
    const uint32_t rec = lmap_off(S, line);

    if (!insert) {
        m.flags |= prog_line(&S->prog, rec)->flags & LF_BP;
        prog_cut(S, rec);
    }

    S->prog.add_flags = m.flags;
    prog_ins(S, rec, m.num, S->comp.cbuf, m.len);
    S->prog.add_flags = 0;
    uimn.text.changed = true;
    lmap_build(S);
}

/* The line of len characters at src stored after the rest, the map not made. */
static void add(Session *const S, const uint8_t *const src, const uint32_t len)
{
    const Made m = make(S, src, len);
    S->prog.add_flags = m.flags;
    S->prog.add_at = 0;
    prog_add(S, m.num, S->comp.cbuf, m.len);
    S->prog.add_flags = 0;
}

void text_init(Session *const S)
{
    uimn.text.changed = true;
    prog_init(S);
    add(S, uimn.build, 0);
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
        text_repl(S, line, uimn.build, 0);
        return;
    }

    prog_cut(S, text_rec(S, line));
    prog_drop(S);
    uimn.text.changed = true;
    lmap_build(S);
    uimn.text.n--;
    tell(line + uimn.text.first, -1);
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

void text_clip_put(const uint8_t *const src, const uint16_t len, const bool lines)
{
    uimn.text.clip_lines = lines;
    uimn.text.clip_len = len;
    memmove(uimn.text.clip, src, len);
}

void text_clip_lines(Session *const S, const uint32_t from, const uint32_t to)
{
    uint32_t n = 0;

    for (uint32_t line = from; line <= to; line++) {
        const uint32_t len = text_get(S, line, uimn.build);

        if (n + len + 1 > UINT16_MAX)
            break;

        memcpy(uimn.text.clip + n, uimn.build, len);
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

/* The line read, len long in uimn.build, stored. */
static void read_line(Session *const S, const uint32_t len)
{
    if (!edit_canon(S, uimn.build, (uint16_t)len))
        uimn.text.asis = true;

    add(S, uimn.build, len);
    io.lines++;
}

/* A line's end c (CR, CR LF, LF) after n characters in uimn.build: the line stored, or, continued
   with '_', the '_' a blank and the next line's indent dropped; the characters kept. */
static uint32_t line_end(Session *const S, const uint8_t c, uint32_t n)
{
    uint8_t *const buf = uimn.build;
    uint8_t after;

    if (c == NL && peek(&after) && after == LF)
        io.pos++;

    const bool more = srcr_phys_end(S);
    srcr_phys_start(S);

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
    uint8_t *const buf = uimn.build;
    srcr_phys_byte(S, c);

    if (n >= LNMAX)
        return n;
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
    prog_init(S);
    io.lines = 0;
    uint32_t n = 0; /* the line's characters, its column */
    srcr_phys_start(S);
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

uint16_t text_write(Session *const S, const char *const name)
{
    const uint16_t e = pfil_create(name, &io.h);

    if (e != PFIL_OK)
        return e;

    io.cnt = 0;
    io.err = 0;
    uint8_t *const buf = uimn.build;

    for (uint32_t line = 0; line < uimn.text.all; line++) {
        uint32_t len = text_of(S, lmap_off(S, line), buf);
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
