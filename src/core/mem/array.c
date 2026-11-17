/* Arrays: their headers and data, made, indexed, freed and resized.
 *
 * # How it works
 * An array's cell (ArrSlot) holds its header (ArrayHdr) in the headers' region, from allo.hdr_base
 * to arry.hdr. The cell is the variable's own, a local's in its call's frame, a parameter's its
 * argument's (symbols.h symb_at). The header:
 * its dimensions, its data's Addr, the count when 1-D from 0 (else 0), the counts, then the lower
 * bounds. Its data is on the heap from allo.bot to allo.arrays, zeroed, 4-aligned: VS a number or an
 * INT, 8 a DOUBLE, DSZ a string, a TYPE's size a record. The first subscript varies the fastest, as
 * in QB 4.5.
 *
 * A local array DIMmed with constant bounds has its header and data in its call's frame
 * (comp/frame.c), zeroed by PCALL, gone with the frame; never freed or REDIMmed.
 *
 * arry_free drops an array: its header and data go and what follows moves down. The headers' data
 * Addrs, the moved string blocks' owners, the headers in the cells, the frames' too, and the
 * parameters' cells in the moved data follow.
 *
 * # asm structure (retired/asm32/mem/arr.asm)
 * | asm | here | |
 * |---|---|---|
 * | mm.aend, mm.ahd | Arry.end, Arry.hdr | MIRROR |
 * | g_dimbuf, g_cntbuf, g_lbbuf | Arry.subs, Arry.counts, Arry.lbounds (ADIMS) | MIRROR |
 * | AH_PTR AH_FAST AH_CNT | ArrayHdr (defs.h) .data .fast .count[] | MOVED: defs.h; D1 |
 * | arr_ref (EBX, narr, dimbuf -> EDI) | arry_ref(id, n) | MIRROR; D2 |
 * | arr_bs | arry_bad_sub | MIRROR |
 * | arr_esz | arry_esize | MIRROR |
 * | arr_dim, arr_new, arr_size | arry_dim, arry_new, arry_size | MIRROR; D2 |
 * | arr_free | arry_free(id, rep) | MIRROR; D2, D3 |
 * | arr_keep, arr_rep | arry_keep, arry_rep | MIRROR; D2 |
 * | arr_chk | static | MIRROR |
 *
 * - D1. The lower bounds follow the counts in the header.
 * - D2. Parameters, not globals: mm.aid is id, rn.narr is n, g_dyn is dyn, g_pold and g_pnew are
 *   old and fresh. The other scratch globals (mm.asz, g_afh, g_kn ...) are locals.
 * - D3. rep is 0 or AREP (g_afrep); the return value is the header's bytes (g_afhs).
 */
#include "mem/array.h"
#include "mem/copy.h"
#include "vm/flow.h"

enum { DATA_MAX = 0x100000, DATA_ALIGN = 4 };

void arry_init(
    Arry *const ay,
    Allo *const al,
    Fault *const f,
    Symb *const sy,
    const Decl *const d,
    Strg *const sg,
    const Addr *const fp
)
{
    ay->fp = fp;
    ay->allo = al;
    ay->fault = f;
    ay->symb = sy;
    ay->decl = d;
    ay->strg = sg;
}

static ArrayHdr *arry_hdr_at(const Addr h) { return (ArrayHdr *)at(h); }

ArrSlot *arry_slot(const Arry *const ay, const uint16_t id)
{
    return (ArrSlot *)at(symb_at(ay->symb, *ay->fp, id));
}

/* A header's bytes: the fixed part, then a count and a lower bound a dimension. */
static uint32_t arry_hdr_size(const uint8_t dims)
{
    return sizeof(ArrayHdr) + dims * 2u * sizeof(uint32_t);
}

static uint32_t arry_count(const ArrayHdr *const a, const uint32_t i) { return a->count[i]; }
static int32_t arry_lbound(const ArrayHdr *const a, const uint32_t i)
{
    return (int32_t)a->count[a->dims + i];
}

static uint32_t arry_elem_count(const ArrayHdr *const a)
{
    uint32_t n = 1;

    for (uint32_t i = 0; i < a->dims; i++)
        n *= arry_count(a, i);

    return n;
}

