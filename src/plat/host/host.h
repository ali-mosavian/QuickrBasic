/* The host platform's own: what tests drive and inspect. Only plat/host and tests include it. */
#ifndef PLAT_HOST_H
#define PLAT_HOST_H
#include "base.h"
#include "plat/pscr.h"

/* A key for pkey_read and pkey_wait: scan code << 8 | character. */
void host_key_push(uint16_t key);
/* Page's cell at row, col. */
PscrCell host_cell(uint8_t page, uint8_t row, uint8_t col);

/* The screen as a display shows it: the page shown's cells, its cursor and shape, the 16 colours
   (6 bits a channel, as pscr_palette's). */
typedef struct {
    const PscrCell *cells;
    uint8_t page, rows, row, col;
    uint16_t shape;
    uint8_t palette[48];
} HostScreen;
HostScreen host_screen(void);

/* The terminal as the keyboard, if standard input is one (pterm.c); on, a key is waiting, the next
   key (scan code << 8 | character; 0 Ctrl+Break). */
void host_term_open(void);
/* The terminal as it was, for a child process (yes), and back. */
void host_term_pause(bool yes);
bool host_term_on(void);
bool host_term_ready(void);
uint16_t host_term_key(void);
/* The shift flags (PKEY_*): none on a terminal. */
uint8_t host_term_shift(void);
/* SIGINT came since the last call (psys.c). */
bool host_stop_taken(void);
#endif
