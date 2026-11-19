/* Strings: their descriptors, temporaries and heap, with collection.
 *
 * # How it works
 * A string value is the Addr of its Desc: a variable's cell, an array element, a record's field or a
 * temporary. The temporaries are NTMP Descs at strg.tmp, tmp_used bytes of them in use. A Desc's
 * ptr may point into the program store, a text buffer in the arena or a heap block.
 *
 * The heap's strings run down from allo.top to low; a block is the chars, then a StrTail (len,
 * owner: the Desc's Addr), live while that Desc points at the chars. At most SMAX chars. When the
 * heap is full strg_gc slides the live blocks up and their owners follow.
 *
 * # asm structure (retired/asm32/mem/str.asm)
 * | asm | here | |
 * |---|---|---|
 * | TSTR, g_state.sg | Strg | MIRROR |
 * | sg.tsp, sg.tspb | Strg.tmp_used, Strg.tmp_floor | MIRROR |
 * | sg.tmplin, g_tmpd | Strg.tmp | MIRROR |
 * | mm.sfr | Strg.low | MIRROR |
 * | g_nbuf | Strg.near_buf | MIRROR |
 * | sg.sln, sg.spt, sg.dpv, sg.sn, sg.bpo, sg.s0v, sg.s1v | Desc, return values, parameters | D1 |
 * | sg.sac sg.sab sg.gq sg.gd | locals | D1 |
 * | str_get, str_getd | strg_get | MIRROR; D2 |
 * | str_push, str_alloc, str_new, str_own | strg_push, strg_alloc, strg_new, strg_own | MIRROR |
 * | str_gc | strg_gc | MIRROR; D3 |
 * | str_set | strg_set(value, desc) | MIRROR |
 * | str_near, str_print | strg_near, strg_print | MIRROR; D4 |
 * | str_item, str_skq, str_span | strg_item, strg_skipq, strg_span | MIRROR; D5 |
 * | str_dlen, str_cat, str_sub | strg_len, strg_cat, strg_sub | MIRROR |
 * | str_cmp (EDX, EAX -> CL 1, 2, 4) | strg_cmp -> SCMP_GT, SCMP_EQ, SCMP_LT | MIRROR |
 * | str_tods, str_buftmp | strg_tods, strg_buftmp | MIRROR |
 * | str_lit, str_scan, str_fit, str_cpy, str_cpyd | | MIRROR; static |
 *
 * - D1. The routines' register and global inputs and outputs are a Desc (.len, .ptr), return values
 *   and parameters.
 * - D2. Returns the Desc; empty is `.len == 0`.
 * - D3. FRE is `allo.strings - allo.arrays` after it.
 * - D4. Length min(s.len, 255).
 * - D5. The position is an Addr in `*pos`; strg_span's start is `*p` before the call.
 */
#include "mem/string.h"
#include "mem/copy.h"

void strg_init(Strg *const sg, Allo *const al, Fault *const f, const Prog *const pg)
{
    sg->allo = al;
    sg->fault = f;
    sg->prog = pg;
    sg->tmp = allo_carve(al, DSZ * NTMP);
}

static void strg_desc_put(const Addr d, const Desc s) { memcpy(at(d), &s, DSZ); }

/* The tail of the block whose len chars are at chars. */
static StrTail *strg_tail_of(const Addr chars, const uint16_t len)
{
    return (StrTail *)at(chars + len);
}

/* Is d one of the temporaries' Descs, in use or not? */
static bool strg_is_tmp(const Strg *const sg, const Addr d) { return d - sg->tmp < DSZ * NTMP; }

/* Is d a temporary popped already? */
static bool strg_is_popped_tmp(const Strg *const sg, const Addr d)
{
    return strg_is_tmp(sg, d) && d - sg->tmp >= sg->tmp_used;
}

Desc strg_get(Strg *const sg, const Addr desc)
{
    if (desc == sg->tmp + sg->tmp_used - DSZ)
        sg->tmp_used -= DSZ;

    return strg_peek(desc);
}

Addr strg_push(Strg *const sg, const Desc s)
{
    if (sg->tmp_used >= DSZ * NTMP)
        fault_error(sg->fault, E_ST);

    const Addr d = sg->tmp + sg->tmp_used;
    strg_desc_put(d, s);
    sg->tmp_used += DSZ;
    return d;
}

void strg_own(const Addr chars, const uint16_t len, const Addr owner)
{
    strg_tail_of(chars, len)->owner = owner;
}

Addr strg_new(Strg *const sg, const Addr chars, const uint16_t len)
{
    strg_own(chars, len, sg->tmp + sg->tmp_used);
    return strg_push(sg, (Desc){.len = len, .ptr = chars});
}

/* A block of len chars under low fits above the arrays: its chars, else 0. */
static Addr strg_fit(const Strg *const sg, const uint16_t len)
{
    const Addr c = sg->allo->strings - len - BHDR;
    return sg->allo->arrays < c ? c : 0;
}

Addr strg_alloc(Strg *const sg, const uint16_t len)
{
    Addr c = strg_fit(sg, len);

    if (!c) {
        strg_gc(sg);
        c = strg_fit(sg, len);

        if (!c)
            fault_error(sg->fault, E_OM);
    }

    sg->allo->strings = c;
    *strg_tail_of(c, len) = (StrTail){.len = len, .owner = 0};

    return c;
}

/* Is the block at chars live: its owner a Desc still pointing at them? */
/* Is d a Desc of the data stack's below its top: a frame gone? */
static bool strg_is_popped_frame(const Strg *const sg, const Addr d)
{
    return d >= sg->stack && d < sg->stack_end && d < addr_of(*sg->sp);
}

