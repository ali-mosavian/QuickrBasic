#include "mem/string.h"
#include "session.h"
#include "io/console.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/array.h"
#include "mem/copy.h"
#include "mem/program.h"

void strg_init(Session *const S) { S->strg.tmp = allo_carve(S, DSZ * NTMP); }

static void desc_put(const Addr d, const Desc s) { memcpy(at(d), &s, DSZ); }

/* The tail of the block whose len chars are at chars. */
static StrTail *tail_of(const Addr chars, const uint16_t len) { return (StrTail *)at(chars + len); }

/* Is d one of the temporaries' Descs, in use or not? */
static bool is_tmp(Session *const S, const Addr d) { return d - S->strg.tmp < DSZ * NTMP; }

/* Is d a temporary popped already? */
static bool is_popped_tmp(Session *const S, const Addr d)
{
    return is_tmp(S, d) && d - S->strg.tmp >= S->strg.tmp_used;
}

Desc strg_get(Session *const S, const Addr desc)
{
    if (desc == S->strg.tmp + S->strg.tmp_used - DSZ)
        S->strg.tmp_used -= DSZ;

    return strg_peek(desc);
}

Addr strg_push(Session *const S, const Desc s)
{
    if (S->strg.tmp_used >= DSZ * NTMP)
        repl_error(S, E_ST);

    const Addr d = S->strg.tmp + S->strg.tmp_used;
    desc_put(d, s);
    S->strg.tmp_used += DSZ;
    return d;
}

void strg_own(const Addr chars, const uint16_t len, const Addr owner)
{
    tail_of(chars, len)->owner = owner;
}

Addr strg_new(Session *const S, const Addr chars, const uint16_t len)
{
    strg_own(chars, len, S->strg.tmp + S->strg.tmp_used);
    return strg_push(S, (Desc){.len = len, .ptr = chars});
}

/* A block of len chars under low fits above the arrays: its chars, else 0. */
static Addr fit(Session *const S, const uint16_t len)
{
    const Addr c = S->strg.low - len - BHDR;
    return S->arry.end < c ? c : 0;
}

Addr strg_alloc(Session *const S, const uint16_t len)
{
    Addr c = fit(S, len);

    if (!c) {
        strg_gc(S);
        c = fit(S, len);

        if (!c)
            repl_error(S, E_OM);
    }

    S->strg.low = c;
    *tail_of(c, len) = (StrTail){.len = len, .owner = 0};

    return c;
}

/* Is the block at chars live: its owner a Desc still pointing at them? */
static bool is_live(Session *const S, const Addr chars, const Addr owner)
{
    return owner && !is_popped_tmp(S, owner) && strg_peek(owner).ptr == chars;
}

void strg_gc(Session *const S)
{
    Addr dst = S->allo.top;

    for (Addr end = S->allo.top; end > S->strg.low;) {
        const StrTail tail = *(const StrTail *)at(end - BHDR);
        const Addr chars = end - BHDR - tail.len;
        end = chars; /* the next block's end */
        if (!is_live(S, chars, tail.owner))
            continue;

        dst -= tail.len + BHDR;
        copy_mem(dst, chars, tail.len + BHDR);
        wr32(tail.owner + offsetof(Desc, ptr), dst);
    }

    S->strg.low = dst;
}

/* Are s's chars in the program's text? */
static bool in_program(Session *const S, const Desc s)
{
    const uint32_t off = s.ptr - S->prog.base;
    return off >= PROGBASE && off < S->prog.end;
}

/* Is s's block the temporary value's own, to be taken over? */
static bool owned_by_tmp(Session *const S, const Desc s, const Addr value)
{
    return s.ptr >= S->strg.low && s.ptr < S->allo.top && is_tmp(S, value) &&
           tail_of(s.ptr, s.len)->owner == value;
}

