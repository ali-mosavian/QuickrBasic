/* The keyboard on DOS32: INT 16h, the BIOS shift flags, DOS's 0Bh and 07h; the traps are taken
   by the tick (ptick.asm). */
#include "dos.h"
#include "plat/pkey.h"

extern volatile uint8_t *ptick_kflag;
extern uint8_t ptick_kbit;
extern volatile uint8_t ptick_traps[2 * PKEY_NTRAP];

bool pkey_ready(void)
{
    PdosRegs r = {.eax = 0x1100};
    return !(pdos_int(0x16, &r) & PDOS_ZF);
}

PkeyKey pkey_read(void)
{
    PdosRegs r = {.eax = 0x1000};
    pdos_int(0x16, &r);
    return (PkeyKey){.ch = r.eax, .scan = r.eax >> 8};
}

uint16_t pkey_wait(void)
{
    PdosRegs r = {.eax = 0};
    pdos_int(0x16, &r);
    return r.eax;
}

uint8_t pkey_shift(void) { return *pdos_lin(0x417); }

void pkey_flush(void)
{
    for (;;) {
        PdosRegs r = {.eax = 0x0100};
        if (pdos_int(0x16, &r) & PDOS_ZF)
            return;
        r.eax = 0;
        pdos_int(0x16, &r);
    }
}

uint8_t pkey_poll(void)
{
    PdosRegs r = {.eax = 0x0B00};
    pdos_int(0x21, &r);
    if (!(r.eax & 0xFF))
        return 0;
    r.eax = 0x0700;
    pdos_int(0x21, &r);
    return r.eax;
}

void pkey_init(volatile uint8_t *flag, uint8_t bit)
{
    ptick_kbit = bit;
    ptick_kflag = flag;
}

void pkey_trap(const PkeyTrap traps[PKEY_NTRAP])
{
    for (int i = 0; i < PKEY_NTRAP; i++) {
        ptick_traps[2 * i] = 0; /* the scan last: the tick never sees half a trap */
        ptick_traps[2 * i + 1] = traps[i].shift;
        ptick_traps[2 * i] = traps[i].scan;
    }
}
