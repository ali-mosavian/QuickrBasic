/* Copies' interface. How it works: copy.c. */
#ifndef MEM_COPY_H
#define MEM_COPY_H
#include "defs.h"

/* len bytes from src to dst; they may overlap (mem_cpy, mem_up). */
void copy_mem(Addr dst, Addr src, uint32_t len);
/* The len bytes at a and at b swapped; they do not overlap. */
void copy_swap(Addr a, Addr b, uint32_t len);
/* len bytes zeroed at a; returns a + len (mem_zero). */
Addr copy_zero(Addr a, uint32_t len);
#endif
