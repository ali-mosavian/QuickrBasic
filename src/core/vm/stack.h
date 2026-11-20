/* The data stack's segments and the frames' room. How it works: stack.c. */
#ifndef CORE_VM_STACK_H
#define CORE_VM_STACK_H
#include "session.h"

/* The frames' first room and the data stack's first segment, from the heap's top (empty: at boot).
   Out of memory. */
void stack_init(Session *S);
/* Room for n frames: the frames grown. false: no memory. */
bool stack_frames(Session *S, uint32_t n);
/* A segment for a call whose frame's n cells and npar arguments are past vm.stklim: the arguments
   copied there from vm.sp, vm.sp and vm.stklim its. false: no memory. */
bool stack_call(Session *S, uint16_t npar, uint32_t n);
/* No calls: the segments past the first and the frames' room past its first back to the heap. */
void stack_reset(Session *S);
/* vm.sp back in segment cur: vm.stklim its; the segments past it back to the heap, but spare. */
void stack_back(Session *S, uint32_t cur, uint32_t spare);
/* size bytes on the running call's arena for cell s, its local's: their Addr, vm.sp and its frame's
   stack below them. 0: s not its local's, its frame not the top one, a statement's values on the
   stack, or no room above vm.stklim. */
Addr stack_arena_take(Vm *vm, Addr s, uint32_t size);
/* The size bytes at a given back when the running call's arena's newest, at a statement's start. */
void stack_arena_give(Vm *vm, Addr a, uint32_t size);
#endif
