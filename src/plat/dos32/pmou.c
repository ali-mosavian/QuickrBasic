/* The mouse on DOS32.
 *
 * # How it works
 * INT 33h; a null INT 33h vector is no driver. The driver's place is a virtual screen 640 wide: a
 * 320-wide mode's pixel is half its x. A text cell is 8 of it square; a graphics mode's is 8 wide
 * and as high as the BIOS's characters.
 */
#include "dos.h"
#include "plat/pmou.h"

enum { RESET_OK = 0xFFFF, CELL_SHIFT = 3, MODE_MONO = 7, MODE_GRAPHICS = 4 };

static PdosRegs int33(const uint16_t ax, const uint16_t bx)
{
    PdosRegs r = {.eax = ax, .ebx = bx};

    pdos_int(INT_MOUSE, &r);
    return r;
}

/* The pixel and the cell of the driver's cx, dx. */
static void place(
    const uint32_t cx,
    const uint32_t dx,
    uint16_t *const x,
    uint16_t *const y,
    uint8_t *const row,
    uint8_t *const col
)
{
    const uint8_t mode = *pdos_lin(BDA_MODE);
    const bool graphics = mode >= MODE_GRAPHICS && mode != MODE_MONO;
    const bool half = mode == 0x04 || mode == 0x05 || mode == 0x0D || mode == 0x13;
    const uint8_t char_h = *pdos_lin(BDA_CHAR_H);

    *x = (uint16_t)((cx & 0xFFFF) >> half);
    *y = (uint16_t)(dx & 0xFFFF);
    *col = (uint8_t)(graphics ? *x >> CELL_SHIFT : (cx & 0xFFFF) >> CELL_SHIFT);
    *row = (uint8_t)(graphics && char_h ? *y / char_h : *y >> CELL_SHIFT);
}

bool pmou_there(void)
{
    volatile const uint8_t *const v = pdos_lin(INT_MOUSE * 4); /* the real-mode vector, linear */
    return v[0] | v[1] | v[2] | v[3];
}

bool pmou_init(void) { return pmou_there() && (int33(MOUSE_RESET, 0).eax & 0xFFFF) == RESET_OK; }

void pmou_show(void) { int33(MOUSE_SHOW, 0); }
void pmou_hide(void) { int33(MOUSE_HIDE, 0); }

PmouState pmou_state(void)
{
    const PdosRegs r = int33(MOUSE_STATE, 0);
    PmouState s = {.buttons = (uint8_t)r.ebx};

    place(r.ecx, r.edx, &s.x, &s.y, &s.row, &s.col);
    return s;
}

PmouPress pmou_press(const uint8_t button)
{
    const PdosRegs r = int33(MOUSE_PRESS, button);
    PmouPress p = {.count = (uint16_t)r.ebx};

    place(r.ecx, r.edx, &p.x, &p.y, &p.row, &p.col);
    return p;
}
