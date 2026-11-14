/* Copies in the arena and in the program store (src/mem/far.asm). */
#ifndef MEM_COPY_H
#define MEM_COPY_H
#include "defs.h"

/* len bytes from src to dst; they may overlap (mem_cpy, mem_up). */
void copy_mem(Addr dst, Addr src, uint32_t len);
/* len bytes zeroed at a; returns a + len (mem_zero). */
Addr copy_zero(Addr a, uint32_t len);
/* len bytes inside the program store, dst and src offsets in it; they may overlap (mem_pmov). */
void copy_prog(Session *S, uint32_t dst, uint32_t src, uint32_t len);
#endif
