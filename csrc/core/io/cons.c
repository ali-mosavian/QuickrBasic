/* Console output and INPUT's lines (io/io.asm), standard output's buffer and standard input
   (io/plat.asm's). */
#include "io/cons.h"
#define GEN_DATA
#include "io/gen.h"
#include "io/chnl.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "vm/exec.h"
#include "plat/pfil.h"
#include "plat/pkey.h"
#include "plat/psys.h"

Cons cons;

enum : uint8_t { KEY_CTRL_C = 3, KEY_BACKSPACE = '\b', KEY_ENTER = '\r' };
enum { KEYED_MAX = 80 }; /* a keyed line's characters at most */


void cons_init(void)
{
    cons.inbuf = at(allo_carve(INMAX + 1));
    cons.sbuf = at(allo_carve(SBMAX + 1));
    cons.stdin_dev = pfil_isdev(PFIL_STDIN);
    chnl_init();
}

void cons_putc(const uint8_t c)
{
    if (cons.capture & CAP_SBUF) {
        if (cons.sblen < SBMAX) /* full: the rest dropped */
            cons.sbuf[cons.sblen++] = c;
        return;
    }
    if (cons.capture & CAP_VERIFY) {
        if (cons_getc() != c)
            cons.capture |= CAP_DIFF;
        return;
    }
    cons.col++;
    if (c == NL)
        cons.col = 0;
    if (chnl.out)
        chnl_putc(c);
    else
        cons_out(c);
}

void cons_space(void) { cons_putc(' '); }

void cons_spaces(const uint8_t n)
{
    for (uint8_t i = 0; i < n; i++)
        cons_space();
}

void cons_nl(void) { cons_putc(NL); }

void cons_fresh(void)
{
    if (cons.col)
        cons_nl();
}

void cons_uint(uint32_t v)
{
    char d[10];
    uint32_t n = 0;
    do d[n++] = (char)('0' + v % 10);
    while (v /= 10);
    for (uint32_t i = n; i > 0; i--)
        cons_putc((uint8_t)d[i - 1]);
}

void cons_int(const int32_t v)
{
    cons_putc(v < 0 ? '-' : ' ');
    cons_uint(v < 0 ? 0u - (uint32_t)v : (uint32_t)v);
    cons_space();
}

void cons_hex4(const uint16_t v)
{
    for (uint32_t i = 0; i < 4; i++)
        cons_putc((uint8_t)"0123456789ABCDEF"[(v >> (12 - 4 * i)) & 15]);
}

void cons_puts(const char *const s)
{
    for (size_t i = 0; s[i]; i++)
        cons_putc((uint8_t)s[i]);
}

/* The lists (io/gen.h): an entry's last byte has LAST set. A byte from REF_HIGH refers to
   message b - REF_HIGH_BASE, one under ' ' (but NL) to (b + REF_LOW_BIAS) & CHAR. */
enum : uint8_t { LAST = 0x80, CHAR = 0x7F, REF_HIGH = 0x5F, REF_HIGH_BASE = 0x3C, REF_LOW_BIAS = 0x43 };

/* Byte c of an entry: a character, or the message it refers to. */
static void entry_byte(const uint8_t c)
{
    if (c >= REF_HIGH)
        cons_msg(c - REF_HIGH_BASE);
    else if (c == NL || c >= ' ')
        cons_putc(c);
    else
        cons_msg((c + REF_LOW_BIAS) & CHAR);
}

/* Entry n of the list at list. */
static void nth(const uint8_t *list, const uint8_t n)
{
    for (uint8_t i = 0; i < n; list++)
        if (*list & LAST)
            i++;
    for (;; list++) {
        entry_byte(*list & CHAR);
        if (*list & LAST)
            return;
    }
}

void cons_msg(const uint8_t id) { nth(cons_msgs, id); }
void cons_kw(const uint8_t tok) { nth(cons_kwlist, tok & CHAR); }
void cons_kw1(const uint8_t n) { nth(cons_kwlist1, n); }

void cons_prompt(void)
{
    cons_putc('?');
    cons_space();
    cons_read();
}

