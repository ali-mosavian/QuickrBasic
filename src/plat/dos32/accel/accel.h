/* DOS's 3D accelerators: the driver for the card found, loaded once from lib/drivers/ (load.c),
 * through which gfx's lists are played (player.c).
 */
#ifndef PLAT_DOS32_ACCEL_ACCEL_H
#define PLAT_DOS32_ACCEL_ACCEL_H
#include "plat/dos32/accel/driver.h"

/* The driver for an accelerator on the bus, loaded the first time it is asked for, its card to
   *card: null, none, or no driver for any. */
const AccelDriver *accel_driver(PciFunction *card);
/* A driver file's n bytes, a DLL, drive card f: its accel_ids holds f's (file.c). */
bool accel_drives(const uint8_t *file, uint32_t n, const PciFunction *f);

/* gfx's lists played through driver d on card (player.c), as plat/pgfx.h's PgfxPlayer: a w by h
   screen opened with QxScreen's flags, shown at the retrace with QX_SCREEN_VSYNC, its frame shown
   not carried into the next with QX_SCREEN_DISCARD; false, the card could not. Open, the screen
   is closed first. */
bool accel_open(
    const AccelDriver *d,
    const PciFunction *card,
    uint16_t w,
    uint16_t h,
    uint8_t flags
);
void accel_close(void);
/* The list played: true, it showed a frame. A FIXED host's (qx/qxlist.h): only the texture the first
   SHOW shows is shown. */
bool accel_play(const uint32_t *list, uint32_t words);
/* w by h pixels at x, y of a texture into out, bytes R, G, B and A. */
void accel_read(uint32_t texture, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *out);
/* The open screen's refresh in Hz: 0, unknown or none open. */
float accel_hz(void);
#endif
