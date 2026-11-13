#include "mem/arry.h"
#include "comp/decl.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/copy.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "vm/exec.h"
#include "vm/flow.h"

enum { DATA_MAX = 0x100000, DATA_ALIGN = 4 };

Arry arry;

static ArrayHdr *hdr_at(const Addr h) { return (ArrayHdr *)at(h); }

/* A header's bytes: the fixed part, then a count and a lower bound a dimension. */
static uint32_t hdr_size(const uint8_t dims) { return sizeof(ArrayHdr) + dims * 2u * sizeof(uint16_t); }

static uint16_t count(const ArrayHdr *const a, const uint32_t i) { return a->count[i]; }
static int16_t lbound(const ArrayHdr *const a, const uint32_t i) { return (int16_t)a->count[a->dims + i]; }

static uint32_t elem_count(const ArrayHdr *const a)
{
    uint32_t n = 1;
    for (uint32_t i = 0; i < a->dims; i++)
        n *= count(a, i);
    return n;
}

/* size bytes from a, to the next DATA_ALIGN. */
static uint64_t aligned_end(const Addr a, const uint32_t size)
{
    return ((uint64_t)a + size + DATA_ALIGN - 1) & ~(uint64_t)(DATA_ALIGN - 1);
}

static bool is_array_var(const uint16_t id) { return symb_var(id)->info & VI_ARR; }

[[noreturn]] void arry_bad_sub(void) { repl_error(E_BS); }

uint32_t arry_esize(const uint16_t id)
{
    const Var *const v = symb_var(id);
    if (v->type >= ET_REC)
        return decl_types[v->type - ET_REC].size;
    if (v->info & SK_DBL)
        return 8;
    return (v->info & VK_MASK) == VK_STR ? DSZ : VS;
}

Addr arry_ref(const uint16_t id, const uint8_t n)
{
    Addr h = symb_var(id)->vt;
    if (!h) {
        if (symb_varx(id)->dyn) /* ERASE freed it: not made again */
            arry_bad_sub();
        for (uint32_t i = 0; i < n; i++) { /* OPTION BASE to 10 */
            arry.counts[i] = 11 - decl.base;
            arry.lbounds[i] = decl.base;
        }
        h = arry_new(id, n, false);
    }
    const ArrayHdr *const a = hdr_at(h);
    if (n != a->dims)
        arry_bad_sub();
    uint32_t idx = 0; /* the last subscript the fastest */
    for (uint32_t i = 0; i < n; i++) {
        const uint16_t r = arry.subs[i] - (uint16_t)lbound(a, i);
        if (r >= count(a, i))
            arry_bad_sub();
        idx = idx * count(a, i) + r;
    }
    return a->data + idx * arry_esize(id);
}

void arry_dim(const uint16_t id, const uint8_t n, const bool dyn)
{
    for (uint32_t i = n; i-- > 0;) {
        if ((int16_t)arry.subs[i] < 0)
            repl_error(E_IQ);
        arry.counts[i] = arry.subs[i] + 1;
    }
    if (symb_var(id)->vt)
        repl_error(E_RD);
    arry_new(id, n, dyn);
}

Addr arry_new(const uint16_t id, const uint8_t n, const bool dyn)
{
    uint64_t size = arry_esize(id);
    for (uint32_t i = 0; i < n; i++) {
        size *= arry.counts[i];
        if (size > DATA_MAX)
            repl_error(E_OM);
    }
    if (arry.hdr + hdr_size(n) > allo.hdr_base + HDRSZ)
        repl_error(E_OM);
    if (aligned_end(arry.end, (uint32_t)size) > strg.low) {
        strg_gc();
        if (aligned_end(arry.end, (uint32_t)size) > strg.low)
            repl_error(E_OM);
    }
    const Addr data = arry.end;
    arry.end = (Addr)aligned_end(data, (uint32_t)size);
    copy_zero(data, (uint32_t)size);

    const Addr h = arry.hdr;
    ArrayHdr *const a = hdr_at(h);
    a->dims = n;
    a->data = data;
    a->fast = n == 1 && arry.lbounds[0] == 0 ? arry.counts[0] : 0; /* 1-D from 0 */
    memcpy(&a->count[0], arry.counts, n * sizeof arry.counts[0]);
    memcpy(&a->count[n], arry.lbounds, n * sizeof arry.lbounds[0]);
    arry.hdr = h + hdr_size(n);
    symb_var(id)->vt = h;
    symb_varx(id)->dyn = dyn;
    return h;
}

uint32_t arry_size(const uint16_t id, const Addr hdr)
{
    return arry_esize(id) * elem_count(hdr_at(hdr));
}

/* ---- freeing -------------------------------------------------------------------------------- */

/* An array being freed: its header and the header's bytes, its data [data, data + size), the
   data's old end, what references to the header become. */