static bool strg_is_live(const Strg *const sg, const Addr chars, const Addr owner)
{
    return owner && !strg_is_popped_tmp(sg, owner) && !strg_is_popped_frame(sg, owner) &&
           strg_peek(owner).ptr == chars;
}

void strg_gc(Strg *const sg)
{
    Addr dst = sg->allo->top;

    for (Addr end = sg->allo->top; end > sg->allo->strings;) {
        const StrTail tail = *(const StrTail *)at(end - BHDR);
        const Addr chars = end - BHDR - tail.len;
        end = chars; /* the next block's end */
        if (!strg_is_live(sg, chars, tail.owner))
            continue;

        dst -= tail.len + BHDR;
        copy_mem(dst, chars, tail.len + BHDR);
        wr32(tail.owner + offsetof(Desc, ptr), dst);
    }

    sg->allo->strings = dst;
}

/* Are s's chars in the program's text? */
static bool strg_in_program(const Strg *const sg, const Desc s)
{
    const uint32_t off = s.ptr - sg->prog->base;
    return off >= PROGBASE && off < sg->prog->end;
}

/* Is s's block the temporary value's own, to be taken over? */
static bool strg_owned_by_tmp(const Strg *const sg, const Desc s, const Addr value)
{
    return s.ptr >= sg->allo->strings && s.ptr < sg->allo->top && strg_is_tmp(sg, value) &&
           strg_tail_of(s.ptr, s.len)->owner == value;
}

void strg_set(Strg *const sg, const Addr value, const Addr desc)
{
    Desc s = strg_get(sg, value);

    if (s.len && !strg_in_program(sg, s)) {     /* the program's text is kept where it is */
        if (!strg_owned_by_tmp(sg, s, value)) { /* a variable's block stays its own */
            const Addr c = strg_alloc(sg, s.len);
            s = strg_get(sg, value);
            copy_mem(c, s.ptr, s.len);
            s.ptr = c;
        }

        strg_own(s.ptr, s.len, desc);
    }

    strg_desc_put(desc, s);
}

const uint8_t *strg_near(Strg *const sg, const Desc s)
{
    const uint16_t n = s.len > NEAR_MAX ? NEAR_MAX : s.len;
    memcpy(sg->near_buf, at(s.ptr), n);
    sg->near_buf[n] = 0;
    return sg->near_buf;
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
static Addr strg_scan(Strg *const sg, Addr *const pos, const uint8_t stop1, const uint8_t stop2)
{
    const uint8_t *p = at(*pos);
    const Desc s = {.len = (uint16_t)strg_span(&p, stop1, stop2), .ptr = *pos};

    *pos = addr_of(p);
    return strg_push(sg, s);
}

Addr strg_item(Strg *const sg, Addr *const pos)
{
    while (rd8(*pos) == ' ')
        ++*pos;
    if (rd8(*pos) != '"')
        return strg_scan(sg, pos, ',', ':');

    ++*pos;
    const Addr t = strg_scan(sg, pos, '"', '"');
    *pos = addr_of(strg_skipq(at(*pos)));
    return t;
}

uint16_t strg_len(const Addr desc) { return rd16(desc); }

Addr strg_cat(Strg *const sg, const Addr left, const Addr right)
{
    const uint16_t left_len = strg_len(left);
    const uint32_t n = left_len + strg_len(right);

    if (n > SMAX)
        fault_error(sg->fault, E_LS);

    const Addr c = strg_alloc(sg, (uint16_t)n);
    const Desc r = strg_get(sg, right);
    copy_mem(c + left_len, r.ptr, r.len);
    const Desc l = strg_get(sg, left);
    copy_mem(c, l.ptr, l.len);
    return strg_new(sg, c, (uint16_t)n);
}

uint8_t strg_cmp(Strg *const sg, const Addr left, const Addr right)
{
    const Desc r = strg_get(sg, right);
    const Desc l = strg_get(sg, left);
    const uint8_t *const a = at(l.ptr);
    const uint8_t *const b = at(r.ptr);
    const uint16_t n = l.len < r.len ? l.len : r.len;

    for (uint16_t i = 0; i < n; i++)
        if (a[i] != b[i])
            return a[i] < b[i] ? SCMP_LT : SCMP_GT;

    return l.len == r.len ? SCMP_EQ : l.len < r.len ? SCMP_LT : SCMP_GT;
}

Addr strg_sub(Strg *const sg, const Addr desc, const uint16_t start, const uint16_t most)
{
    const uint16_t len = strg_len(desc);
    const uint16_t rest = len > start ? len - start : 0;
    const uint16_t n = rest < most ? rest : most;
    const Addr c = strg_alloc(sg, n);
    const Desc s = strg_get(sg, desc);
    return strg_tods(sg, c, s.ptr + start, n);
}

Addr strg_tods(Strg *const sg, const Addr chars, const Addr src, const uint16_t len)
{
    copy_mem(chars, src, len);
    return strg_new(sg, chars, len);
}

Addr strg_text(Strg *const sg, const uint8_t *const text, const uint16_t len)
{
    const Addr c = strg_alloc(sg, len);
    return strg_tods(sg, c, addr_of(text), len);
}

void strg_move(const Strg *const sg, const Addr from, const Addr to)
{
    const Desc d = strg_peek(from);
    strg_desc_put(to, d);

    if (d.len && d.ptr >= sg->allo->strings && d.ptr < sg->allo->top)
        strg_own(d.ptr, d.len, to);
}

void strg_copy_owned(Strg *const sg, const Addr dst, const Addr src, const uint16_t *const offs, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        strg_set(sg, src + offs[i], dst + offs[i]);
}

void strg_move_owned(const Strg *const sg, const Addr dst, const Addr src, const uint16_t *const offs, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        strg_move(sg, src + offs[i], dst + offs[i]);
}
