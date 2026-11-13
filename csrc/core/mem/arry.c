#include "mem/arry.h"
#include "comp/decl.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/copy.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "vm/exec.h"
#include "vm/flow.h"

Arry arry;

/* An array header's fields, by Addr: its dimensions, its data, the fast count, then the counts
   and the lower bounds. */
enum { AH_PTR = offsetof(ArrayHdr, data), AH_FAST = offsetof(ArrayHdr, fast), AH_CNT = 7 };
static_assert(AH_CNT == sizeof(ArrayHdr));

static uint16_t count(Addr h, uint32_t i) { return rd16(h + AH_CNT + i * 2); }
static int16_t lbound(Addr h, uint32_t i)
{
    return (int16_t)rd16(h + AH_CNT + rd8(h) * 2u + i * 2);
}
static Addr vt_of(uint16_t id) { return symb.vars.at[1] + id * 8u + offsetof(Var, vt); }

[[noreturn]] void arry_bad_sub(void) { repl_error(E_BS); }

uint32_t arry_esize(uint16_t id)
{
    const Var *v = symb_var(id);
    if (v->type >= ET_REC)
        return decl_types[v->type - ET_REC].size;
    if (v->info & SK_DBL)
        return 8;
    return (v->info & VK_MASK) == VK_STR ? DSZ : VS;
}

Addr arry_ref(uint16_t id, uint8_t n)
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
    if (n != rd8(h))
        arry_bad_sub();
    uint32_t idx = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t r = arry.subs[i] - (uint16_t)lbound(h, i);
        if (r >= count(h, i))
            arry_bad_sub();
        idx += r;
        if (i + 1 < n)
            idx *= count(h, i + 1);
    }
    return rd32(h + AH_PTR) + idx * arry_esize(id);
}

void arry_dim(uint16_t id, uint8_t n, bool dyn)
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

/* The data's end, on 4, for size bytes from arry.end. */
static uint64_t data_end(uint32_t size) { return ((uint64_t)arry.end + size + 3) & ~3ull; }

Addr arry_new(uint16_t id, uint8_t n, bool dyn)
{
    uint64_t size = arry_esize(id);
    for (uint32_t i = 0; i < n; i++) {
        size *= arry.counts[i];
        if (size > 0x100000)
            repl_error(E_OM);
    }
    if (arry.hdr + n * 4u + AH_CNT > allo.hdr_base + HDRSZ)
        repl_error(E_OM);
    if (data_end((uint32_t)size) > strg.low) {
        strg_gc();
        if (data_end((uint32_t)size) > strg.low)
            repl_error(E_OM);
    }
    Addr data = arry.end;
    arry.end = (Addr)data_end((uint32_t)size);
    copy_zero(data, (uint32_t)size);
    Addr h = arry.hdr;
    wr32(h + AH_PTR, data);
    wr16(h + AH_FAST, n == 1 && arry.lbounds[0] == 0 ? arry.counts[0] : 0); /* 1-D from 0 */
    symb_var(id)->vt = h;
    symb_varx(id)->dyn = dyn;
    wr8(h, n);
    memcpy(at(h + AH_CNT), arry.counts, n * 2u);
    memcpy(at(h + AH_CNT + n * 2u), arry.lbounds, n * 2u);
    arry.hdr = h + AH_CNT + n * 4u;
    return h;
}

uint32_t arry_size(uint16_t id, Addr hdr)
{
    uint32_t size = arry_esize(id);
    for (uint32_t i = 0; i < rd8(hdr); i++)
        size *= count(hdr, i);
    return size;
}

/* arry_free's: the header dropped, its bytes, the data dropped [data, data+size), the data's old
   end, what references to the header become. */
static Addr fh, fdata, fend, frep;
static uint32_t fhs, fsize;

/* A header kept at a: past the dropped one it moves down; the dropped one's becomes frep. */
static void follow_hdr(Addr a)
{
    Addr x = rd32(a);
    if (x < fh)
        return;
    if (x == fh)
        wr32(a, frep);
    else if (x < arry.hdr + fhs)
        wr32(a, x - fhs);
}