/* size bytes from a, to the next DATA_ALIGN. */
static uint64_t arry_aligned_end(const Addr a, const uint32_t size)
{
    return ((uint64_t)a + size + DATA_ALIGN - 1) & ~(uint64_t)(DATA_ALIGN - 1);
}

static bool arry_is_array(const Arry *const ay, const uint16_t id)
{
    return symb_var(ay->symb, id)->info & VI_ARR;
}

[[noreturn]]
void arry_bad_sub(const Arry *const ay)
{
    fault_error(ay->fault, E_BS);
}

uint32_t arry_elem_size(const Symb *const sy, const Decl *const d, const uint16_t id)
{
    const Var *const v = symb_var(sy, id);

    if (v->type >= ET_REC)
        return decl_type(d, v->type - ET_REC)->size;
    if (v->info & SK_DBL)
        return 8;

    return (v->info & VK_MASK) == VK_STR ? DSZ : VS;
}

uint32_t arry_esize(const Arry *const ay, const uint16_t id)
{
    return arry_elem_size(ay->symb, ay->decl, id);
}

uint32_t arry_frame_hdr(const uint8_t dims) { return (arry_hdr_size(dims) + 7) & ~7u; }

static_assert(ARRY_FRAME_HDR1 == ((sizeof(ArrayHdr) + 2 * sizeof(uint32_t) + 7) & ~7u));

bool arry_heap(const Arry *const ay, const Addr h)
{
    return h >= ay->allo->hdr_base && h < ay->hdr;
}

/* Array id's header made in its frame (comp/frame.c), arry.counts and arry.lbounds [0..n) its
   shape; false: none there, or too small for it. */
static bool arry_in_frame(Arry *const ay, const uint16_t id, const uint8_t n)
{
    const FrameArr *const fa = symb_farr(ay->symb, id);
    uint64_t elems = 1;

    for (uint32_t i = 0; i < n && elems <= 0xFFFFFF; i++)
        elems *= ay->counts[i];

    if (!fa->at || fa->shape >> 24 != n ||
        elems > (fa->shape & 0xFFFFFF)) /* at: a local's (comp/frame.c) */
        return false;

    const Addr h = *ay->fp + (uint32_t)fa->at; /* the data after it, zeroed by PCALL */
    ArrayHdr *const a = arry_hdr_at(h);
    a->dims = n;
    a->data = h + arry_frame_hdr(n);
    a->fast = n == 1 && ay->lbounds[0] == 0 ? ay->counts[0] : 0;
    memcpy(&a->count[0], ay->counts, n * sizeof ay->counts[0]);
    memcpy(&a->count[n], ay->lbounds, n * sizeof ay->lbounds[0]);
    *arry_slot(ay, id) = (ArrSlot){h, false};

    return true;
}

Addr arry_ref(Arry *const ay, ArrSlot *const s, const uint16_t id, const uint8_t n)
{
    Addr h = s->hdr;

    if (!h) {
        if (s->dyn) /* ERASE freed it: not made again */
            arry_bad_sub(ay);
        for (uint32_t i = 0; i < n; i++) { /* OPTION BASE to 10 */
            ay->counts[i] = 11 - ay->decl->base;
            ay->lbounds[i] = ay->decl->base;
        }

        h = arry_new(ay, id, n, false);
    }

    const ArrayHdr *const a = arry_hdr_at(h);

    if (n != a->dims)
        arry_bad_sub(ay);

    uint32_t idx = 0; /* the first subscript the fastest, as QB 4.5 */
    for (uint32_t i = n; i-- > 0;) {
        const uint32_t r = (uint32_t)ay->subs[i] - (uint32_t)arry_lbound(a, i);

        if (r >= arry_count(a, i))
            arry_bad_sub(ay);

        idx = idx * arry_count(a, i) + r;
    }

    return a->data + idx * arry_esize(ay, id);
}

