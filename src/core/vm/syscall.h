/* The runtime's services by number (O_SYSCALL n): the numbers. How it works: syscall.c. */
#ifndef CORE_VM_SYSCALL_H
#define CORE_VM_SYSCALL_H
#include "vm/exec.h"

typedef enum : uint8_t {
    SC_NONE, /* not a service: a function's own CALL (comp/builtins.h) */
#define SC(name, h) SC_##name,
#include "vm/syscalls.def"
    SC_N
} Syscall;
#endif
