/* The data stack's segments and the frames' room, from the heap's top.
 *
 * # How it works
 * The frames and the data stack's segments are taken from the heap's top, the strings slid down
 * under them (strg_take_top). A call whose frame does not fit above vm.stklim gets the next
 * segment, its arguments copied there; its return goes back to the caller's, the one its frame
 * names. A segment never moves: stack cells' addresses are kept in refs, in strings' owners and in
 * values on the stack. The frames sit under the segments and move as segments come and go and as
 * the frames grow. Each segment has twice its caller's room, or what is left, so deep recursion
 * slides the strings a few times, not once a segment.
 *
 *   strings | frames | segment n-1 | .. | segment 1 | segment 0 | (arena's end)
 *
 * The segments are contiguous, each call's below its caller's, so a Desc in the data stack is
 * popped exactly when it lies under sp: from the strings' start up to sp nothing is live
 * (strg_is_popped_frame). The segment past sp's stays for the next call, so calls at a segment's
 * edge do not slide the strings each time.
 */
#include "vm/stack.h"
#include "mem/string.h"
#include "util/grow.h"
#include <string.h>

enum { FRAMES_FIRST = 64, ALIGN = 16, STACK_TOP = 16 /* above a segment's first cell */ };

static uint32_t align_up(const uint32_t n) { return (n + ALIGN - 1) & ~(uint32_t)(ALIGN - 1); }

static uint32_t frames_first(void) { return align_up(FRAMES_FIRST * sizeof(Frame)); }

static Frame *frames_floor(const Vm *const vm) { return vm->frames - 1; }

/* The frames moved by delta bytes. */
static void frames_move(Vm *const vm, const int32_t delta)
{
    Frame *const floor = frames_floor(vm);
    const uint32_t n = (uint32_t)(vm->ftop - vm->frames);
    Frame *const to = (Frame *)at(addr_of(floor) + (uint32_t)delta);

    memmove(to, floor, (n + 1) * sizeof(Frame));
    vm->frames = to + 1;
    vm->ftop = vm->frames + n;
    vm->fend = to + vm->fbytes / sizeof(Frame);
}

/* size bytes more under the segments, the frames moved down to their foot. */
static bool take(Session *const S, const uint32_t size)
{
    if (!strg_take_top(&S->strg, size))
        return false;

    frames_move(&S->vm, -(int32_t)size);
    return true;
}

/* A segment of size bytes past the last. */
static bool add_seg(Session *const S, const uint32_t size)
{
    Stak *const k = &S->vm.stak;

    if (!GROW(k->seg, k->cap, k->n + 1) || !take(S, size))
        return false;

    const Addr base = addr_of(frames_floor(&S->vm)) + S->vm.fbytes; /* the bytes taken */
    k->seg[k->n++] = (StakSeg){base, base + size};

    return true;
}

/* Segments from i on back to the heap, the frames moved up over them. */
static void give_from(Session *const S, const uint32_t i)
{
    Stak *const k = &S->vm.stak;

    if (k->n <= i)
        return;

    const uint32_t size = k->seg[i].end - k->seg[k->n - 1].base;
    k->n = i;
    frames_move(&S->vm, (int32_t)size);
    strg_give_top(&S->strg, size);
}

void stack_init(Session *const S)
{
    Vm *const vm = &S->vm;

    S->allo.top &= ~(Addr)(ALIGN - 1);
    S->allo.strings = S->allo.top;
    vm->fbytes = frames_first();

    if (!strg_take_top(&S->strg, vm->fbytes))
        fault_error(&S->fault, E_OM);

    Frame *const floor = (Frame *)at(S->allo.top);
    floor->kind = FRAME_NONE;
    vm->frames = vm->ftop = floor + 1;
    vm->fend = floor + vm->fbytes / sizeof(Frame);

    if (!add_seg(S, STKSZ))
        fault_error(&S->fault, E_OM);

    vm->sp = vm->rsp = (Cell *)at(vm->stak.seg[0].end - STACK_TOP);
    stack_back(S, 0, 0);
}

bool stack_frames(Session *const S, const uint32_t n)
{
    Vm *const vm = &S->vm;

    if (n <= (uint32_t)(vm->fend - vm->frames))
        return true;

    const uint64_t want = ((uint64_t)n + 1) * sizeof(Frame);
    const uint64_t half_again = vm->fbytes + vm->fbytes / 2;

    if (want > UINT32_MAX / 2)
        return false;

    const uint32_t size = align_up((uint32_t)(want > half_again ? want : half_again));

    if (!take(S, size - vm->fbytes))
        return false;

    vm->fbytes = size;
    vm->fend = frames_floor(vm) + vm->fbytes / sizeof(Frame);
    return true;
}

bool stack_call(Session *const S, const uint16_t npar, const uint32_t n)
{
    Vm *const vm = &S->vm;
    Stak *const k = &vm->stak;
    const uint64_t need = ((uint64_t)n + 2u * npar) * sizeof(Cell) + STACK_TOP;
    const uint64_t room = need > STKROOM ? need : STKROOM;
    const uint64_t twice = 2ull * (k->seg[k->cur].end - k->seg[k->cur].base - STKEVAL);

    if (room > UINT32_MAX / 4)
        return false;

    const uint32_t least = align_up((uint32_t)room + STKEVAL);
    const uint32_t ample =
        twice > room && twice <= UINT32_MAX / 4 ? align_up((uint32_t)twice + STKEVAL) : least;
    const uint32_t next = k->cur + 1;

    if (k->n > next && k->seg[next].end - k->seg[next].base < least)
        give_from(S, next); /* the spare too small */
    if (k->n == next && !add_seg(S, ample) && (ample == least || !add_seg(S, least)))
        return false;

    /* The refs still point at the caller's cells: read only through them (mem/loc.h), they live
       untouched in the caller's segment till the call returns. */
    Cell *const to = (Cell *)at(k->seg[next].end - STACK_TOP) - 2u * npar;
    memcpy(to, vm->sp, 2u * npar * sizeof(Cell));
    vm->sp = to;
    stack_back(S, next, 1);
    return true;
}

void stack_reset(Session *const S)
{
    Vm *const vm = &S->vm;
    stack_back(S, 0, 0);

    if (vm->fbytes == frames_first())
        return;

    const uint32_t size = vm->fbytes - frames_first();
    vm->fbytes = frames_first();
    frames_move(vm, (int32_t)size);
    strg_give_top(&S->strg, size);
}

void stack_back(Session *const S, const uint32_t cur, const uint32_t spare)
{
    Vm *const vm = &S->vm;
    Stak *const k = &vm->stak;

    k->cur = cur;
    vm->stklim = (Cell *)at(k->seg[cur].base + STKEVAL);
    give_from(S, cur + 1 + spare);
}
