/* DOS's 3D accelerators: the driver for the card found, loaded once from lib/drivers/ (load.c),
 * through which gfx's lists are played (player.c).
 */
#ifndef PLAT_DOS32_ACCEL_ACCEL_H
#define PLAT_DOS32_ACCEL_ACCEL_H
#include "plat/dos32/accel/driver.h"

/* The driver for an accelerator on the bus, loaded the first time it is asked for, its card to
   *card: null, none, or no driver for any. */
const AccelDriver *accel_driver(PciFunction *card);
/* A driver file's first n bytes say it drives card f (file.c). */
bool accel_drives(const uint8_t *head, uint32_t n, const PciFunction *f);
/* A driver file's n bytes: its image's first byte and their count; false, not a driver file. */
bool accel_image(const uint8_t *file, uint32_t n, const uint8_t **image, uint32_t *bytes);
#endif
