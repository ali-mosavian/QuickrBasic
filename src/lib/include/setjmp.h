/* setjmp and longjmp for targets without a C library.
 *
 * # How it works
 * dos32 implements them in plat/dos32/jump.asm. A jmp_buf holds ebx, esi, edi, ebp, esp and eip.
 */
#ifndef SETJMP_H
#define SETJMP_H
#include <cdecl.h>

typedef unsigned jmp_buf[6]; /* ebx esi edi ebp esp eip */

CDECL int setjmp(jmp_buf env);
[[noreturn]]
CDECL void longjmp(jmp_buf env, int value);
#endif