void arry_dim(Arry *const ay, const uint16_t id, const uint8_t n, const bool dyn)
{
    for (uint32_t i = n; i-- > 0;) {
        if (ay->subs[i] < 0)
            fault_error(ay->fault, E_IQ);

        ay->counts[i] = (uint32_t)ay->subs[i] + 1;
    }

    if (arry_slot(ay, id)->hdr)
        fault_error(ay->fault, E_RD);
    if (!dyn && arry_in_frame(ay, id, n))
        return;

    arry_new(ay, id, n, dyn);
}

Addr arry_new(Arry *const ay, const uint16_t id, const uint8_t n, const bool dyn)
{
    uint64_t size = arry_esize(ay, id);

    for (uint32_t i = 0; i < n; i++) {
        size *= ay->counts[i];

        if (size > DATA_MAX)
            fault_error(ay->fault, E_OM);
    }

    if (ay->hdr + arry_hdr_size(n) > ay->allo->hdr_base + HDRSZ)
        fault_error(ay->fault, E_OM);
    if (arry_aligned_end(ay->allo->arrays, (uint32_t)size) > ay->allo->strings) {
        strg_gc(ay->strg);

        if (arry_aligned_end(ay->allo->arrays, (uint32_t)size) > ay->allo->strings)
            fault_error(ay->fault, E_OM);
    }

    const Addr data = ay->allo->arrays;
    ay->allo->arrays = (Addr)arry_aligned_end(data, (uint32_t)size);
    copy_zero(data, (uint32_t)size);

    const Addr h = ay->hdr;
    ArrayHdr *const a = arry_hdr_at(h);
    a->dims = n;
    a->data = data;
    a->fast = n == 1 && ay->lbounds[0] == 0 ? ay->counts[0] : 0; /* 1-D from 0 */
    memcpy(&a->count[0], ay->counts, n * sizeof ay->counts[0]);
    memcpy(&a->count[n], ay->lbounds, n * sizeof ay->lbounds[0]);
    ay->hdr = h + arry_hdr_size(n);
    *arry_slot(ay, id) = (ArrSlot){h, dyn};

    return h;
}

