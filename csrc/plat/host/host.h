/* The host platform's own: what tests drive and inspect. Only plat/host and tests include it. */
#ifndef PLAT_HOST_H
#define PLAT_HOST_H
#include "base.h"
#include "plat/pscr.h"

/* A key for pkey_read and pkey_wait: scan code << 8 | character. */
void host_key_push(uint16_t key);
/* Page's cell at row, col. */
PscrCell host_cell(uint8_t page, uint8_t row, uint8_t col);
#endif