/* A parameter's cell kept at a: in the data moved down, it follows. */
static void follow_cell(Addr a)
{
    Addr x = rd32(a);
    if (x >= fdata && x - fdata >= fsize && x < fend)
        wr32(a, x - fsize);
}

uint32_t arry_free(uint16_t id, Addr rep)
{
    Addr h = symb_var(id)->vt;
    symb_var(id)->vt = 0;
    fh = h;
    frep = rep;
    fdata = rd32(h + AH_PTR);
    fsize = (uint32_t)(((uint64_t)fdata + arry_size(id, h) + 3) & ~3ull) - fdata;
    fhs = rd8(h) * 4u + AH_CNT;
    copy_mem(fdata, fdata + fsize, arry.end - (fdata + fsize));
    fend = arry.end;
    arry.end -= fsize;
    copy_mem(h, h + fhs, arry.hdr - (h + fhs));
    arry.hdr -= fhs;
    for (Addr p = allo.hdr_base; p < arry.hdr; p += rd8(p) * 4u + AH_CNT) {
        Addr d = rd32(p + AH_PTR);
        if (d > fdata)
            wr32(p + AH_PTR, d - fsize);
    }
    for (Addr b = allo.top; b > strg.low;) { /* the moved string blocks' owners */
        b -= BHDR;
        uint16_t len = rd16(b);
        Addr own = rd32(b + offsetof(StrTail, owner));
        if (own >= fdata && own < fend)
            wr32(b + offsetof(StrTail, owner), own - fsize);
        b -= len;
    }
    for (uint32_t c = symb.nvars - 1u; c > 0 && c < symb.nvars; c--) {
        if (symb_var((uint16_t)c)->info & VI_ARR)
            follow_hdr(vt_of((uint16_t)c));
        else
            follow_cell(vt_of((uint16_t)c));
    }
    for (Addr e = exec.lsav; e < exec.lsp; e += LSSZ) { /* save entries: id:2 ref:2 value:8 off:4 */
        if (symb_var(rd16(e))->info & VI_ARR)
            follow_hdr(e + 4);
        if (rd16(e + 2) == 0xFFFE) /* a parameter's cell before */
            follow_cell(e + 12);
    }
    return fhs;
}

void arry_keep(uint16_t id, Addr old, Addr fresh)
{
    uint32_t n = rd8(old), esz = arry_esize(id), total = 1;
    for (uint32_t i = 0; i < n; i++)
        total *= count(old, i);
    const Var *v = symb_var(id);
    bool str = (v->info & (SK_DBL | VK_MASK)) == VK_STR && v->type < ET_REC;
    for (uint32_t k = 0; k < total; k++) {
        uint32_t rest = k, idx = 0, mul = 1, c = n;
        for (; c > 0; c--) { /* the last subscript the fastest */
            uint32_t r = rest % count(old, c - 1);
            rest /= count(old, c - 1);
            uint32_t s = r + lbound(old, c - 1) - lbound(fresh, c - 1);
            if (s >= count(fresh, c - 1))
                break; /* not in the new one, or under 0 */
            idx += s * mul;
            mul *= count(fresh, c - 1);
        }
        if (c)
            continue;
        Addr src = rd32(old + AH_PTR) + k * esz, dst = rd32(fresh + AH_PTR) + idx * esz;
        if (str)
            flow_lsmove(src, dst); /* its block moves too */
        else
            copy_mem(dst, src, esz);
    }
}

void arry_rep(uint16_t id)
{
    Addr h = symb_var(id)->vt;
    for (uint32_t c = symb.nvars - 1u; c > 0 && c < symb.nvars; c--)
        if (symb_var((uint16_t)c)->vt == AREP)
            symb_var((uint16_t)c)->vt = h;
    for (Addr e = exec.lsav; e < exec.lsp; e += LSSZ)
        if ((symb_var(rd16(e))->info & VI_ARR) && rd32(e + 4) == AREP)
            wr32(e + 4, h);
}
