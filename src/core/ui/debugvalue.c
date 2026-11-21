/* Values as the debugger shows them: numbers as PRINT's, strings in quotes.
 *
 * # How it works
 * A variable's kind (Var.info) says how its cell holds it: a DOUBLE's 8 bytes, a string's Desc, a
 * SINGLE's 4, else an integer's.
 */
#include "ui/debugvalue.h"
#include "session.h"
#include "mem/alloc.h"
#include "mem/symbols.h"
#include "num/double.h"

char *dval_cpy(char *const d, const char *const s)
{
    uint32_t i = 0;

    for (; s[i]; i++)
        d[i] = s[i];

    return d + i;
}

char *dval_dec(char *d, const int32_t v)
{
    uint32_t u = (uint32_t)v;

    if (v < 0) {
        *d++ = '-';
        u = 0u - u;
    }

    char t[10];
    int n = 0;

    do {
        t[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);

    for (int i = 0; i < n; i++)
        d[i] = t[n - 1 - i];

    return d + n;
}

void dval_cut(char *const d, const char *const s)
{
    uint32_t i = 0;

    for (; i < DP_TWMAX - 1 && s[i]; i++)
        d[i] = s[i];

    d[i] = 0;
}

char *dval_var(Session *const S, char *d, const uint32_t id, const Addr cell)
{
    const Var *const v = symb_var(&S->symb, id);
    uint8_t buf[DUBL_TXT];
    uint32_t n;

    if (v->info & SK_DBL) {
        const double x = load_cell(at(cell)).d;
        n = dubl_fmt(x, buf);
    } else if ((v->info & VK_MASK) == VK_STR) {
        const Desc *const s = (const Desc *)at(cell);
        const uint32_t len = s->len > DP_TWMAX ? DP_TWMAX : s->len;
        *d++ = '"';
        memcpy(d, at(s->ptr), len);
        d += len;
        *d++ = '"';
        return d;
    } else if (v->type == ET_R4 || v->type == ET_IMP) {
        n = dubl_fmtf(rd32(cell), buf);
    } else {
        return dval_dec(d, (int32_t)rd32(cell));
    }

    const uint8_t *s = buf;

    if (*s == ' ') {
        s++;
        n--;
    }

    memcpy(d, s, n);
    return d + n;
}
