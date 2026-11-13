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


void cons_init(void)
{
    cons.inbuf = at(allo_carve(INMAX + 1));
    cons.sbuf = at(allo_carve(SBMAX + 1));
    cons.stdin_dev = pfil_isdev(PFIL_STDIN);
    chnl_init();
}

void cons_putc(uint8_t c)
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

void cons_spaces(uint8_t n)
{
    while (n--)
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
    int n = 0;
    do d[n++] = (char)('0' + v % 10);
    while (v /= 10);
    while (n)
        cons_putc((uint8_t)d[--n]);
}

void cons_int(int32_t v)
{
    cons_putc(v < 0 ? '-' : ' ');
    cons_uint(v < 0 ? 0u - (uint32_t)v : (uint32_t)v);
    cons_space();
}

void cons_hex4(uint16_t v)
{
    for (int s = 12; s >= 0; s -= 4)
        cons_putc((uint8_t)"0123456789ABCDEF"[(v >> s) & 15]);
}

void cons_puts(const char *s)
{
    while (*s)
        cons_putc((uint8_t)*s++);
}

/* Entry n of the list at list. */
static void nth(const uint8_t *list, uint8_t n)
{
    for (; n; list++)
        if (*list & 0x80)
            n--;
    uint8_t b;
    do {
        b = *list++;
        uint8_t c = b & 0x7F;
        if (c >= 0x5F)
            cons_msg(c - 0x3C);
        else if (c == NL || c >= 0x20)
            cons_putc(c);
        else
            cons_msg((c + 0x43) & 0x7F);
    } while (!(b & 0x80));
}

/* The lists (io/gen.h): a byte 5Fh..7Eh refers to message b-3Ch, one under 20h (but NL) to
   (b+43h)&7Fh. */
void cons_msg(uint8_t id) { nth(cons_msgs, id); }
void cons_kw(uint8_t tok) { nth(cons_kwlist, tok & 0x7F); }
void cons_kw1(uint8_t n) { nth(cons_kwlist1, n); }

void cons_prompt(void)
{
    cons_putc('?');
    cons_space();
    cons_read();
}

void cons_read(void)
{
    int32_t n = cons_getline();
    if (n < 0) {
        if (chnl.in)
            chnl_error(62); /* Input past end of file */
        if (!(exec.stop & SF_BRK))
            repl_error(E_OD);
        exec.stop = 0; /* the STOP key: BREAK, no CONT */
        exec.cont_line = 0;
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
        int32_t b = cons.inpos + 1;
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

static void obyte(uint8_t c)
{
    cons.obuf[cons.oblen++] = c;
    if (cons.oblen >= OBSZ)
        cons_flush();
}

void cons_out(uint8_t c)
{
    if (c != NL) {
        obyte(c);
        return;
    }
    obyte(13);
    obyte(10);
    cons_flush();
}

static void echo(uint8_t c)
{
    cons_out(c);
    cons_flush();
}

int32_t cons_getline(void)
{
    cons_flush();
    uint32_t n = 0;
    if (!chnl.in && cons.stdin_dev) {
        /* key by key, not DOS's line input: that one restarts on Ctrl+C, so INPUT could not
           break */
        for (;;) {
            uint16_t key = pkey_wait();
            uint8_t ch = (uint8_t)key;
            if (!key || ch == 3) { /* Ctrl+Break, Ctrl+C */
                exec.stop = SF_BRK;
                pkey_flush();
                return -1;
            }
            if (ch == 13) {
                cons_out(NL);
                return (int32_t)n;
            }
            if (ch == 8) {
                if (n) {
                    n--;
                    echo(8);
                    echo(' ');
                    echo(8);
                }
                continue;
            }
            if (ch < ' ' || n >= 80)
                continue;
            cons.inbuf[n++] = ch;
            echo(ch);
        }
    }
    for (;;) {
        int b = cons_getb();
        if (b < 0) {
            if (!n)
                return -1;
            break;
        }
        if (b == 10)
            break;
        if (b == 13) {
            if (!chnl.in)
                continue;
            chnl_skiplf(); /* a file's CR LF: one line's end */
            break;
        }
        if (n < INMAX) /* past it: left out */
            cons.inbuf[n++] = (uint8_t)b;
    }
    if (!chnl.in && !cons.stdin_dev)
        cons_out(NL);
    return (int32_t)n;
}

uint8_t cons_getc(void)
{
    if (!chnl.in && cons.stdin_dev) {
        cons_flush();
        return pkey_poll();
    }
    int b = cons_getb();
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

void cons_exit(uint8_t code)
{
    cons_flush();
    psys_exit(code);
}
