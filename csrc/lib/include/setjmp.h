/* setjmp and longjmp for targets without a C library (dos32: plat/dos32/jump.asm). */
#ifndef SETJMP_H
#define SETJMP_H

typedef unsigned jmp_buf[6]; /* ebx esi edi ebp esp eip */

int setjmp(jmp_buf env);
[[noreturn]] void longjmp(jmp_buf env, int value);
#endif