uint32_t arry_size(const Arry *const ay, const uint16_t id, const Addr hdr)
{
    return arry_esize(ay, id) * arry_elem_count(arry_hdr_at(hdr));
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
static Addr arry_moved_hdr(const Arry *const ay, const Freed *const f, const Addr h)
{
    if (h < f->hdr)
        return h;
    if (h == f->hdr)
        return f->rep;

    return h < ay->hdr + f->hdr_size ? h - f->hdr_size : h;
}

/* A parameter's cell kept: in the data moved down, it follows. */
static Addr arry_moved_cell(const Freed *const f, const Addr c)
{
    const bool moved = c >= f->data && c - f->data >= f->size && c < f->old_end;
    return moved ? c - f->size : c;
}

/* Each array's cell holding a header: the variables' own, then each call's locals' in its frame. */
static void arry_each_slot(
    const Arry *const ay,
    const Vm *const vm,
    void (*const each)(const Arry *, ArrSlot *, const Freed *),
    const Freed *const f
)
{
    const Symb *const sy = ay->symb;

    for (uint16_t c = sy->nvars; c-- > 1;)
        if (arry_is_array(ay, c))
            each(ay, (ArrSlot *)at(symb_cell(sy, c)), f);

    for (uint32_t i = 0; i < vm_nframes(vm); i++) {
        const Frame *const fr = &vm->frames[i];

        if (fr->kind != FRAME_CALL)
            continue;

        for (uint16_t c = sy->first_local[fr->call.proc]; c; c = symb_varx(sy, c)->nloc)
            if (arry_is_array(ay, c) && symb_is_local(sy, c))
                each(ay, (ArrSlot *)at(loc_frame_at(*symb_loc(sy, c), fr->call.fp)), f);
    }
}

static void arry_slot_moved(const Arry *const ay, ArrSlot *const s, const Freed *const f)
{
    if (s->hdr) /* a frame's is past the headers' region: it stays */
        s->hdr = arry_moved_hdr(ay, f, s->hdr);
}

/* The moved string blocks' owners follow. */
static void arry_follow_owners(const Arry *const ay, const Freed *const f)
{
    for (Addr b = ay->allo->top; b > ay->allo->strings;) {
        b -= BHDR;
        StrTail *const tail = (StrTail *)at(b);

        if (tail->owner >= f->data && tail->owner < f->old_end)
            tail->owner -= f->size;

        b -= tail->len;
    }
}

uint32_t arry_free(Arry *const ay, Vm *const vm, const uint16_t id, const Addr rep)
{
    ArrSlot *const s = arry_slot(ay, id);
    const Addr h = s->hdr;
    s->hdr = 0;
    const ArrayHdr *const a = arry_hdr_at(h);
    const Freed f = {
        .hdr = h,
        .hdr_size = arry_hdr_size(a->dims),
        .data = a->data,
        .size = (uint32_t)(arry_aligned_end(a->data, arry_size(ay, id, h)) - a->data),
        .old_end = ay->allo->arrays,
        .rep = rep,
    };

    copy_mem(f.data, f.data + f.size, ay->allo->arrays - (f.data + f.size));
    ay->allo->arrays -= f.size;
    copy_mem(h, h + f.hdr_size, ay->hdr - (h + f.hdr_size));
    ay->hdr -= f.hdr_size;

    for (Addr p = ay->allo->hdr_base; p < ay->hdr; p += arry_hdr_size(arry_hdr_at(p)->dims)) {
        ArrayHdr *const kept = arry_hdr_at(p);

        if (kept->data > f.data)
            kept->data -= f.size;
    }

    arry_follow_owners(ay, &f);

    arry_each_slot(ay, vm, arry_slot_moved, &f);

    for (uint32_t i = 0; i < vm_nframes(vm); i++) { /* the arguments: an element's address */
        const Call *const c = &vm->frames[i].call;

        if (vm->frames[i].kind != FRAME_CALL)
            continue;

        Cell *const refs = (Cell *)at(c->fp);

        for (uint32_t k = 0; k < c->npar; k++)
            refs[2 * k].a = arry_moved_cell(&f, refs[2 * k].a);
    }

    return f.hdr_size;
}

/* ---- REDIM PRESERVE ------------------------------------------------------------------------- */

/* Element k of old, by its subscripts, in fresh: *idx; false it is not there. */
static bool arry_index_in(
    const ArrayHdr *const old,
    const ArrayHdr *const fresh,
    uint32_t k,
    uint32_t *const idx
)
{
    uint32_t i = 0;
    uint32_t scale = 1;

    for (uint32_t d = 0; d < old->dims; d++) { /* the first subscript the fastest */
        const uint32_t r = k % arry_count(old, d);
        k /= arry_count(old, d);
        const int64_t s = (int64_t)r + arry_lbound(old, d) - arry_lbound(fresh, d);

        if (s < 0 || s >= arry_count(fresh, d))
            return false;

        i += (uint32_t)s * scale;
        scale *= arry_count(fresh, d);
    }

    *idx = i;
    return true;
}

void arry_keep(Arry *const ay, const uint16_t id, const Addr old_hdr, const Addr fresh_hdr)
{
    const ArrayHdr *const old = arry_hdr_at(old_hdr);
    const ArrayHdr *const fresh = arry_hdr_at(fresh_hdr);
    const uint32_t esize = arry_esize(ay, id);
    const Var *const v = symb_var(ay->symb, id);
    const bool strings = (v->info & (SK_DBL | VK_MASK)) == VK_STR && v->type < ET_REC;
    const uint32_t total = arry_elem_count(old);

    for (uint32_t k = 0; k < total; k++) {
        uint32_t idx;

        if (!arry_index_in(old, fresh, k, &idx))
            continue;

        const Addr src = old->data + k * esize;
        const Addr dst = fresh->data + idx * esize;

        if (strings)
            strg_move(ay->strg, src, dst); /* its block moves too */
        else
            copy_mem(dst, src, esize);
    }
}

static void arry_slot_rep(const Arry *const ay, ArrSlot *const s, const Freed *const f)
{
    if (s->hdr == AREP)
        s->hdr = f->rep;
}

void arry_rep(Arry *const ay, Vm *const vm, const uint16_t id)
{
    const Freed f = {.rep = arry_slot(ay, id)->hdr};

    arry_each_slot(ay, vm, arry_slot_rep, &f);
}