typedef struct {
    Addr hdr;
    uint32_t hdr_size;
    Addr data;
    uint32_t size;
    Addr old_end;
    Addr rep;
} Freed;

/* A header kept: past the dropped one it moves down; the dropped one's becomes rep. */
static Addr moved_hdr(const Freed *const f, const Addr h)
{
    if (h < f->hdr)
        return h;
    if (h == f->hdr)
        return f->rep;
    return h < arry.hdr + f->hdr_size ? h - f->hdr_size : h;
}

/* A parameter's cell kept: in the data moved down, it follows. */
static Addr moved_cell(const Freed *const f, const Addr c)
{
    const bool moved = c >= f->data && c - f->data >= f->size && c < f->old_end;
    return moved ? c - f->size : c;
}

/* The moved string blocks' owners follow. */
static void follow_owners(const Freed *const f)
{
    for (Addr b = allo.top; b > strg.low;) {
        b -= BHDR;
        StrTail *const tail = (StrTail *)at(b);
        if (tail->owner >= f->data && tail->owner < f->old_end)
            tail->owner -= f->size;
        b -= tail->len;
    }
}

uint32_t arry_free(const uint16_t id, const Addr rep)
{
    Var *const v = symb_var(id);
    const Addr h = v->vt;
    v->vt = 0;
    const ArrayHdr *const a = hdr_at(h);
    const Freed f = {
        .hdr = h,
        .hdr_size = hdr_size(a->dims),
        .data = a->data,
        .size = (uint32_t)(aligned_end(a->data, arry_size(id, h)) - a->data),
        .old_end = arry.end,
        .rep = rep,
    };
    copy_mem(f.data, f.data + f.size, arry.end - (f.data + f.size));
    arry.end -= f.size;
    copy_mem(h, h + f.hdr_size, arry.hdr - (h + f.hdr_size));
    arry.hdr -= f.hdr_size;
    for (Addr p = allo.hdr_base; p < arry.hdr; p += hdr_size(hdr_at(p)->dims)) {
        ArrayHdr *const kept = hdr_at(p);
        if (kept->data > f.data)
            kept->data -= f.size;
    }
    follow_owners(&f);
    for (uint16_t c = symb.nvars; c-- > 1;) {
        Var *const var = symb_var(c);
        var->vt = is_array_var(c) ? moved_hdr(&f, var->vt) : moved_cell(&f, var->vt);
    }
    for (uint32_t i = 0; i < vm.ncallvars; i++) {
        CallVar *const cv = &vm.callvars[i];
        if (is_array_var(cv->var))
            cv->array.hdr = moved_hdr(&f, cv->array.hdr);
        else if (cv->cell) /* a parameter's cell before */
            cv->cell = moved_cell(&f, cv->cell);
    }
    return f.hdr_size;
}

/* ---- REDIM PRESERVE ------------------------------------------------------------------------- */

/* Element k of old, by its subscripts, in fresh: *idx; false it is not there. */
static bool index_in(const ArrayHdr *const old, const ArrayHdr *const fresh, uint32_t k,
                     uint32_t *const idx)
{
    uint32_t i = 0;
    uint32_t scale = 1;
    for (uint32_t d = old->dims; d-- > 0;) { /* the last subscript the fastest */
        const uint32_t r = k % count(old, d);
        k /= count(old, d);
        const uint32_t s = r + lbound(old, d) - lbound(fresh, d);
        if (s >= count(fresh, d)) /* past it, or under 0 */
            return false;
        i += s * scale;
        scale *= count(fresh, d);
    }
    *idx = i;
    return true;
}

void arry_keep(const uint16_t id, const Addr old_hdr, const Addr fresh_hdr)
{
    const ArrayHdr *const old = hdr_at(old_hdr);
    const ArrayHdr *const fresh = hdr_at(fresh_hdr);
    const uint32_t esize = arry_esize(id);
    const Var *const v = symb_var(id);
    const bool strings = (v->info & (SK_DBL | VK_MASK)) == VK_STR && v->type < ET_REC;
    const uint32_t total = elem_count(old);
    for (uint32_t k = 0; k < total; k++) {
        uint32_t idx;
        if (!index_in(old, fresh, k, &idx))
            continue;
        const Addr src = old->data + k * esize;
        const Addr dst = fresh->data + idx * esize;
        if (strings)
            flow_lsmove(src, dst); /* its block moves too */
        else
            copy_mem(dst, src, esize);
    }
}

void arry_rep(const uint16_t id)
{
    const Addr h = symb_var(id)->vt;
    for (uint16_t c = symb.nvars; c-- > 1;)
        if (symb_var(c)->vt == AREP)
            symb_var(c)->vt = h;
    for (uint32_t i = 0; i < vm.ncallvars; i++) {
        CallVar *const cv = &vm.callvars[i];
        if (is_array_var(cv->var) && cv->array.hdr == AREP)
            cv->array.hdr = h;
    }
}
