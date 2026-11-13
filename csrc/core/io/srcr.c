/* A source file's lines, streamed to the lexer (io/srd.asm). */
#include "io/srcr.h"
#include "io/chnl.h"
#include "io/repl.h"

Srcr srcr;



void srcr_phys_start(void)
{
    srcr.cont.st = IN_CODE;
    srcr.cont.letters = 0;
    srcr.cont.under = false;
    srcr.cont.blank = true; /* the line's start */
}

/* A word ended: REM's comment. */
static void word_end(void)
{
    if (srcr.cont.letters == 3 && !memcmp(srcr.cont.rem, "rem", 3))
        srcr.cont.st = IN_COMMENT;
    srcr.cont.letters = 0;
}

void srcr_phys_byte(const uint8_t c)
{
    if (srcr.cont.st == IN_COMMENT) /* to the line's end */
        return;
    if (srcr.cont.st == IN_STRING) {
        if (c != '"')
            return;
        srcr.cont.st = IN_CODE;
    } else if (c == '"') {
        srcr.cont.st = IN_STRING;
    } else if (c == '\'') {
        srcr.cont.st = IN_COMMENT;
    } else if (is_blank(c)) {
        word_end();
        srcr.cont.blank = true;
        return;
    } else if (is_letter(c)) { /* a word's letters: REM a comment */
        if (srcr.cont.letters < 3)
            srcr.cont.rem[srcr.cont.letters] = (char)(c | 0x20);
        srcr.cont.letters++;
    } else {
        word_end();
        if (c == '_') { /* after a blank: perhaps the end */
            srcr.cont.under = srcr.cont.blank;
            srcr.cont.blank = false;
            return;
        }
    }
    srcr.cont.under = false;
    srcr.cont.blank = false;
}

bool srcr_phys_end(void)
{
    word_end();
    return srcr.cont.st != IN_COMMENT && srcr.cont.under; /* '_' in a comment: the comment's */
}

/* The window filled on from end: to the logical line's end (done set) or SRCAP; 0 after. */
static void fill(void)
{
    uint8_t *d = srcr.end;
    for (;;) {
        const uint32_t len = (uint32_t)(d - cons.inbuf);
        if (len >= SRCAP && (d[-1] != '_' || len >= INMAX)) /* full, a '_' not last: it slides */
            break;
        const int b = cons_getb();
        if (b < 0) {
            srcr.done = true;
            break;
        }
        srcr.any = true;
        if (b == '\n' || b == '\r') {
            if (b == '\r')
                chnl_skiplf(); /* CR LF: one line's end */
            srcr.pend = 0; /* the line's last blanks go */
            if (!srcr_phys_end()) {
                srcr.done = true;
                break;
            }
            repl.fline++; /* continued: the '_' a blank, then the next line's bytes */
            d[-1] = ' ';
            srcr_phys_start();
            continue;
        }
        srcr_phys_byte((uint8_t)b);
        if (srcr.cont.st == IN_CODE && is_blank((uint8_t)b)) { /* a blank in code: when a byte follows */
            srcr.pend++;
            continue;
        }
        for (; srcr.pend; srcr.pend--)
            *d++ = ' ';
        *d++ = (uint8_t)b;
    }
    *d = 0;
    srcr.end = d;
}

int32_t srcr_line(void)
{
    srcr.end = cons.inbuf;
    srcr.done = false;
    srcr.pend = 0;
    srcr.any = false;
    srcr_phys_start();
    fill();
    return srcr.any ? (int32_t)(srcr.end - cons.inbuf) : -1;
}

bool srcr_more(const uint8_t **const keep)
{
    if (srcr.done)
        return false;
    const uint32_t n = (uint32_t)(srcr.end - *keep);
    memmove(cons.inbuf, *keep, n);
    srcr.end = cons.inbuf + n;
    fill();
    *keep = cons.inbuf;
    return true;
}
