/* O_SYSCALL n: the runtime's service n (syscalls.def), through its handler.
 *
 * # How it works
 * The compiler pushes a service's arguments and emits O_SYSCALL with its number; the number picks
 * the handler from the table, which pops them and calls into the runtime. A service is added by
 * its line in syscalls.def and its handler: this dispatch stays as it is.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | sysc_call | NEW |
 */
#include "vm/syscall.h"
#include "session.h"

#define SC(name, h) void h(Session *S);
#include "vm/syscalls.def"

typedef void Handler(Session *S);

static Handler *const calls[SC_N] = {
#define SC(name, h) [SC_##name] = h,
#include "vm/syscalls.def"
};

void sysc_call(Session *const S) { calls[exec_u8(&S->vm)](S); }
