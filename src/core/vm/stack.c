/* The data stack's segments and the frames' room, each a block of the shared heap.
 *
 * # How it works
 * A call whose frame does not fit above vm.stklim gets the next segment, its arguments copied
 * there; its return goes back to the caller's, the one its frame names. A segment never moves:
 * stack cells' addresses are kept in refs, in strings' owners and in values on the stack. The
 * frames' block moves as it grows. Each segment has twice its caller's room, or what is left.
 *
 * Segments lie anywhere, so depth is the segment's index, then the address within it: a Desc is
 * popped when it lies in segment cur under sp or in a segment past cur (stack_popped). The strings
 * are collected before a segment is given back, so no string's owner is left in freed memory. The
 * segment past sp's stays for the next call, so calls at a segment's edge do not give and take.
 *
 * A call's arena is the stack under its locals: its dynamic arrays (mem/array.c), taken at a
 * statement's start, when nothing is on the stack, while its frame is the top one (no GOSUB's
 * above). Its frame's stack is the arena's top, the stack at a statement's start. Its return drops
 * the arena with the frame; only the newest array is given back sooner.
 */
#include "vm/stack.h"
#include "mem/string.h"
#include "plat/pmem.h"
#include "util/grow.h"
#include <string.h>

enum { FRAMES_FIRST = 64, ALIGN = 16, STACK_TOP = 16 /* above a segment's first cell */ };

static uint32_t align_up(const uint32_t n) { return (n + ALIGN - 1) & ~(uint32_t)(ALIGN - 1); }

static uint32_t frames_first(void) { return align_up(FRAMES_FIRST * sizeof(Frame)); }

/* The frames' room as size bytes, moved or not: false, no memory. */
static bool frames_resize(Vm *const vm, const uint32_t size)
{
    const uint32_t n = vm->frames ? (uint32_t)(vm->ftop - vm->frames) : 0;
    Frame *const floor = pmem_resize(vm->frames ? vm->frames - 1 : nullptr, size);

    if (!floor)
        return false;

    vm->frames = floor + 1;
    vm->ftop = vm->frames + n;
    vm->fend = floor + size / sizeof(Frame);
    vm->fbytes = size;
    return true;
}

/* A segment of size bytes past the last. */
static bool add_seg(Session *const S, const uint32_t size)
{
    Stak *const k = &S->vm.stak;

    if (!GROW(k->seg, k->cap, k->n + 1))
        return false;

    void *const p = pmem_resize(nullptr, size);

    if (!p)
        return false;

    k->seg[k->n++] = (StakSeg){addr_of(p), addr_of(p) + size};
    return true;
}

/* Segments from i on back to the heap, the strings they owned collected first. */
static void give_from(Session *const S, const uint32_t i)
{
    Stak *const k = &S->vm.stak;

    if (k->n <= i)
        return;

    strg_gc(&S->strg);

    while (k->n > i)
        pmem_free(at(k->seg[--k->n].base));
}

uint32_t stack_seg_of(const Vm *const vm, const Addr a)
{
    const Stak *const k = &vm->stak;

    for (uint32_t i = 0; i < k->n; i++)
        if (a >= k->seg[i].base && a < k->seg[i].end)
            return i;

    return k->n;
}

bool stack_popped(const void *const v, const Addr d)
{
    const Vm *const vm = v;
    const uint32_t i = stack_seg_of(vm, d);

    return i < vm->stak.n && (i > vm->stak.cur || (i == vm->stak.cur && d < addr_of(vm->sp)));
}

void stack_init(Session *const S)
{
    Vm *const vm = &S->vm;

    S->strg.popped = stack_popped;
    S->strg.vm = vm;

    if (!frames_resize(vm, frames_first()) || !add_seg(S, STKSZ))
        fault_error(&S->fault, E_OM);

    vm->frames[-1].kind = FRAME_NONE;
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

    return frames_resize(vm, align_up((uint32_t)(want > half_again ? want : half_again)));
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

    /* Given back and taken anew: shrunk in place, a block at its room's top keeps its tail apart. */
    pmem_free(vm->frames - 1);
    vm->frames = vm->ftop = nullptr;

    if (!frames_resize(vm, frames_first()))
        fault_error(&S->fault, E_OM);

    vm->frames[-1].kind = FRAME_NONE;
}

void stack_back(Session *const S, const uint32_t cur, const uint32_t spare)
{
    Vm *const vm = &S->vm;
    Stak *const k = &vm->stak;

    k->cur = cur;
    vm->stklim = (Cell *)at(k->seg[cur].base + STKEVAL);
    give_from(S, cur + 1 + spare);
}

/* The running call's, its frame the top one, at a statement's start: null none. */
static Call *arena_call(const Vm *const vm)
{
    Call *const c = &vm->ftop[-1].call; /* the floor's FRAME_NONE: none */
    return frame_is_call(&vm->ftop[-1]) && c->fp == vm->fp && c->stack == vm->sp ? c : nullptr;
}

Addr stack_arena_take(Vm *const vm, const Addr s, const uint32_t size)
{
    Call *const c = arena_call(vm);

    if (!c || s < addr_of(vm->sp) || s >= vm->fp || vm->sp < vm->stklim ||
        (uint32_t)((uint8_t *)vm->sp - (uint8_t *)vm->stklim) < size)
        return 0;

    vm->sp = c->stack = (Cell *)((uint8_t *)vm->sp - size);
    return addr_of(vm->sp);
}

void stack_arena_give(Vm *const vm, const Addr a, const uint32_t size)
{
    Call *const c = arena_call(vm);

    if (c && addr_of(vm->sp) == a)
        vm->sp = c->stack = (Cell *)at(a + size);
}
