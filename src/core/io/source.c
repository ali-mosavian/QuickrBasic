/* A source file's lines, streamed to the lexer.
 *
 * # How it works
 * A logical line is physical lines joined where one ends in a blank and '_', outside a string and
 * a comment (BC's continuation); the '_' becomes a space. The window, cons.inbuf, holds what of it
 * fits, 0 after. srcr_more slides it for the lexer and reads on, so a line is as long as the store
 * takes. Blanks in code wait until a byte follows them: a line's last ones go.
 *
 * srcr_phys_* decide, byte by byte, whether a physical line continues (the editor uses them too):
 * a line starts; its next byte; it ended, true when it continues. SrcrCont tracks where the byte
 * is (Where: code, string, comment), the word's characters so far (REM starts a comment), and whether
 * the last code byte was a '_' after a blank.
 *
 * # Example
 *   x = 1 + _      ->  x = 1 + 2
 *       2
 *
 * # asm structure (retired/asm32/io/srd.asm)
 * | asm | here | |
 * |---|---|---|
 * | sr_line | srcr_line | MIRROR; -1 at the end |
 * | sr_more | srcr_more | MIRROR; takes a LexrMore keep |
 * | sr_mem |  | D1 |
 * | sr_fill | static | MIRROR |
 * | cn_init, cn_byte, cn_eol | srcr_phys_start, srcr_phys_byte, srcr_phys_end | MIRROR |
 * | g_srfe | srcr.end | D2 |
 * | g_srpend, g_srend, g_srany | srcr.pend, srcr.done, srcr.any | MIRROR |
 * | g_srmem, g_cn* | static | MIRROR |
 *
 * - D1. Dropped: no caller.
 * - D2. A pointer into cons.inbuf.
 */
#include "io/source.h"
#include <stddef.h>
#include "io/channel.h"

void srcr_phys_start(SrcrCont *const cn)
{
    cn->st = IN_CODE;
    cn->chars = 0;
    cn->under = false;
    cn->blank = true; /* the line's start */
}

/* A word ended: REM's comment. */
static void srcr_word_end(SrcrCont *const cn)
{
    if (cn->chars == 3 && !memcmp(cn->rem, "rem", 3))
        cn->st = IN_COMMENT;

    cn->chars = 0;
}

void srcr_phys_byte(SrcrCont *const cn, const uint8_t c)
{
    if (cn->st == IN_COMMENT) /* to the line's end */
        return;
    if (cn->st == IN_STRING) {
        if (c != '"')
            return;

        cn->st = IN_CODE;
    } else if (c == '"') {
        cn->st = IN_STRING;
    } else if (c == '\'') {
        cn->st = IN_COMMENT;
    } else if (is_blank(c)) {
        srcr_word_end(cn);
        cn->blank = true;
        return;
    } else if (cn->chars ? is_name_char(c) : is_letter(c)) { /* a word's: REM a comment */
        if (cn->chars < 3)
            cn->rem[cn->chars] = (char)(c | 0x20);

        cn->chars++;
    } else {
        srcr_word_end(cn);

        if (c == '_') { /* after a blank: perhaps the end */
            cn->under = cn->blank;
            cn->blank = false;
            return;
        }
    }

    cn->under = false;
    cn->blank = false;
}

bool srcr_phys_end(SrcrCont *const cn)
{
    srcr_word_end(cn);
    return cn->st != IN_COMMENT && cn->under; /* '_' in a comment: the comment's */
}

/* The window's room for n bytes at d: d where it is now. */
static uint8_t *srcr_room(Srcr *const sr, uint8_t *const d, const uint32_t n)
{
    const uint32_t off = (uint32_t)(d - sr->cons->inbuf);

    if (off + n <= sr->cons->incap)
        return d;

    return cons_inroom(sr->cons, off + n) + off;
}