void cons_read(void)
{
    const int32_t n = cons_getline();
    if (n < 0) {
        if (chnl.in)
            repl_qberr(QB_PAST_END);
        if (!(vm.stop & SF_BRK))
            repl_error(E_OD);
        vm.stop = 0; /* the STOP key: BREAK, no CONT */
        vm.cont.line = 0;
        repl_error(E_BRK);
    }
    cons.inlen = (uint32_t)n;
    cons.inbuf[n] = 0;
    cons.inpos = -1;
    if (!chnl.in) /* the console's: its Enter ended the line */
        cons.col = 0;
}

void cons_field(void)
{
    for (;;) {
        const int32_t b = cons.inpos + 1;
        if (b == 0 || cons.inbuf[b - 1] == ',') {
            cons.inpos = b;
            return;
        }
        if (cons.from_file) {
            cons_read();
        } else {
            cons_putc('?');
            cons_prompt();
        }
    }
}

void cons_flush(void)
{
    if (!cons.oblen)
        return;
    uint32_t put;
    pfil_write(PFIL_STDOUT, cons.obuf, cons.oblen, &put);
    cons.oblen = 0;
}

static void obyte(const uint8_t c)
{
    cons.obuf[cons.oblen++] = c;
    if (cons.oblen >= OBSZ)
        cons_flush();
}

void cons_out(const uint8_t c)
{
    if (c != NL) {
        obyte(c);
        return;
    }
    obyte('\r');
    obyte('\n');
    cons_flush();
}

static void echo(const uint8_t c)
{
    cons_out(c);
    cons_flush();
}

/* The keyboard's line, key by key, not DOS's line input: that one restarts on Ctrl+C, so INPUT
   could not break. Its length; -1 broken. */
static int32_t keyed_line(void)
{
    uint32_t n = 0;
    for (;;) {
        const uint16_t key = pkey_wait();
        const uint8_t ch = (uint8_t)key;
        if (!key || ch == KEY_CTRL_C) { /* Ctrl+Break, Ctrl+C */
            vm.stop = SF_BRK;
            pkey_flush();
            return -1;
        }
        if (ch == KEY_ENTER) {
            cons_out(NL);
            return (int32_t)n;
        }
        if (ch == KEY_BACKSPACE) {
            if (n) {
                n--;
                echo('\b');
                echo(' ');
                echo('\b');
            }
            continue;
        }
        if (ch < ' ' || n >= KEYED_MAX)
            continue;
        cons.inbuf[n++] = ch;
        echo(ch);
    }
}

/* The input channel's or redirected input's line, to LF (a file's CR LF too); past INMAX left
   out. Its length; -1 at the end with nothing read. */
static int32_t stream_line(void)
{
    uint32_t n = 0;
    for (;;) {
        const int b = cons_getb();
        if (b < 0) {
            if (!n)
                return -1;
            break;
        }
        if (b == '\n')
            break;
        if (b == '\r') {
            if (!chnl.in)
                continue;
            chnl_skiplf(); /* a file's CR LF: one line's end */
            break;
        }
        if (n < INMAX)
            cons.inbuf[n++] = (uint8_t)b;
    }
    return (int32_t)n;
}

int32_t cons_getline(void)
{
    cons_flush();
    if (!chnl.in && cons.stdin_dev)
        return keyed_line();
    const int32_t n = stream_line();
    if (n >= 0 && !chnl.in) /* redirected input: its line echoed */
        cons_out(NL);
    return n;
}

uint8_t cons_getc(void)
{
    if (!chnl.in && cons.stdin_dev) {
        cons_flush();
        return pkey_poll();
    }
    const int b = cons_getb();
    return b < 0 ? 0 : (uint8_t)b;
}

int cons_getb(void)
{
    if (chnl.in)
        return chnl_getb();
    uint8_t b;
    uint32_t got;
    if (pfil_read(PFIL_STDIN, &b, 1, &got) || got != 1)
        return -1;
    return b;
}

void cons_exit(const uint8_t code)
{
    cons_flush();
    psys_exit(code);
}
