/* The mouse on DOS32: INT 33h; a null INT 33h vector is no driver. */
#include "dos.h"
#include "plat/pmou.h"

static uint32_t int33(uint32_t ax, PdosRegs *r)
{
    *r = (PdosRegs){.eax = ax};
    pdos_int(0x33, r);
    return r->eax & 0xFFFF;
}

bool pmou_init(void)
{
    volatile uint8_t *v = pdos_lin(0x33 * 4); /* the real-mode vector, linear */
    if (!(v[0] | v[1] | v[2] | v[3]))
        return false;
    PdosRegs r;
    return int33(0, &r) == 0xFFFF;
}

void pmou_show(void)
{
    PdosRegs r;
    int33(1, &r);
}

void pmou_hide(void)
{
    PdosRegs r;
    int33(2, &r);
}

PmouState pmou_state(void)
{
    PdosRegs r;
    int33(3, &r);
    return (PmouState){.row = (uint8_t)((r.edx & 0xFFFF) >> 3),
                       .col = (uint8_t)((r.ecx & 0xFFFF) >> 3),
                       .buttons = (uint8_t)r.ebx};
}
