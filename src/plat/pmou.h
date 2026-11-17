/* The mouse, when a driver is there.
 *
 * # How it works
 * pmou_init resets the driver and reports whether one is there; its cursor is hidden after it.
 * pmou_show and pmou_hide are counted, as INT 33h counts them.
 *
 * x and y are the pixel under it: a graphics mode's own, in a text mode the cell's times 8 (INT 33h's).
 *
 * On DOS32 it is INT 33h; a null INT 33h vector is no driver.
 */
#ifndef PLAT_PMOU_H
#define PLAT_PMOU_H
#include "base.h"

enum : uint8_t { PMOU_LEFT = 0, PMOU_RIGHT = 1, PMOU_NBUTTONS = 2 };

typedef struct {
    uint8_t row, col; /* the text cell under it */
    uint8_t buttons;  /* bit 0 the left, 1 the right, 2 the middle */
    uint16_t x, y;
} PmouState;

/* A button's presses since the last ask, and where the last of them was (AX=5). */
typedef struct {
    uint16_t count;
    uint8_t row, col;
    uint16_t x, y;
} PmouPress;

/* The driver reset (INT 33h AX=0); false: no mouse. Its cursor is hidden after it. */
bool pmou_init(void);
/* A driver is there; nothing reset. */
bool pmou_there(void);
/* Its cursor shown, hidden (AX=1, AX=2); the driver counts them, as INT 33h does. */
void pmou_show(void);
void pmou_hide(void);
/* Its place and buttons now (AX=3). */
PmouState pmou_state(void);
/* PMOU_LEFT's or PMOU_RIGHT's presses, counted from 0 again. */
PmouPress pmou_press(uint8_t button);
#endif
