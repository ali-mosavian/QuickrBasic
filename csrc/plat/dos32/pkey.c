/* The keyboard on DOS32: INT 16h, the BIOS shift flags, DOS's 0Bh and 07h; the traps are taken
   by the tick (ptick.asm). */
#include "dos.h"
#include "plat/pkey.h"

extern volatile uint8_t *ptick_kflag;
extern uint8_t ptick_kbit;
extern volatile uint8_t ptick_traps[2 * PKEY_NTRAP];

static uint32_t int16(const uint8_t fn, PdosRegs *const r)
{
    *r = (PdosRegs){.eax = pdos_fn(fn, 0)};
    return pdos_int(INT_KEYBOARD, r);
}

bool pkey_ready(void)
{
    PdosRegs r;
    return !(int16(KBD_STATUS_EXT, &r) & PDOS_ZF);
}

PkeyKey pkey_read(void)
{
    PdosRegs r;
    int16(KBD_READ_EXT, &r);
    return (PkeyKey){.ch = r.eax, .scan = r.eax >> 8};
}

uint16_t pkey_wait(void)
{
    PdosRegs r;
    int16(KBD_READ, &r);
    return r.eax;
}

uint8_t pkey_shift(void) { return *pdos_lin(BDA_SHIFT); }

void pkey_flush(void)
{
    PdosRegs r;
    while (!(int16(KBD_STATUS, &r) & PDOS_ZF))
        int16(KBD_READ, &r);
}

uint8_t pkey_poll(void)
{
    PdosRegs r = {.eax = pdos_fn(DOS_INPUT_STATUS, 0)};
    pdos_int(INT_DOS, &r);
    if (!(r.eax & 0xFF))
        return 0;
    r.eax = pdos_fn(DOS_READ_KEY, 0);
    pdos_int(INT_DOS, &r);
    return r.eax;
}

void pkey_init(volatile uint8_t *const flag, const uint8_t bit)
{
    ptick_kbit = bit;
    ptick_kflag = flag;
}

void pkey_trap(const PkeyTrap traps[PKEY_NTRAP])
{
    for (uint32_t i = 0; i < PKEY_NTRAP; i++) {
        ptick_traps[2 * i] = 0; /* the scan last: the tick never sees half a trap */
        ptick_traps[2 * i + 1] = traps[i].shift;
        ptick_traps[2 * i] = traps[i].scan;
    }
}
