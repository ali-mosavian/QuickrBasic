/* Console output and INPUT's lines (io/io.asm), standard output's buffer and standard input
   (io/plat.asm's). */
#include "io/console.h"
#include "session.h"
#define GEN_DATA
#include "io/gen.h"
#include "io/channel.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "vm/exec.h"
#include "plat/pfil.h"
#include "plat/pkey.h"
#include "plat/psys.h"

enum : uint8_t { KEY_CTRL_C = 3, KEY_BACKSPACE = '\b', KEY_ENTER = '\r' };
enum { KEYED_MAX = 80 }; /* a keyed line's characters at most */

void cons_init(Session *const S)
{
    S->cons.inbuf = at(allo_carve(S, INMAX + 1));
    S->cons.sbuf = at(allo_carve(S, SBMAX + 1));
    S->cons.stdin_dev = pfil_isdev(PFIL_STDIN);
    chnl_init(S);
}

void cons_putc(Session *const S, const uint8_t c)
{
    if (S->cons.capture & CAP_SBUF) {
        if (S->cons.sblen < SBMAX) /* full: the rest dropped */
            S->cons.sbuf[S->cons.sblen++] = c;

        return;
    }

    if (S->cons.capture & CAP_VERIFY) {
        if (cons_getc(S) != c)
            S->cons.capture |= CAP_DIFF;

        return;
    }

    S->cons.col++;

    if (c == NL)
        S->cons.col = 0;
    if (S->chnl.out)
        chnl_putc(S, c);
    else
        cons_out(S, c);
}

void cons_space(Session *const S) { cons_putc(S, ' '); }

void cons_spaces(Session *const S, const uint8_t n)
{
    for (uint8_t i = 0; i < n; i++)
        cons_space(S);
}

void cons_nl(Session *const S) { cons_putc(S, NL); }

void cons_fresh(Session *const S)
{
    if (S->cons.col)
        cons_nl(S);
}

void cons_uint(Session *const S, uint32_t v)
{
    char d[10];
    uint32_t n = 0;

    do
        d[n++] = (char)('0' + v % 10);
    while (v /= 10);
    for (uint32_t i = n; i > 0; i--)
        cons_putc(S, (uint8_t)d[i - 1]);
}

void cons_int(Session *const S, const int32_t v)
{
    cons_putc(S, v < 0 ? '-' : ' ');
    cons_uint(S, v < 0 ? 0u - (uint32_t)v : (uint32_t)v);
    cons_space(S);
}

void cons_hex4(Session *const S, const uint16_t v)
{
    for (uint32_t i = 0; i < 4; i++)
        cons_putc(S, (uint8_t)"0123456789ABCDEF"[(v >> (12 - 4 * i)) & 15]);
}

void cons_puts(Session *const S, const char *const s)
{
    for (size_t i = 0; s[i]; i++)
        cons_putc(S, (uint8_t)s[i]);
}

/* The lists (io/gen.h): an entry's last byte has LAST set. A byte from REF_HIGH refers to
   message b - REF_HIGH_BASE, one under ' ' (but NL) to (b + REF_LOW_BIAS) & CHAR. */
enum : uint8_t {
    LAST = 0x80,
    CHAR = 0x7F,
    REF_HIGH = 0x5F,
    REF_HIGH_BASE = 0x3C,
    REF_LOW_BIAS = 0x43
};

/* Byte c of an entry: a character, or the message it refers to. */
static void entry_byte(Session *const S, const uint8_t c)
{
    if (c >= REF_HIGH)
        cons_msg(S, c - REF_HIGH_BASE);
    else if (c == NL || c >= ' ')
        cons_putc(S, c);
    else
        cons_msg(S, (c + REF_LOW_BIAS) & CHAR);
}

/* Entry n of the list at list. */
static void nth(Session *const S, const uint8_t *list, const uint8_t n)
{
    for (uint8_t i = 0; i < n; list++)
        if (*list & LAST)
            i++;
    for (;; list++) {
        entry_byte(S, *list & CHAR);

        if (*list & LAST)
            return;
    }
}

void cons_msg(Session *const S, const uint8_t id) { nth(S, cons_msgs, id); }
void cons_kw(Session *const S, const uint8_t tok) { nth(S, cons_kwlist, tok & CHAR); }
void cons_kw1(Session *const S, const uint8_t n) { nth(S, cons_kwlist1, n); }

void cons_prompt(Session *const S)
{
    cons_putc(S, '?');
    cons_space(S);
    cons_read(S);
}

