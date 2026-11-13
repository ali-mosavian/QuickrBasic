/* A source file's lines, streamed to the lexer (io/srd.asm). */
#include "io/srcr.h"
#include "io/chnl.h"
#include "io/repl.h"

Srcr srcr;

/* Where a physical line's byte is. */
typedef enum : uint8_t { IN_CODE, IN_STRING, IN_COMMENT } Where;

/* Whether the physical line continues, by its bytes. */
static struct {
    Where st;
    uint8_t letters; /* the word's letters so far */
    bool under;     /* the last byte in code a '_' after a blank */
    bool blank;     /* the byte before a blank */
    char rem[3];    /* the word's first letters, small */
} cn;

void srcr_phys_start(void)
{
    cn.st = IN_CODE;
    cn.letters = 0;
    cn.under = false;
    cn.blank = true; /* the line's start */
}

/* A word ended: REM's comment. */
static void word_end(void)
{
    if (cn.letters == 3 && !memcmp(cn.rem, "rem", 3))
        cn.st = IN_COMMENT;
    cn.letters = 0;
}

void srcr_phys_byte(const uint8_t c)
{
    if (cn.st == IN_COMMENT) /* to the line's end */
        return;
    if (cn.st == IN_STRING) {
        if (c != '"')
            return;
        cn.st = IN_CODE;
    } else if (c == '"') {
        cn.st = IN_STRING;
    } else if (c == '\'') {
        cn.st = IN_COMMENT;
    } else if (is_blank(c)) {
        word_end();
        cn.blank = true;
        return;
    } else if (is_letter(c)) { /* a word's letters: REM a comment */
        if (cn.letters < 3)
            cn.rem[cn.letters] = (char)(c | 0x20);
        cn.letters++;
    } else {
        word_end();
        if (c == '_') { /* after a blank: perhaps the end */
            cn.under = cn.blank;
            cn.blank = false;
            return;
        }
    }
    cn.under = false;
    cn.blank = false;
}

bool srcr_phys_end(void)
{
    word_end();
    return cn.st != IN_COMMENT && cn.under; /* '_' in a comment: the comment's */
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
        if (cn.st == IN_CODE && is_blank((uint8_t)b)) { /* a blank in code: when a byte follows */
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
