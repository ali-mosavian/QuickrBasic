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
 * The running call's other arrays, made at a statement's start, are its arena's (vm/stack.c): the
 * header then the data, under its locals, gone at its return with no work. Freeing the newest gives
 * its room back; an older one stays dead until the return. REDIM PRESERVE of the newest makes the
 * new one under it, moves the elements, then slides it up into the old one's room. The arrays made
 * mid-statement, in a GOSUB, too big for the room, or in a caller's cell are the heap's.
 *
 * A fixed array field passed whole to an array parameter gets a field cell: the cell and a header
 * at the headers' region's top, growing down, the header's data the field. A call's field cells
 * lie above its arguments' stack: given back once vm.sp is above them, their call over. Their data
 * follows its record as the heap moves.
 *
 * A list (Quickr's) is a 1-D array from 0 with room to grow: its count its capacity, its fast its
 * length, which bounds its subscripts. A QBasic array's fast is its count, so it is unchanged.
 *
 * A heap header names its owner, the cell holding it (ours): a variable's by its id, else the
 * cell's Addr, which may be a record's in the heap; an arena's names OWN_ARENA. arry_kill marks an
 * array dead (owner 0), and the arrays its records hold; arry_sweep removes the dead: each one's
 * header and data go and what follows moves down. The headers' data, their owners in the moved
 * data, the cells of the moved headers, the moved string blocks' owners and the parameters' cells
 * in the moved data follow.
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
 * - D3. arry_free takes the cell; the asm's rep (AREP) and its scan of the cells are gone: the
 *   header's owner is its cell.
 */
#include "mem/array.h"
#include "mem/copy.h"
#include "vm/flow.h"
#include "vm/stack.h"

enum { DATA_ALIGN = 4, BLOCK_ALIGN = 8 /* a frame's or the arena's: a cell */ };

void arry_init(
    Arry *const ay,
    Allo *const al,
    Fault *const f,
    Symb *const sy,
    const Decl *const d,
    Strg *const sg,
    Vm *const vm
)
{
    ay->vm = vm;
    ay->allo = al;
    ay->fault = f;
    ay->symb = sy;
    ay->decl = d;
    ay->strg = sg;
}

static ArrayHdr *arry_hdr_at(const Addr h) { return (ArrayHdr *)at(h); }

void arry_clear_heap(Arry *const ay)
{
    ay->hdr = ay->allo->hdr_base;
    ay->fcells = ay->allo->hdr_base + HDRSZ;
}

ArrSlot *arry_slot(const Arry *const ay, const uint32_t id)
{
    return (ArrSlot *)at(symb_at(ay->symb, ay->vm->fp, id));
}

/* A header's owner, ours: a variable's own cell by its id (the cells move as the table grows), else
   its cell's Addr; an arena's ARENA. */
enum : Addr { OWN_VAR = 0x80000000u, OWN_ARENA = 1 };

static Addr arry_owner(const Arry *const ay, const ArrSlot *const s)
{
    const Addr a = addr_of(s), cells = symb_cell(ay->symb, 0);

    if (a >= cells && a < symb_cell(ay->symb, ay->symb->nvars))
        return OWN_VAR | (a - cells) / (uint32_t)sizeof(Cell);

    return a;
}

static ArrSlot *arry_owner_slot(const Arry *const ay, const Addr owner)
{
    return (ArrSlot *)at(owner & OWN_VAR ? symb_cell(ay->symb, owner & ~OWN_VAR) : owner);
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

[[noreturn]]
void arry_bad_sub(const Arry *const ay)
{
    fault_error(ay->fault, E_BS);
}

uint32_t arry_frame_hdr(const uint8_t dims)
{
    return (arry_hdr_size(dims) + BLOCK_ALIGN - 1) & ~(uint32_t)(BLOCK_ALIGN - 1);
}

static_assert(ARRY_FRAME_HDR1 == ((sizeof(ArrayHdr) + 2 * sizeof(uint32_t) + 7) & ~7u));

/* Header h's records' array fields empty, ours. */
static void arry_empty_recs(const Arry *const ay, const Addr h)
{
    const ArrayHdr *const a = arry_hdr_at(h);

    if (a->et >= ET_REC)
        arry_empty_cells(a->data, arry_elem_count(a), decl_type(ay->decl, a->et - ET_REC));
}

/* Header h is the heap's (arry_make's), not a frame's or an arena's. */
static bool arry_heap(const Arry *const ay, const Addr h)
{
    return h >= ay->allo->hdr_base && h < ay->hdr;
}

static bool arry_arena(const Addr h) { return arry_hdr_at(h)->owner == OWN_ARENA; }

bool arry_fixed(const Arry *const ay, const Addr h) { return !arry_heap(ay, h) && !arry_arena(h); }

/* A field cell: the stack its call's arguments lie above, the cell, then its header. */
typedef struct {
    Addr sp;
    ArrSlot slot;
} FieldCell;

static Addr arry_fcells_end(const Arry *const ay) { return ay->allo->hdr_base + HDRSZ; }

static ArrayHdr *arry_fcell_hdr(const Addr c) { return arry_hdr_at(c + sizeof(FieldCell)); }

static uint32_t arry_fcell_size(const uint8_t dims)
{
    return (sizeof(FieldCell) + arry_hdr_size(dims) + BLOCK_ALIGN - 1) &
           ~(uint32_t)(BLOCK_ALIGN - 1);
}

static Addr arry_fcell_next(const Addr c) { return c + arry_fcell_size(arry_fcell_hdr(c)->dims); }

/* Header h is a field cell's. */
static bool arry_fcell(const Arry *const ay, const Addr h)
{
    return h >= ay->fcells && h < arry_fcells_end(ay);
}

/* The field cells of the calls over given back: their stack under vm.sp. */
static void arry_fcells_drop(Arry *const ay)
{
    const Addr sp = addr_of(ay->vm->sp);

    while (ay->fcells < arry_fcells_end(ay) && ((const FieldCell *)at(ay->fcells))->sp <= sp)
        ay->fcells = arry_fcell_next(ay->fcells);
}

/* Header h filled: arry.counts and arry.lbounds [0..n) its shape, its et elements at data. */
static void arry_fill(
    const Arry *const ay,
    const Addr h,
    const Addr data,
    const uint32_t et,
    const uint8_t n,
    const Addr owner
)
{
    ArrayHdr *const a = arry_hdr_at(h);
    a->dims = n;
    a->et = et;
    a->data = data;
    a->fast = n == 1 && ay->lbounds[0] == 0 ? ay->counts[0] : 0; /* 1-D from 0 */
    a->owner = owner;
    memcpy(&a->count[0], ay->counts, n * sizeof ay->counts[0]);
    memcpy(&a->count[n], ay->lbounds, n * sizeof ay->lbounds[0]);
}

/* Array id's header made in its frame (comp/frame.c), arry.counts and arry.lbounds [0..n) its
   shape; false: none there, or too small for it. */
static bool arry_in_frame(Arry *const ay, const uint32_t id, const uint8_t n)
{
    const FrameArr *const fa = symb_farr(ay->symb, id);
    uint64_t elems = 1;

    for (uint32_t i = 0; i < n && elems <= 0xFFFFFF; i++)
        elems *= ay->counts[i];

    if (!fa->at || fa->shape >> 24 != n ||
        elems > (fa->shape & 0xFFFFFF)) /* at: a local's (comp/frame.c) */
        return false;

    const Addr h = ay->vm->fp + (uint32_t)fa->at; /* the data after it, zeroed by PCALL */
    ArrSlot *const s = arry_slot(ay, id);
    arry_fill(ay, h, h + arry_frame_hdr(n), arry_var_et(ay->symb, id), n, arry_owner(ay, s));
    *s = (ArrSlot){h, false};

    arry_empty_recs(ay, h);

    return true;
}

Addr arry_ref(Arry *const ay, ArrSlot *const s, const uint32_t id, const uint8_t n)
{
    Addr h = s->hdr;

    if (!h) {
        if (s->dyn) /* ERASE freed it: not made again */
            arry_bad_sub(ay);

        const uint8_t base = symb_varx(ay->symb, id)->base;

        for (uint32_t i = 0; i < n; i++) { /* its file's OPTION BASE to 10 */
            ay->counts[i] = 11 - base;
            ay->lbounds[i] = base;
        }

        h = arry_new(ay, id, n, false);
    }

    const ArrayHdr *const a = arry_hdr_at(h);

    if (n != a->dims)
        arry_bad_sub(ay);

    uint32_t idx = 0; /* the first subscript the fastest, as QB 4.5 */
    for (uint32_t i = n; i-- > 0;) {
        const uint32_t r = (uint32_t)ay->subs[i] - (uint32_t)arry_lbound(a, i);

        if (r >= arry_count(a, i) || (a->fast && r >= a->fast))
            arry_bad_sub(ay);

        idx = idx * arry_count(a, i) + r;
    }

    return a->data + idx * arry_et_size(ay->decl, a->et);
}

void arry_counts(Arry *const ay, const uint8_t n)
{
    for (uint32_t i = n; i-- > 0;) {
        if (ay->subs[i] < 0)
            fault_error(ay->fault, E_IQ);

        ay->counts[i] = (uint32_t)ay->subs[i] + 1;
    }
}

void arry_dim(Arry *const ay, const uint32_t id, const uint8_t n)
{
    arry_counts(ay, n);

    if (arry_slot(ay, id)->hdr)
        fault_error(ay->fault, E_RD);
    if (!arry_in_frame(ay, id, n))
        arry_new(ay, id, n, false);
}

void arry_empty(Arry *const ay, const uint32_t id, const bool keep)
{
    ArrSlot *const s = arry_slot(ay, id);

    if (s->hdr && !keep)
        fault_error(ay->fault, E_RD);
    if (!s->hdr)
        s->dyn = 1;
}

/* The call whose frame holds cell s, a local's or in one of its records, marked to free its heap
   arrays at its return (vm/flow.c); none for a variable's or a heap array's cell. Only such calls
   take the slow return. */
static void arry_own_call(const Arry *const ay, const Addr s)
{
    Vm *const vm = ay->vm;

    if (s < ay->allo->top) /* the variables and the heap: no call's */
        return;

    for (uint32_t i = vm_nframes(vm); i-- > 0;) { /* its locals under its fp, above its callees' */
        Frame *const f = &vm->frames[i];

        if (!frame_is_call(f) || s >= f->call.fp)
            continue;
        if (f->kind == FRAME_CALL)
            f->call.back = nullptr; /* the inline PCALL left it */

        f->kind = FRAME_CALL_X;
        f->call.heap = true;
        return;
    }
}

/* size bytes of data for cell s's array of n dimensions on the heap: its header's Addr, *data the
   data's. Out of memory. */
static Addr arry_heap_take(
    Arry *const ay,
    const ArrSlot *const s,
    const uint8_t n,
    const uint32_t size,
    Addr *const data
)
{
    arry_fcells_drop(ay); /* their room first */

    if (ay->hdr + arry_hdr_size(n) > ay->fcells)
        fault_error(ay->fault, E_OM);
    if (arry_aligned_end(ay->allo->arrays, size) > ay->allo->strings) {
        strg_gc(ay->strg);

        if (arry_aligned_end(ay->allo->arrays, size) > ay->allo->strings)
            fault_error(ay->fault, E_OM);
    }

    arry_own_call(ay, addr_of(s));
    *data = ay->allo->arrays;
    ay->allo->arrays = (Addr)arry_aligned_end(*data, size);

    const Addr h = ay->hdr;
    ay->hdr = h + arry_hdr_size(n);
    return h;
}

/* A block's bytes in a frame or the arena: the header, then size bytes of data. */
static uint64_t arry_block_of(const uint8_t n, const uint64_t size)
{
    return arry_frame_hdr(n) + ((size + BLOCK_ALIGN - 1) & ~(uint64_t)(BLOCK_ALIGN - 1));
}

static uint32_t arry_block(const Arry *const ay, const Addr h)
{
    return (uint32_t)arry_block_of(arry_hdr_at(h)->dims, arry_size(ay, h));
}

/* arry_make's array; arena: on the running call's arena if it can be. */
static Addr arry_make_on(
    Arry *const ay,
    ArrSlot *const s,
    const uint32_t et,
    const uint8_t n,
    const bool dyn,
    const bool arena
)
{
    uint64_t size = arry_et_size(ay->decl, et);

    for (uint32_t i = 0; i < n; i++) {
        size *= ay->counts[i];

        if (size > REF_ADDR) /* past any heap */
            fault_error(ay->fault, E_OM);
    }

    const uint64_t block = arry_block_of(n, size);
    Addr h =
        arena && block <= UINT32_MAX ? stack_arena_take(ay->vm, addr_of(s), (uint32_t)block) : 0;
    Addr data = h + arry_frame_hdr(n);

    if (!h)
        h = arry_heap_take(ay, s, n, (uint32_t)size, &data);

    copy_zero(data, (uint32_t)size);
    arry_fill(ay, h, data, et, n, arry_heap(ay, h) ? arry_owner(ay, s) : OWN_ARENA);
    *s = (ArrSlot){h, dyn};

    arry_empty_recs(ay, h);

    return h;
}

Addr arry_make(Arry *const ay, ArrSlot *const s, const uint32_t et, const uint8_t n, const bool dyn)
{
    return arry_make_on(ay, s, et, n, dyn, true);
}

void arry_empty_cells(const Addr data, const uint32_t n, const TypeDef *const td)
{
    for (uint32_t k = 0; td->nslot && k < n; k++)
        for (uint32_t i = 0; i < td->nslot; i++)
            *(ArrSlot *)at(data + k * td->size + td->slots[i]) = (ArrSlot){0, 1};
}

Addr arry_new(Arry *const ay, const uint32_t id, const uint8_t n, const bool dyn)
{
    return arry_make(ay, arry_slot(ay, id), arry_var_et(ay->symb, id), n, dyn);
}

uint32_t arry_size(const Arry *const ay, const Addr hdr)
{
    const ArrayHdr *const a = arry_hdr_at(hdr);
    return arry_et_size(ay->decl, a->et) * arry_elem_count(a);
}

/* ---- freeing -------------------------------------------------------------------------------- */

/* An array being removed: its header and the header's bytes, its data [data, data + size), the
   data's old end. */
typedef struct {
    Addr hdr;
    uint32_t hdr_size;
    Addr data;
    uint32_t size;
    Addr old_end;
} Freed;

/* A cell kept: in the data moved down, it follows. */
static Addr arry_moved_cell(const Freed *const f, const Addr c)
{
    const bool moved = c >= f->data && c - f->data >= f->size && c < f->old_end;
    return moved ? c - f->size : c;
}

/* In the data removed. */
static bool arry_in_removed(const Freed *const f, const Addr c)
{
    return c >= f->data && c - f->data < f->size;
}

/* The moved string blocks' owners follow; a block owned in the data removed is dead. */
static void arry_follow_owners(const Arry *const ay, const Freed *const f)
{
    for (Addr b = ay->allo->top; b > ay->allo->strings;) {
        b -= BHDR;
        StrTail *const tail = (StrTail *)at(b);

        if (arry_in_removed(f, tail->owner))
            tail->owner = 0;
        else
            tail->owner = arry_moved_cell(f, tail->owner);

        b -= tail->len;
    }
}

/* Dead header h's array removed: its header and data go and what follows moves down. The headers'
   data, their owners in the moved data, the cells of the headers moved, the moved string blocks'
   owners and the parameters' cells in the moved data follow. */
static void arry_remove(Arry *const ay, Vm *const vm, const Addr h)
{
    const ArrayHdr *const a = arry_hdr_at(h);
    const Freed f = {
        .hdr = h,
        .hdr_size = arry_hdr_size(a->dims),
        .data = a->data,
        .size = (uint32_t)(arry_aligned_end(a->data, arry_size(ay, h)) - a->data),
        .old_end = ay->allo->arrays,
    };

    copy_mem(f.data, f.data + f.size, ay->allo->arrays - (f.data + f.size));
    ay->allo->arrays -= f.size;
    copy_mem(h, h + f.hdr_size, ay->hdr - (h + f.hdr_size));
    ay->hdr -= f.hdr_size;

    for (Addr p = ay->allo->hdr_base; p < ay->hdr; p += arry_hdr_size(arry_hdr_at(p)->dims)) {
        ArrayHdr *const kept = arry_hdr_at(p);

        if (kept->data > f.data)
            kept->data -= f.size;
        if (!kept->owner) /* dead: removed next */
            continue;
        if (!(kept->owner & OWN_VAR) && arry_in_removed(&f, kept->owner)) { /* its cell gone */
            kept->owner = 0;
            continue;
        }

        if (!(kept->owner & OWN_VAR))
            kept->owner = arry_moved_cell(&f, kept->owner);

        arry_owner_slot(ay, kept->owner)->hdr = p;
    }

    arry_follow_owners(ay, &f);

    for (Addr c = ay->fcells; c < arry_fcells_end(ay); c = arry_fcell_next(c))
        arry_fcell_hdr(c)->data = arry_moved_cell(&f, arry_fcell_hdr(c)->data);

    for (uint32_t i = 0; i < vm_nframes(vm); i++) { /* the arguments: an element's address */
        const Call *const c = &vm->frames[i].call;

        if (!frame_is_call(&vm->frames[i]))
            continue;

        Cell *const refs = (Cell *)at(c->fp);

        for (uint32_t k = 0; k < c->npar; k++)
            refs[2 * k].a = arry_moved_cell(&f, refs[2 * k].a);
    }
}

/* The arrays the n et elements at data hold dead, ours, a record's. */
static void arry_kill_recs(const Arry *const ay, const Addr data, const uint32_t n, const uint32_t et)
{
    if (et < ET_REC)
        return;

    const TypeDef *const td = decl_type(ay->decl, et - ET_REC);

    for (uint32_t k = 0; td->nslot && k < n; k++)
        for (uint32_t i = 0; i < td->nslot; i++)
            arry_kill(ay, ((const ArrSlot *)at(data + k * td->size + td->slots[i]))->hdr);
}

void arry_kill(const Arry *const ay, const Addr h)
{
    if (!h)
        return;
    const ArrayHdr *const a = arry_hdr_at(h);

    if (arry_heap(ay, h)) /* a frame's goes with the frame, its records' arrays do not */
        arry_hdr_at(h)->owner = 0;

    arry_kill_recs(ay, a->data, arry_elem_count(a), a->et);
}

void arry_sweep(Arry *const ay, Vm *const vm)
{
    for (Addr p = ay->allo->hdr_base; p < ay->hdr;)
        if (arry_hdr_at(p)->owner)
            p += arry_hdr_size(arry_hdr_at(p)->dims);
        else
            arry_remove(ay, vm, p); /* the next one moves down to p */
}

/* Header h's array dead (arry_kill); the arena's newest, its room given back. */
static void arry_drop(const Arry *const ay, const Addr h)
{
    arry_kill(ay, h);

    if (h && arry_arena(h))
        stack_arena_give(ay->vm, h, arry_block(ay, h));
}

void arry_free(Arry *const ay, Vm *const vm, ArrSlot *const s)
{
    const Addr h = s->hdr;
    s->hdr = 0;
    arry_drop(ay, h);
    arry_sweep(ay, vm);
}

/* ---- views, ours ---------------------------------------------------------------------------- */

ArrView arry_view(const Addr h)
{
    const ArrayHdr *const a = arry_hdr_at(h);
    return (ArrView){.data = a->data, .et = a->et, .dims = a->dims, .hdr = a};
}

ArrView arry_view_field(const Arry *const ay, const Addr data, const uint32_t key)
{
    const Field *const f = decl_keyfield(ay->decl, key);
    return (ArrView){.data = data, .et = decl_fieldtype(f), .dims = f->ndim,
                     .fdims = decl_fielddim(ay->decl, f, 0)};
}

uint32_t arry_view_count(const ArrView *const v, const uint32_t i)
{
    return v->hdr ? arry_count(v->hdr, i) : v->fdims[i].count;
}

int32_t arry_view_lbound(const ArrView *const v, const uint32_t i)
{
    return v->hdr ? arry_lbound(v->hdr, i) : v->fdims[i].lb;
}

uint32_t arry_view_elems(const ArrView *const v)
{
    if (v->hdr)
        return arry_elem_count(v->hdr);

    uint32_t n = 1;

    for (uint32_t i = 0; i < v->dims; i++)
        n *= v->fdims[i].count;

    return n;
}

uint32_t arry_view_size(const Arry *const ay, const ArrView *const v)
{
    return arry_view_elems(v) * arry_et_size(ay->decl, v->et);
}

ArrSlot *arry_field_cell(Arry *const ay, const Addr data, const uint32_t key)
{
    const Field *const f = decl_keyfield(ay->decl, key);
    const FieldDim *const fd = decl_fielddim(ay->decl, f, 0);
    const uint32_t size = arry_fcell_size(f->ndim);

    arry_fcells_drop(ay);

    if (ay->fcells - ay->hdr < size)
        fault_error(ay->fault, E_OM);

    ay->fcells -= size;
    FieldCell *const c = (FieldCell *)at(ay->fcells);
    const Addr h = ay->fcells + sizeof(FieldCell);

    for (uint32_t i = 0; i < f->ndim; i++) {
        ay->counts[i] = fd[i].count;
        ay->lbounds[i] = fd[i].lb;
    }

    arry_fill(ay, h, data, decl_fieldtype(f), f->ndim, addr_of(&c->slot));
    *c = (FieldCell){addr_of(ay->vm->sp), {h, false}};

    return &c->slot;
}

void arry_clear(Arry *const ay, Vm *const vm, const ArrView *const v)
{
    const uint32_t n = arry_view_elems(v);

    arry_kill_recs(ay, v->data, n, v->et);
    copy_zero(v->data, arry_view_size(ay, v));

    if (v->et >= ET_REC)
        arry_empty_cells(v->data, n, decl_type(ay->decl, v->et - ET_REC));

    arry_sweep(ay, vm);
}

/* ---- a() = b(), ours ------------------------------------------------------------------------ */

/* The TYPE of header h's elements holding STRINGs or arrays, ours; null: none. */
static const TypeDef *arry_deeptype(const Arry *const ay, const Addr h)
{
    const ArrayHdr *const a = arry_hdr_at(h);

    if (a->et < ET_REC)
        return nullptr;

    const TypeDef *const td = decl_type(ay->decl, a->et - ET_REC);
    return td->nstr || td->nslot ? td : nullptr;
}

/* Cell d, assigned a whole array: Duplicate definition when a DIM's with elements, whose bounds
   stay; its old header, which the caller kills once d holds the new one. */
static Addr arry_assigned(const Arry *const ay, const ArrSlot *const d)
{
    if (d->hdr && !d->dyn)
        fault_error(ay->fault, E_RD);

    return d->hdr;
}

void arry_copy_rec(Arry *const ay, const Addr dst, const Addr src, const TypeDef *const td)
{
    strg_copy_owned(ay->strg, dst, src, td->strs, td->nstr);

    for (uint32_t i = 0; i < td->nslot; i++) { /* each array its own copy */
        ArrSlot *const d = (ArrSlot *)at(dst + td->slots[i]);
        *d = (ArrSlot){0, 1};

        arry_copy_into(ay, d, (const ArrSlot *)at(src + td->slots[i]));
    }
}

/* arry_copy_into's copy; arena: on the running call's arena if it can be. */
static void arry_copy_on(Arry *const ay, ArrSlot *const d, const ArrSlot *const s, const bool arena)
{
    if (!s->hdr)
        return;

    const ArrayHdr *const a = arry_hdr_at(s->hdr);
    const uint8_t n = a->dims;

    for (uint32_t i = 0; i < n; i++) {
        ay->counts[i] = arry_count(a, i);
        ay->lbounds[i] = arry_lbound(a, i);
    }

    arry_make_on(ay, d, a->et, n, true, arena); /* at the end: nothing moves */

    const ArrayHdr *const b = arry_hdr_at(s->hdr);
    arry_hdr_at(d->hdr)->fast = b->fast; /* a list's length */
    const Addr from = b->data, to = arry_hdr_at(d->hdr)->data;
    const uint32_t size = arry_size(ay, s->hdr);
    const TypeDef *const td = arry_deeptype(ay, s->hdr);

    if (b->et == ET_SD) {
        for (uint32_t k = 0; k < size; k += DSZ) /* each string its own copy */
            strg_set(ay->strg, from + k, to + k);

        return;
    }

    copy_mem(to, from, size);

    for (uint32_t k = 0; td && k < size; k += td->size) /* a record's STRINGs and arrays its own */
        arry_copy_rec(ay, to + k, from + k, td);
}

void arry_copy_into(Arry *const ay, ArrSlot *const d, const ArrSlot *const s)
{
    arry_copy_on(ay, d, s, true);
}

void arry_copy(Arry *const ay, Vm *const vm, ArrSlot *const d, const ArrSlot *const s)
{
    if (d == s)
        return;

    const Addr old = arry_assigned(ay, d);
    *d = (ArrSlot){0, 1};

    arry_copy_into(ay, d, s);
    arry_kill(ay, old);
    arry_sweep(ay, vm);
}

/* Cell d's array, the heap's, its: its owner d, d's call to free it (arry_own_call). */
static void arry_own(const Arry *const ay, const ArrSlot *const d)
{
    if (d->hdr && arry_heap(ay, d->hdr)) {
        arry_hdr_at(d->hdr)->owner = arry_owner(ay, d);
        arry_own_call(ay, addr_of(d));
    }
}

/* Cell c is the running call's, in its frame or arena: its frame's and arena's arrays live as long. */
static bool arry_in_call(const Vm *const vm, const Addr c)
{
    return c >= addr_of(vm->sp) && c < vm->fp;
}

/* Cell s's array, a frame's or an arena's, copied to the heap, the old one dropped: s's array goes
   to a cell its call may outlive. Its records' arrays are the heap's: their cells are. */
static void arry_off_call(Arry *const ay, ArrSlot *const s)
{
    const ArrSlot old = *s;

    if (!old.hdr || arry_heap(ay, old.hdr))
        return;

    *s = (ArrSlot){0, 1};
    arry_copy_on(ay, s, &old, false);
    arry_drop(ay, old.hdr);
}

void arry_move(Arry *const ay, Vm *const vm, ArrSlot *const d, ArrSlot *const s)
{
    if (d == s)
        return;
    if (s->hdr && arry_fcell(ay, s->hdr)) { /* a fixed field's: its record keeps it */
        arry_copy(ay, vm, d, s);
        return;
    }

    const Addr old = arry_assigned(ay, d);

    if (!arry_in_call(vm, addr_of(d)))
        arry_off_call(ay, s);

    *d = (ArrSlot){s->hdr, 1};
    *s = (ArrSlot){0, 1};

    arry_own(ay, d);
    arry_drop(ay, old);
    arry_sweep(ay, vm);
}

void arry_reset(Arry *const ay, Vm *const vm, ArrSlot *const s)
{
    const Addr old = s->hdr;
    *s = (ArrSlot){0, 1};

    arry_drop(ay, old);
    arry_sweep(ay, vm);
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

/* Record r of TYPE td: its heap arrays' owners its cells, their call's to free (arry_own_call). */
static void arry_own_slots(const Arry *const ay, const Addr r, const TypeDef *const td)
{
    for (uint32_t i = 0; i < td->nslot; i++)
        arry_own(ay, (const ArrSlot *)at(r + td->slots[i]));
}

void arry_move_rec(const Arry *const ay, const Addr dst, const Addr src, const TypeDef *const td)
{
    strg_move_owned(ay->strg, dst, src, td->strs, td->nstr);
    arry_own_slots(ay, dst, td);

    for (uint32_t i = 0; i < td->nslot; i++) /* not the old one's to kill */
        *(ArrSlot *)at(src + td->slots[i]) = (ArrSlot){0, 1};
}

void arry_swap_rec(Arry *const ay, const Addr a, const Addr b, const TypeDef *const td)
{
    for (uint32_t i = 0; i < td->nslot; i++) {
        arry_off_call(ay, (ArrSlot *)at(a + td->slots[i])); /* a SWAP may hand it on */
        arry_off_call(ay, (ArrSlot *)at(b + td->slots[i]));
    }

    copy_swap(a, b, td->size);
    strg_move_owned(ay->strg, a, a, td->strs, td->nstr); /* in place: the owners follow */
    strg_move_owned(ay->strg, b, b, td->strs, td->nstr);
    arry_own_slots(ay, a, td);
    arry_own_slots(ay, b, td);
    arry_sweep(ay, ay->vm); /* last: a and b may be in the heap */
}

/* Header h's element at src moved to dst: a string's block's owner follows, a record's STRINGs'
   and arrays' too (ours); td the record's TYPE, if it holds any. */
static void arry_move_elem(
    const Arry *const ay,
    const Addr h,
    const TypeDef *const td,
    const Addr dst,
    const Addr src
)
{
    const uint32_t et = arry_hdr_at(h)->et;

    if (et == ET_SD)
        strg_move(ay->strg, src, dst);
    else
        copy_mem(dst, src, arry_et_size(ay->decl, et));
    if (td)
        arry_move_rec(ay, dst, src, td);
}

void arry_keep(Arry *const ay, const Addr old_hdr, const Addr fresh_hdr)
{
    const ArrayHdr *const old = arry_hdr_at(old_hdr);
    const ArrayHdr *const fresh = arry_hdr_at(fresh_hdr);
    const uint32_t esize = arry_et_size(ay->decl, old->et);
    const TypeDef *const td = arry_deeptype(ay, old_hdr);
    const uint32_t total = arry_elem_count(old);

    for (uint32_t k = 0; k < total; k++) {
        uint32_t idx;

        if (arry_index_in(old, fresh, k, &idx))
            arry_move_elem(ay, old_hdr, td, fresh->data + idx * esize, old->data + k * esize);
    }
}

/* REDIM PRESERVE's new array in cell s, made right under old, the arena's newest: slid up to old's
   end, the room under it given back; elsewhere, left. */
static void arry_slide(Arry *const ay, ArrSlot *const s, const Addr old)
{
    const Addr h = s->hdr;

    if (!arry_arena(h) || !arry_arena(old) || h + arry_block(ay, h) != old)
        return;

    ArrayHdr *const a = arry_hdr_at(h);
    const uint32_t delta = arry_block(ay, old);
    const uint32_t esize = arry_et_size(ay->decl, a->et);
    const uint32_t total = arry_elem_count(a);
    const TypeDef *const td = arry_deeptype(ay, h);

    if (a->et == ET_SD || td)
        for (uint32_t k = total; k-- > 0;) /* the highest first: the rooms overlap */
            arry_move_elem(ay, h, td, a->data + delta + k * esize, a->data + k * esize);
    else
        copy_mem(a->data + delta, a->data, total * esize);

    a->data += delta;
    copy_mem(h + delta, h, arry_frame_hdr(a->dims)); /* last: it lands on the data moved */
    s->hdr = h + delta;
    stack_arena_give(ay->vm, h, delta);
}

/* arry_redim, the new one's fast len when 1-D, else as made. */
static void arry_resize(
    Arry *const ay,
    ArrSlot *const s,
    const uint32_t et,
    const uint8_t n,
    const bool keep,
    const uint32_t len
)
{
    const Addr old = s->hdr;

    if (old && arry_fixed(ay, old))
        fault_error(ay->fault, E_RD);
    if (old && keep && n != arry_hdr_at(old)->dims)
        arry_bad_sub(ay);

    arry_counts(ay, n);

    if (!old || !keep) { /* the old one's room first: the newest's is the new one's */
        s->hdr = 0;
        arry_drop(ay, old);
        arry_make(ay, s, et, n, true);

        if (len != UINT32_MAX)
            arry_hdr_at(s->hdr)->fast = len;

        arry_sweep(ay, ay->vm); /* last: s may be in the heap */
        return;
    }

    arry_make(ay, s, et, n, true);

    if (len != UINT32_MAX)
        arry_hdr_at(s->hdr)->fast = len;

    arry_keep(ay, old, s->hdr);
    arry_kill(ay, old);
    arry_slide(ay, s, old);
    arry_sweep(ay, ay->vm);
}

void arry_redim(
    Arry *const ay,
    ArrSlot *const s,
    const uint32_t et,
    const uint8_t n,
    const bool keep
)
{
    arry_resize(ay, s, et, n, keep, UINT32_MAX);
}

void arry_relen(
    Arry *const ay,
    ArrSlot *const s,
    const uint32_t et,
    const uint32_t cap,
    const uint32_t len
)
{
    ay->subs[0] = (int32_t)cap - 1;
    ay->lbounds[0] = 0;
    arry_resize(ay, s, et, 1, s->hdr != 0, len);
}
