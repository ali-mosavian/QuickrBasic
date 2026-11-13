#include "mem/strg.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/arry.h"
#include "mem/copy.h"
#include "mem/prog.h"

Strg strg;

static uint8_t nbuf[257];

void strg_init(void) { strg.tmp = allo_carve(DSZ * NTMP); }

static Desc desc_at(Addr d)
{
    Desc s;
    memcpy(&s, at(d), DSZ);
    return s;
}

static void desc_put(Addr d, Desc s) { memcpy(at(d), &s, DSZ); }

/* Whether d is one of the temporaries' Descs, in use or not. */
static bool is_tmp(Addr d) { return d - strg.tmp < DSZ * NTMP; }

Desc strg_get(Addr desc)
{
    if (desc == strg.tmp + strg.tmp_used - DSZ)
        strg.tmp_used -= DSZ;
    return desc_at(desc);
}

Addr strg_push(Desc s)
{
    if (strg.tmp_used >= DSZ * NTMP)
        repl_error(E_ST);
    Addr d = strg.tmp + strg.tmp_used;
    desc_put(d, s);
    strg.tmp_used += DSZ;
    return d;
}

void strg_own(Addr chars, uint16_t len, Addr owner)
{
    wr32(chars + len + offsetof(StrTail, owner), owner);
}

Addr strg_new(Addr chars, uint16_t len)
{
    strg_own(chars, len, strg.tmp + strg.tmp_used);
    return strg_push((Desc){len, chars});
}

/* A block of len chars under low fits above the arrays: its chars, else 0. */
static Addr fit(uint16_t len)
{
    Addr c = strg.low - len - BHDR;
    return arry.end < c ? c : 0;
}

Addr strg_alloc(uint16_t len)
{
    Addr c = fit(len);
    if (!c) {
        strg_gc();
        c = fit(len);
        if (!c)
            repl_error(E_OM);
    }
    strg.low = c;
    wr16(c + len, len);
    wr32(c + len + offsetof(StrTail, owner), 0);
    return c;
}

void strg_gc(void)
{
    Addr q = allo.top, d = allo.top;
    while (q > strg.low) {
        q -= BHDR;
        uint16_t len = rd16(q);
        Addr owner = rd32(q + offsetof(StrTail, owner));
        q -= len; /* the chars, the next block's end */
        if (!owner)
            continue;
        if (is_tmp(owner) && owner - strg.tmp >= strg.tmp_used)
            continue; /* a popped temporary */
        if (desc_at(owner).ptr != q)
            continue;
        d -= len + BHDR;
        copy_mem(d, q, len + BHDR);
        wr32(owner + offsetof(Desc, ptr), d);
    }
    strg.low = d;
}

void strg_set(Addr value, Addr desc)
{
    Desc s = strg_get(value);
    if (s.len) {
        uint32_t off = s.ptr - prog.base;
        if (off >= PROGBASE && off < prog.end)
            goto point; /* in the program's text: kept there */
        bool own = s.ptr >= strg.low && s.ptr < allo.top && is_tmp(value) &&
                   rd32(s.ptr + s.len + offsetof(StrTail, owner)) == value;
        if (!own) { /* a variable's block stays its own */
            Addr c = strg_alloc(s.len);
            s = strg_get(value);
            copy_mem(c, s.ptr, s.len);
            s.ptr = c;
        }
        strg_own(s.ptr, s.len, desc);
    }
point:
    desc_put(desc, s);
}

const uint8_t *strg_near(Desc s)
{
    uint16_t n = s.len > 255 ? 255 : s.len;
    memcpy(nbuf, at(s.ptr), n);
    nbuf[n] = 0;
    return nbuf;
}

void strg_print(Desc s)
{
    while (s.len) {
        uint16_t n = s.len > 255 ? 255 : s.len;
        const uint8_t *p = strg_near(s);
        for (uint16_t i = 0; i < n; i++)
            cons_putc(p[i]);
        s.ptr += n;
        s.len -= n;
    }
}

uint32_t strg_span(const uint8_t **p, uint8_t stop1, uint8_t stop2)
{
    const uint8_t *s = *p;
    while (**p && **p != stop1 && **p != stop2)
        (*p)++;
    return (uint32_t)(*p - s);
}

const uint8_t *strg_skipq(const uint8_t *p) { return *p ? p + 1 : p; }

/* A temporary for the text at *pos up to a 0, stop1 or stop2; *pos at the stop. */
static Addr scan(Addr *pos, uint8_t stop1, uint8_t stop2)
{
    const uint8_t *p = at(*pos);
    uint16_t len = (uint16_t)strg_span(&p, stop1, stop2);
    Desc s = {len, *pos};
    *pos = addr_of(p);
    return strg_push(s);
}

Addr strg_item(Addr *pos)
{
    while (rd8(*pos) == ' ')
        ++*pos;
    if (rd8(*pos) != '"')
        return scan(pos, ',', ':');
    ++*pos;
    Addr t = scan(pos, '"', '"');
    *pos = addr_of(strg_skipq(at(*pos)));
    return t;
}

uint16_t strg_len(Addr desc) { return rd16(desc); }

Addr strg_cat(Addr left, Addr right)
{
    uint16_t llen = strg_len(left);
    uint32_t n = strg_len(right) + llen;
    if (n > SMAX)
        repl_error(E_LS);
    Addr c = strg_alloc((uint16_t)n);
    Desc s = strg_get(right);
    copy_mem(c + llen, s.ptr, s.len);
    s = strg_get(left);
    copy_mem(c, s.ptr, s.len);
    return strg_new(c, (uint16_t)n);
}

uint8_t strg_cmp(Addr left, Addr right)
{
    Desc r = strg_get(right);
    Desc l = strg_get(left);
    const uint8_t *a = at(l.ptr), *b = at(r.ptr);
    for (uint16_t i = 0;; i++) {
        if (i == l.len)
            return i == r.len ? SCMP_EQ : SCMP_LT;
        if (i == r.len)
            return SCMP_GT;
        if (a[i] != b[i])
            return a[i] < b[i] ? SCMP_LT : SCMP_GT;
    }
}

Addr strg_sub(Addr desc, uint16_t start, uint16_t most)
{
    uint16_t len = strg_len(desc);
    uint16_t n = len > start ? len - start : 0;
    if (n > most)
        n = most;
    Addr c = strg_alloc(n);
    Desc s = strg_get(desc);
    return strg_tods(c, s.ptr + start, n);
}

Addr strg_tods(Addr chars, Addr src, uint16_t len)
{
    copy_mem(chars, src, len);
    return strg_new(chars, len);
}

Addr strg_buftmp(uint16_t len)
{
    Addr c = strg_alloc(len);
    return strg_tods(c, addr_of(cons.sbuf), len);
}
