/* The mouse on DOS32.
 *
 * # How it works
 * INT 33h; a null INT 33h vector is no driver.
 */
#include "dos.h"
#include "plat/pmou.h"

enum { RESET_OK = 0xFFFF, CELL_SHIFT = 3 }; /* a text cell is 8 mickeys square */

static PdosRegs int33(const uint16_t ax)
{
    PdosRegs r = {.eax = ax};

    pdos_int(INT_MOUSE, &r);
    return r;
}

bool pmou_init(void)
{
    volatile const uint8_t *const v = pdos_lin(INT_MOUSE * 4); /* the real-mode vector, linear */
    if (!(v[0] | v[1] | v[2] | v[3]))
        return false;

    return (int33(MOUSE_RESET).eax & 0xFFFF) == RESET_OK;
}

void pmou_show(void) { int33(MOUSE_SHOW); }
void pmou_hide(void) { int33(MOUSE_HIDE); }

PmouState pmou_state(void)
{
    const PdosRegs r = int33(MOUSE_STATE);
    return (PmouState){.row = (uint8_t)((r.edx & 0xFFFF) >> CELL_SHIFT),
                       .col = (uint8_t)((r.ecx & 0xFFFF) >> CELL_SHIFT),
                       .buttons = (uint8_t)r.ebx};
}