void cons_read(Session *const S)
{
    const int32_t n = cons_getline(S);

    if (n < 0) {
        if (S->chnl.in)
            repl_qberr(S, QB_PAST_END);
        if (!(S->vm.stop & SF_BRK))
            repl_error(S, E_OD);

        S->vm.stop = 0; /* the STOP key: BREAK, no CONT */
        S->vm.cont.line = 0;
        repl_error(S, E_BRK);
    }

    S->cons.inlen = (uint32_t)n;
    S->cons.inbuf[n] = 0;
    S->cons.inpos = -1;

    if (!S->chnl.in) /* the console's: its Enter ended the line */
        S->cons.col = 0;
}

void cons_field(Session *const S)
{
    for (;;) {
        const int32_t b = S->cons.inpos + 1;

        if (b == 0 || S->cons.inbuf[b - 1] == ',') {
            S->cons.inpos = b;
            return;
        }

        if (S->cons.from_file) {
            cons_read(S);
        } else {
            cons_putc(S, '?');
            cons_prompt(S);
        }
    }
}

void cons_flush(Session *const S)
{
    if (!S->cons.oblen)
        return;

    uint32_t put;
    pfil_write(PFIL_STDOUT, S->cons.obuf, S->cons.oblen, &put);
    S->cons.oblen = 0;
}

static void obyte(Session *const S, const uint8_t c)
{
    S->cons.obuf[S->cons.oblen++] = c;

    if (S->cons.oblen >= OBSZ)
        cons_flush(S);
}

void cons_out(Session *const S, const uint8_t c)
{
    if (c != NL) {
        obyte(S, c);
        return;
    }

    obyte(S, '\r');
    obyte(S, '\n');
    cons_flush(S);
}

static void echo(Session *const S, const uint8_t c)
{
    cons_out(S, c);
    cons_flush(S);
}

/* The keyboard's line, key by key, not DOS's line input: that one restarts on Ctrl+C, so INPUT
   could not break. Its length; -1 broken. */
static int32_t keyed_line(Session *const S)
{
    uint32_t n = 0;

    for (;;) {
        const uint16_t key = pkey_wait();
        const uint8_t ch = (uint8_t)key;

        if (!key || ch == KEY_CTRL_C) { /* Ctrl+Break, Ctrl+C */
            S->vm.stop = SF_BRK;
            pkey_flush();
            return -1;
        }

        if (ch == KEY_ENTER) {
            cons_out(S, NL);
            return (int32_t)n;
        }

        if (ch == KEY_BACKSPACE) {
            if (n) {
                n--;
                echo(S, '\b');
                echo(S, ' ');
                echo(S, '\b');
            }

            continue;
        }

        if (ch < ' ' || n >= KEYED_MAX)
            continue;

        S->cons.inbuf[n++] = ch;
        echo(S, ch);
    }
}

/* The input channel's or redirected input's line, to LF (a file's CR LF too); past INMAX left
   out. Its length; -1 at the end with nothing read. */
static int32_t stream_line(Session *const S)
{
    uint32_t n = 0;

    for (;;) {
        const int b = cons_getb(S);

        if (b < 0) {
            if (!n)
                return -1;

            break;
        }

        if (b == '\n')
            break;
        if (b == '\r') {
            if (!S->chnl.in)
                continue;

            chnl_skiplf(S); /* a file's CR LF: one line's end */
            break;
        }

        if (n < INMAX)
            S->cons.inbuf[n++] = (uint8_t)b;
    }

    return (int32_t)n;
}

int32_t cons_getline(Session *const S)
{
    cons_flush(S);

    if (!S->chnl.in && S->cons.stdin_dev)
        return keyed_line(S);

    const int32_t n = stream_line(S);

    if (n >= 0 && !S->chnl.in) /* redirected input: its line echoed */
        cons_out(S, NL);

    return n;
}

uint8_t cons_getc(Session *const S)
{
    if (!S->chnl.in && S->cons.stdin_dev) {
        cons_flush(S);
        return pkey_poll();
    }

    const int b = cons_getb(S);
    return b < 0 ? 0 : (uint8_t)b;
}

int cons_getb(Session *const S)
{
    if (S->chnl.in)
        return chnl_getb(S);

    uint8_t b;
    uint32_t got;

    if (pfil_read(PFIL_STDIN, &b, 1, &got) || got != 1)
        return -1;

    return b;
}

void cons_exit(Session *const S, const uint8_t code)
{
    cons_flush(S);
    psys_exit(code);
}
