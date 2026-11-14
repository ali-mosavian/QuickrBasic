/* The lines the edit windows show, kept: a scroll lists and colours only the lines it brings in.
 *
 * # How it works
 * A ring of VISL_RING slots holds a line's text and colours at the slot of its line in the store
 * (the view's first line on), modulo the ring. A slot stays good while the line's record has the
 * bytes it was listed from: a line's text is its record's alone. A record past VISL_REC bytes or a
 * text past SYNT_KEEP characters is not kept; it is listed into uimn.line each time.
 */
#include "ui/visible.h"
#include "session.h"
#include "ui/highlight.h"
#include "ui/text.h"

enum { VISL_RING = 64, VISL_REC = 512 };

static struct {
    uint32_t line; /* the store's line + 1, 0 none */
    uint32_t n;    /* the record's bytes past its header, kept in rec */
    uint8_t asis;  /* its LF_ASIS */
    bool colored;  /* cols worked out */
    uint32_t len;
    uint8_t rec[VISL_REC];
    uint8_t text[SYNT_KEEP];
    uint8_t cols[SYNT_KEEP];
} ring[VISL_RING];

VislLine visl_get(Session *const S, const uint32_t line)
{
    const uint32_t key = uimn.text.first + line + 1;
    const uint32_t rec = text_rec(S, line);
    const Line *const p = prog_line(&S->prog, rec);
    const uint8_t *const body = at(prog_addr(&S->prog, rec + HDR));
    const uint32_t n = p->len - HDR;
    const uint8_t asis = p->flags & LF_ASIS;
    typeof(&ring[0]) const r = &ring[key % VISL_RING];

    if (r->line != key || r->n != n || r->asis != asis || memcmp(r->rec, body, n)) {
        const uint32_t len = text_get(S, line, uimn.line);

        if (n > VISL_REC || len > SYNT_KEEP) {
            r->line = 0;
            return (VislLine){.text = uimn.line, .len = len};
        }

        r->line = key;
        r->n = n;
        r->asis = asis;
        r->colored = false;
        r->len = len;

        memcpy(r->rec, body, n);
        memcpy(r->text, uimn.line, len);
    }

    if (!synt.on)
        return (VislLine){.text = r->text, .len = r->len};

    if (!r->colored) {
        synt_cols(S, r->text, r->len, r->cols);
        r->colored = true;
    }

    return (VislLine){.text = r->text, .len = r->len, .cols = r->cols};
}
