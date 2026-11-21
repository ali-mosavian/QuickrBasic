/* The debugger's Watch: expressions, each PRINTed at each stop.
 *
 * # How it works
 * A watch is PRINTed by session_eval in the stop's procedure's scope; its row is what PRINT shows,
 * its spaces and end of line left out. Watches are kept across runs.
 */
#include "ui/debugwatch.h"
#include "session.h"
#include "io/errmsg.h"
#include "ui/debugvalue.h"

enum {
    NWATCH = 8, /* watches at most */
    WXLEN = 41, /* a watch's expression, 0-ended */
};

static struct {
    uint8_t n;
    char wx[NWATCH][WXLEN];
    char wv[NWATCH][DP_TWMAX];
    char row[DVAL_ROW];
} wat;

uint8_t dwat_count(void) { return wat.n; }

const char *dwat_row(const uint8_t i) { return wat.wv[i]; }

/* session_eval's output out at d, its spaces and end of line left out; past it. */
static char *eval_text(char *d, const char *const out)
{
    uint32_t a = 0;

    while (out[a] == ' ')
        a++;

    uint32_t n = 0;

    for (; (uint8_t)out[a + n] >= ' '; n++)
        d[n] = out[a + n];

    d += n;

    while (d[-1] == ' ') /* may eat into " = " for an empty string, as the asm */
        d--;

    return d;
}

/* Expression expr's value as text at d: session_eval's output, its spaces and end of line left
   out; an error's message. A ':' (another statement) or a direct line's frame on cstk (its code
   would go): none. Past it. */
static char *value(Session *const S, char *const d, const char *const expr, const uint32_t proc)
{
    if (strchr(expr, ':'))
        return dval_cpy(d, emsg_text(S->vm.err.user, E_SN));
    if (session_in_direct(S))
        return dval_cpy(d, "(not here)");

    char out[80];
    const Status st = session_eval(S, expr, proc, out);
    return st != ST_OK ? dval_cpy(d, emsg_text(S->vm.err.user, (uint8_t)st)) : eval_text(d, out);
}

void dwat_eval(Session *const S, const uint32_t proc)
{
    for (uint8_t i = 0; i < wat.n; i++) {
        char *d = dval_cpy(dval_cpy(wat.row, wat.wx[i]), " = ");
        d = value(S, d, wat.wx[i], proc);
        *d = 0;
        dval_cut(wat.wv[i], wat.row);
    }
}

bool dwat_add(const uint8_t *const expr, const uint32_t n)
{
    if (wat.n >= NWATCH)
        return false;

    const uint32_t len = n < WXLEN - 1 ? n : WXLEN - 1;
    memcpy(wat.wx[wat.n], expr, len);
    wat.wx[wat.n][len] = 0;
    wat.n++;
    return true;
}

void dwat_del(const uint8_t i)
{
    if (i >= wat.n)
        return;

    wat.n--;
    const uint32_t after = wat.n - i;
    memmove(wat.wx[i], wat.wx[i + 1], after * WXLEN);
    memmove(wat.wv[i], wat.wv[i + 1], after * sizeof wat.wv[0]);
}
