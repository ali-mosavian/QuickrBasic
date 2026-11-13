/* The editor's text, the clipboard, files (src/ui/text.asm). */
#include "ui/text.h"
#include "ui/edit.h"
#include "ui/lmap.h"
#include "comp/comp.h"
#include "comp/lexr.h"
#include "comp/list.h"
#include "io/cons.h"
#include "io/srcr.h"
#include "mem/prog.h"
#include "vm/exec.h"
#include "plat/pfil.h"

enum { RDSZ = 4096 }; /* a file's bytes read, written at a time */

static struct {
    PfilHandle h;
    uint16_t err;  /* text_write's DOS error, 0 none */
    uint32_t pos;  /* buf's next byte, its bytes */
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

void text_view(uint32_t first, uint32_t n)
{
    uimn.text.first = first;
    uimn.text.n = n;
}

uint32_t text_rec(uint32_t line) { return lmap_off(line + uimn.text.first); }

/* The line of record rec into dst; its length. */
static uint32_t text_of(uint32_t rec, uint8_t *dst)
{
    const Line *p = prog_line(rec);
    uint32_t n = p->pre;
    memcpy(dst, at(prog_addr(rec + p->len - n)), n);
    if (p->flags & LF_ASIS)
        return n;
    uint32_t sblen = cons.sblen; /* sbuf's user's, kept */
    uint8_t capture = cons.capture, col = cons.col;
    cons.capture = CAP_SBUF;
    cons.sblen = 0;
    list_body(at(prog_addr(rec + HDR)));
    memcpy(dst + n, cons.sbuf, cons.sblen);
    n += cons.sblen;
    cons.capture = capture;
    cons.col = col;
    cons.sblen = sblen;
    return n;
}

uint32_t text_get(uint32_t line, uint8_t *dst) { return text_of(text_rec(line), dst); }

/* The line of len characters at src made a record's: its body in comp_cbuf, its number, its text
   before the body (prog.pre); as typed when uimn.text.asis (used up) or when LIST would spell it
   past an editor line. src is kept till the record is filled. */
static Made make(const uint8_t *src, uint32_t len)
{
    Made m = {.num = NONUM};
    prog.pre = src;
    uint8_t *text = uimn.line; /* a copy, 0-ended, worked on: the lexer changes its text */
    memmove(text, src, len);
    text[len] = 0;
    uint8_t *body = edit_body(text);
    prog.pre_len = (uint32_t)(body - text);
    const uint8_t *p = text; /* its number, 64000 when past */
    while (*p == ' ')
        p++;
    const uint8_t *digits = p;
    uint32_t num = 0;
    for (; p < body && (uint8_t)(*p - '0') <= 9; p++) {
        num = num * 10 + (*p - '0');
        if (num >= 64000)
            num = 64000;
    }
    if (p != digits)
        m.num = (uint16_t)num;
    exec.line = DLINE;
    m.len = (uint32_t)(lexr_line(body, comp_cbuf) - comp_cbuf);
    bool asis = uimn.text.asis;
    uimn.text.asis = false;
    if (!asis) {
        cons.capture = CAP_SBUF; /* LIST's spelling's length */
        cons.sblen = 0;
        list_body(comp_cbuf);
        cons.capture = 0;
        if (cons.sblen + prog.pre_len <= LNMAX)
            return m;
    }
    prog.pre_len = len;
    m.flags = LF_ASIS;
    return m;
}

/* The line of len characters at src stored as text line line: before it (insert), else in its
   place, its mark kept. */
static void put(uint32_t line, const uint8_t *src, uint32_t len, bool insert)
{
    Made m = make(src, len);
    uint32_t rec = lmap_off(line);
    if (!insert) {
        m.flags |= prog_line(rec)->flags & LF_BP;
        prog_cut(rec);
    }
    prog.add_flags = m.flags;
    prog_ins(rec, m.num, comp_cbuf, m.len);
    prog.add_flags = 0;
    uimn.text.changed = true;
    lmap_build();
}

/* The line of len characters at src stored after the rest, the map not made. */
static void add(const uint8_t *src, uint32_t len)
{
    Made m = make(src, len);
    prog.add_flags = m.flags;
    prog.add_at = 0;
    prog_add(m.num, comp_cbuf, m.len);
    prog.add_flags = 0;
}

void text_init(void)
{
    uimn.text.changed = true;
    prog_init();
    add(uimn.build, 0);
    lmap_build();
    text_whole();
}

void text_repl(uint32_t line, const uint8_t *src, uint16_t len)
{
    put(line + uimn.text.first, src, len, false);
}

/* A line added (delta 1) or removed (-1): the hook told. */
static void tell(uint32_t line, int32_t delta)
{
    if (uimn.text.hook)
        uimn.text.hook(line, delta);
}

void text_insert(uint32_t line, const uint8_t *src, uint16_t len)
{
    if (line > uimn.text.n) /* past the view: its end */
        line = uimn.text.n;
    line += uimn.text.first;
    put(line, src, len, true);
    uimn.text.n++;
    tell(line, 1);
}

void text_delete(uint32_t line)
{
    if (uimn.text.n <= 1) { /* the view's last line is emptied */
        text_repl(line, uimn.build, 0);
        return;
    }
    prog_cut(text_rec(line));
    prog_drop();
    uimn.text.changed = true;
    lmap_build();
    uimn.text.n--;
    tell(line + uimn.text.first, -1);
}

bool text_mark(uint32_t line) { return prog_line(text_rec(line))->flags & LF_BP; }

void text_toggle(uint32_t line) { prog_line(text_rec(line))->flags ^= LF_BP; }

void text_unmark_all(void)
{
    for (uint32_t rec = PROGBASE; prog_line(rec)->len; rec += prog_line(rec)->len)
        prog_line(rec)->flags &= ~LF_BP;
}

void text_clip_put(const uint8_t *src, uint16_t len, bool lines)
{
    uimn.text.clip_lines = lines;
    uimn.text.clip_len = len;
    memmove(uimn.text.clip, src, len);
}

void text_clip_lines(uint32_t from, uint32_t to)
{
    uint32_t n = 0;
    for (; from <= to; from++) {
        uint32_t len = text_get(from, uimn.build);
        if (n + len + 1 > 0xFFFF)
            break;
        memcpy(uimn.text.clip + n, uimn.build, len);
        n += len;
        uimn.text.clip[n++] = NL;
    }
    uimn.text.clip_len = (uint16_t)n;
    uimn.text.clip_lines = true;
}

/* The file's next byte, not taken; false: the end. */
static bool peek(uint8_t *c)
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
static bool next(uint8_t *c)
{
    if (!peek(c))
        return false;
    io.pos++;
    return true;
}

/* The line read, len long in uimn.build, stored. */
static void read_line(uint32_t len)
{
    if (!edit_canon(uimn.build, (uint16_t)len))
        uimn.text.asis = true;
    add(uimn.build, len);
    io.lines++;
}

uint16_t text_read(const char *name)
{
    uint16_t e = pfil_open(name, PFIL_READ, &io.h);
    if (e != PFIL_OK)
        return e;
    io.pos = io.cnt = 0;
    uimn.text.changed = true;
    prog_init();
    io.lines = 0;
    uint8_t *const buf = uimn.build;
    uint32_t n = 0; /* the line's characters, its column */
    srcr_phys_start();
    for (;;) {
        uint8_t c;
        if (!next(&c) || c == 0x1A)
            break;
        if (c == 10 || c == NL) {
            uint8_t lf;
            if (c == NL && peek(&lf) && lf == 10)
                io.pos++;
            bool more = srcr_phys_end();
            srcr_phys_start();
            if (!more) {
                read_line(n);
                n = 0;
                continue;
            }
            while (n && buf[n - 1] == ' ') /* continued: the '_' a blank, the next line's */
                n--;                       /* indent dropped */
            if (n && buf[n - 1] == '_')
                buf[n - 1] = ' ';
            while (peek(&c) && (c == ' ' || c == 9))
                io.pos++;
            continue;
        }
        srcr_phys_byte(c);
        if (n >= LNMAX)
            continue;
        if (c != 9) {
            buf[n++] = c;
            continue;
        }
        do
            buf[n++] = ' ';
        while (n & 7 && n < LNMAX);
    }
    if (n || !io.lines) /* the last line, unended */
        read_line(n);
    pfil_close(io.h);
    lmap_build();
    text_whole();
    return PFIL_OK;
}

/* len bytes at p written; a DOS error to io.err. */
static void out(const uint8_t *p, uint32_t len)
{
    uint32_t put;
    uint16_t e;
    if (len && (e = pfil_write(io.h, p, len, &put)) != PFIL_OK)
        io.err = e;
}

static void flush(void)
{
    uint32_t n = io.cnt;
    io.cnt = 0;
    out(io.buf, n);
}

uint16_t text_write(const char *name)
{
    uint16_t e = pfil_create(name, &io.h);
    if (e != PFIL_OK)
        return e;
    io.cnt = 0;
    io.err = 0;
    uint8_t *const buf = uimn.build;
    for (uint32_t line = 0; line < uimn.text.all; line++) {
        uint32_t len = text_of(lmap_off(line), buf);
        buf[len++] = NL;
        buf[len++] = 10;
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
