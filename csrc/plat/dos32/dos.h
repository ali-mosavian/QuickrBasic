/* The DOS32 layer's own: software interrupts, the BIOS data area. Only plat/dos32 includes it. */
#ifndef PLAT_DOS32_DOS_H
#define PLAT_DOS32_DOS_H
#include <stdint.h>

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, flags;
} PdosRegs;

enum { PDOS_CF = 1, PDOS_ZF = 0x40 };

/* Interrupt `vector` with r's registers, which it returns; the flags returned too. */
uint32_t pdos_int(int vector, PdosRegs *r);

/* DS's selector. */
uint16_t pdos_ds(void);

/* The flat segment's byte at linear address `a` (DS is zero-based under DOS/32A). */
static inline volatile uint8_t *pdos_lin(uint32_t a) { return (volatile uint8_t *)(uintptr_t)a; }
#endif
