/* A source file's lines, streamed to the lexer (io/srd.asm). */
#include "io/source.h"
#include "session.h"
#include "io/channel.h"
#include "io/repl.h"

void srcr_phys_start(Session *const S)
{
    S->srcr.cont.st = IN_CODE;
    S->srcr.cont.letters = 0;
    S->srcr.cont.under = false;
    S->srcr.cont.blank = true; /* the line's start */
}

/* A word ended: REM's comment. */
static void word_end(Session *const S)
{
    if (S->srcr.cont.letters == 3 && !memcmp(S->srcr.cont.rem, "rem", 3))
        S->srcr.cont.st = IN_COMMENT;

    S->srcr.cont.letters = 0;
}

void srcr_phys_byte(Session *const S, const uint8_t c)
{
    if (S->srcr.cont.st == IN_COMMENT) /* to the line's end */
        return;
    if (S->srcr.cont.st == IN_STRING) {
        if (c != '"')
            return;

        S->srcr.cont.st = IN_CODE;
    } else if (c == '"') {
        S->srcr.cont.st = IN_STRING;
    } else if (c == '\'') {
        S->srcr.cont.st = IN_COMMENT;
    } else if (is_blank(c)) {
        word_end(S);
        S->srcr.cont.blank = true;
        return;
    } else if (is_letter(c)) { /* a word's letters: REM a comment */
        if (S->srcr.cont.letters < 3)
            S->srcr.cont.rem[S->srcr.cont.letters] = (char)(c | 0x20);

        S->srcr.cont.letters++;
    } else {
        word_end(S);

        if (c == '_') { /* after a blank: perhaps the end */
            S->srcr.cont.under = S->srcr.cont.blank;
            S->srcr.cont.blank = false;
            return;
        }
    }

    S->srcr.cont.under = false;
    S->srcr.cont.blank = false;
}

bool srcr_phys_end(Session *const S)
{
    word_end(S);
    return S->srcr.cont.st != IN_COMMENT &&
           S->srcr.cont.under; /* '_' in a comment: the comment's */
}

/* The window filled on from end: to the logical line's end (done set) or SRCAP; 0 after. */
static void fill(Session *const S)
{
    uint8_t *d = S->srcr.end;

    for (;;) {
        const uint32_t len = (uint32_t)(d - S->cons.inbuf);

        if (len >= SRCAP && (d[-1] != '_' || len >= INMAX)) /* full, a '_' not last: it slides */
            break;

        const int b = cons_getb(S);

        if (b < 0) {
            S->srcr.done = true;
            break;
        }

        S->srcr.any = true;

        if (b == '\n' || b == '\r') {
            if (b == '\r')
                chnl_skiplf(S); /* CR LF: one line's end */
            S->srcr.pend = 0;   /* the line's last blanks go */
            if (!srcr_phys_end(S)) {
                S->srcr.done = true;
                break;
            }

            S->repl.fline++; /* continued: the '_' a blank, then the next line's bytes */
            d[-1] = ' ';
            srcr_phys_start(S);
            continue;
        }

        srcr_phys_byte(S, (uint8_t)b);

        if (S->srcr.cont.st == IN_CODE &&
            is_blank((uint8_t)b)) { /* a blank in code: when a byte follows */
            S->srcr.pend++;
            continue;
        }

        for (; S->srcr.pend; S->srcr.pend--)
            *d++ = ' ';

        *d++ = (uint8_t)b;
    }

    *d = 0;
    S->srcr.end = d;
}

int32_t srcr_line(Session *const S)
{
    S->srcr.end = S->cons.inbuf;
    S->srcr.done = false;
    S->srcr.pend = 0;
    S->srcr.any = false;
    srcr_phys_start(S);
    fill(S);
    return S->srcr.any ? (int32_t)(S->srcr.end - S->cons.inbuf) : -1;
}

bool srcr_more(Session *const S, const uint8_t **const keep)
{
    if (S->srcr.done)
        return false;

    const uint32_t n = (uint32_t)(S->srcr.end - *keep);
    memmove(S->cons.inbuf, *keep, n);
    S->srcr.end = S->cons.inbuf + n;
    fill(S);
    *keep = S->cons.inbuf;
    return true;
}
