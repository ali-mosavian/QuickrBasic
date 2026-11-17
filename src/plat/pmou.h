/* The mouse, when a driver is there.
 *
 * # How it works
 * pmou_init resets the driver and reports whether one is there; its cursor is hidden after it.
 * pmou_show and pmou_hide are counted, as INT 33h counts them.
 *
 * On DOS32 it is INT 33h; a null INT 33h vector is no driver.
 */
#ifndef PLAT_PMOU_H
#define PLAT_PMOU_H
#include "base.h"

typedef struct {
    uint8_t row, col; /* the text cell under it (DOS: pixels / 8) */
    uint8_t buttons;  /* bit 0 the left, 1 the right, 2 the middle */
} PmouState;

/* The driver reset (INT 33h AX=0); false: no mouse. Its cursor is hidden after it. */
bool pmou_init(void);
/* Its cursor shown, hidden (AX=1, AX=2); the driver counts them, as INT 33h does. */
void pmou_show(void);
void pmou_hide(void);
/* Its place and buttons now (AX=3). */
PmouState pmou_state(void);
#endif
