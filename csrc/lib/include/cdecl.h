/* Arguments on the stack, whatever the build's default (gcc -mregparm=3): the convention of the
   routines in asm and in llrm's runtime, and of C called from asm. */
#ifndef CDECL_H
#define CDECL_H
#define CDECL __attribute__((regparm(0)))
#endif
