/* setjmp and longjmp for targets without a C library (dos32: plat/dos32/jump.asm). */
#ifndef SETJMP_H
#define SETJMP_H
#include <cdecl.h>

typedef unsigned jmp_buf[6]; /* ebx esi edi ebp esp eip */

CDECL int setjmp(jmp_buf env);
[[noreturn]]
CDECL void longjmp(jmp_buf env, int value);
#endif