/* The window filled on from end: to the logical line's end (done set) or SRCAP; 0 after. */
static void srcr_fill(Srcr *const sr)
{
    uint8_t *d = sr->src.end;

    for (;;) {
        const uint32_t len = (uint32_t)(d - sr->cons->inbuf);

        if (len >= SRCAP && (d[-1] != '_' || len >= INMAX)) /* full, a '_' not last: it slides */
            break;

        const int b = sr->chnl->in ? chnl_getb(sr->chnl) : cons_getb(sr->cons);

        if (b < 0) {
            sr->done = true;
            break;
        }

        sr->any = true;

        if (b == '\n' || b == '\r') {
            if (b == '\r')
                chnl_skiplf(sr->chnl); /* CR LF: one line's end */
            sr->pend = 0;              /* the line's last blanks go */
            if (!srcr_phys_end(&sr->cont)) {
                sr->done = true;
                break;
            }

            ++*sr->fline; /* continued: the '_' a blank, then the next line's bytes */
            d[-1] = ' ';
            srcr_phys_start(&sr->cont);
            continue;
        }

        srcr_phys_byte(&sr->cont, (uint8_t)b);

        if (sr->cont.st == IN_CODE &&
            is_blank((uint8_t)b)) { /* a blank in code: when a byte follows */
            sr->pend++;
            continue;
        }

        d = srcr_room(sr, d, sr->pend + 2); /* the 0 after them */

        for (; sr->pend; sr->pend--)
            *d++ = ' ';

        *d++ = (uint8_t)b;
    }

    d = srcr_room(sr, d, 1);
    *d = 0;
    sr->src.end = d;
}

/* A physical line whole in the file's read-ahead, its last byte neither a blank nor '_': copied
   as it is, true. False, nothing read: srcr_fill's, byte by byte. */
static bool srcr_whole(Srcr *const sr)
{
    uint32_t n;
    const uint8_t *const p = chnl_span(sr->chnl, &n);
    /* the LF, then no CR nor ^Z before it but a CR LF's: memchr's scans, the CPU's (string.asm) */
    const uint8_t *const lf = memchr(p, '\n', n);

    if (!lf)
        return false;

    uint32_t k = (uint32_t)(lf - p);

    if (k && p[k - 1] == '\r')
        k--;
    if (memchr(p, '\r', k) || memchr(p, CTRL_Z, k) || k >= SRCAP ||
        (k && (is_blank(p[k - 1]) || p[k - 1] == '_')))
        return false;

    memcpy(cons_inroom(sr->cons, k + 1), p, k);
    sr->cons->inbuf[k] = 0;
    sr->src.end = sr->cons->inbuf + k;
    const bool cr = p[k] == '\r';
    chnl_skip(sr->chnl, k + 1);

    if (cr)
        chnl_skiplf(sr->chnl);

    sr->done = sr->any = true;
    return true;
}

int32_t srcr_line(Srcr *const sr)
{
    if (sr->chnl->in && srcr_whole(sr))
        return (int32_t)(sr->src.end - sr->cons->inbuf);

    sr->src.end = sr->cons->inbuf;
    sr->done = false;
    sr->pend = 0;
    sr->any = false;
    srcr_phys_start(&sr->cont);
    srcr_fill(sr);
    return sr->any ? (int32_t)(sr->src.end - sr->cons->inbuf) : -1;
}

bool srcr_more(LexrSource *const src, const uint8_t **const keep)
{
    Srcr *const sr = container_of(src, Srcr, src);

    if (sr->done)
        return false;

    const uint32_t n = (uint32_t)(sr->src.end - *keep);
    memmove(sr->cons->inbuf, *keep, n);
    sr->src.end = sr->cons->inbuf + n;
    srcr_fill(sr);
    *keep = sr->cons->inbuf;
    return true;
}

void srcr_init(Srcr *const sr, Cons *const co, Chnl *const ch, uint16_t *const fline)
{
    sr->src.more = srcr_more;
    sr->cons = co;
    sr->chnl = ch;
    sr->fline = fline;
}