void strg_set(Session *const S, const Addr value, const Addr desc)
{
    Desc s = strg_get(S, value);

    if (s.len && !in_program(S, s)) {     /* the program's text is kept where it is */
        if (!owned_by_tmp(S, s, value)) { /* a variable's block stays its own */
            const Addr c = strg_alloc(S, s.len);
            s = strg_get(S, value);
            copy_mem(c, s.ptr, s.len);
            s.ptr = c;
        }

        strg_own(s.ptr, s.len, desc);
    }

    desc_put(desc, s);
}

const uint8_t *strg_near(Session *const S, const Desc s)
{
    const uint16_t n = s.len > NEAR_MAX ? NEAR_MAX : s.len;
    memcpy(S->strg.near_buf, at(s.ptr), n);
    S->strg.near_buf[n] = 0;
    return S->strg.near_buf;
}

void strg_print(Session *const S, const Desc s)
{
    const uint8_t *const p = at(s.ptr);

    for (uint16_t i = 0; i < s.len; i++)
        cons_putc(S, p[i]);
}

uint32_t strg_span(const uint8_t **const p, const uint8_t stop1, const uint8_t stop2)
{
    const uint8_t *const start = *p;

    while (**p && **p != stop1 && **p != stop2)
        (*p)++;

    return (uint32_t)(*p - start);
}

const uint8_t *strg_skipq(const uint8_t *const p) { return *p ? p + 1 : p; }

/* A temporary for the text at *pos up to a 0, stop1 or stop2; *pos at the stop. */
static Addr scan(Session *const S, Addr *const pos, const uint8_t stop1, const uint8_t stop2)
{
    const uint8_t *p = at(*pos);
    const Desc s = {.len = (uint16_t)strg_span(&p, stop1, stop2), .ptr = *pos};

    *pos = addr_of(p);
    return strg_push(S, s);
}

Addr strg_item(Session *const S, Addr *const pos)
{
    while (rd8(*pos) == ' ')
        ++*pos;
    if (rd8(*pos) != '"')
        return scan(S, pos, ',', ':');

    ++*pos;
    const Addr t = scan(S, pos, '"', '"');
    *pos = addr_of(strg_skipq(at(*pos)));
    return t;
}

uint16_t strg_len(const Addr desc) { return rd16(desc); }

Addr strg_cat(Session *const S, const Addr left, const Addr right)
{
    const uint16_t left_len = strg_len(left);
    const uint32_t n = left_len + strg_len(right);

    if (n > SMAX)
        repl_error(S, E_LS);

    const Addr c = strg_alloc(S, (uint16_t)n);
    const Desc r = strg_get(S, right);
    copy_mem(c + left_len, r.ptr, r.len);
    const Desc l = strg_get(S, left);
    copy_mem(c, l.ptr, l.len);
    return strg_new(S, c, (uint16_t)n);
}

uint8_t strg_cmp(Session *const S, const Addr left, const Addr right)
{
    const Desc r = strg_get(S, right);
    const Desc l = strg_get(S, left);
    const uint8_t *const a = at(l.ptr);
    const uint8_t *const b = at(r.ptr);
    const uint16_t n = l.len < r.len ? l.len : r.len;

    for (uint16_t i = 0; i < n; i++)
        if (a[i] != b[i])
            return a[i] < b[i] ? SCMP_LT : SCMP_GT;

    return l.len == r.len ? SCMP_EQ : l.len < r.len ? SCMP_LT : SCMP_GT;
}

Addr strg_sub(Session *const S, const Addr desc, const uint16_t start, const uint16_t most)
{
    const uint16_t len = strg_len(desc);
    const uint16_t rest = len > start ? len - start : 0;
    const uint16_t n = rest < most ? rest : most;
    const Addr c = strg_alloc(S, n);
    const Desc s = strg_get(S, desc);
    return strg_tods(S, c, s.ptr + start, n);
}

Addr strg_tods(Session *const S, const Addr chars, const Addr src, const uint16_t len)
{
    copy_mem(chars, src, len);
    return strg_new(S, chars, len);
}

Addr strg_buftmp(Session *const S, const uint16_t len)
{
    const Addr c = strg_alloc(S, len);
    return strg_tods(S, c, addr_of(S->cons.sbuf), len);
}
