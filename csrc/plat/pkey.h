/* The keyboard: keys waiting, read with their scan codes, the shift flags; ON KEY's trapping.
   On DOS32 the keys are INT 16h's, the shift flags 0040:0017h's. */
#ifndef PLAT_PKEY_H
#define PLAT_PKEY_H
#include "base.h"

/* A key read: its character as the host gives it (0, or E0h with a scan code: an extended key),
   its scan code. */
typedef struct {
    uint8_t ch, scan;
} PkeyKey;

bool pkey_ready(void);   /* a key waiting */
PkeyKey pkey_read(void); /* the next key, waited for */
uint8_t pkey_shift(void); /* the shift flags: bit 0 right shift, 1 left, 2 Ctrl, 3 Alt */
void pkey_flush(void);   /* the keys waiting dropped */

/* ON KEY's trapping. A trap: the scan code of the key, 0 none; the shift flags (low 4 bits) it
   needs, PKEY_ANY any. While a trap is set, a key matching it is taken before the program can
   read it (DOS32: by the tick, at the BIOS buffer's head), its bit set in the taken mask and
   flag's bit ORed (pkey_init). */
enum : uint8_t { PKEY_ANY = 0xFF, PKEY_NTRAP = 32 };
typedef struct {
    uint8_t scan, shift;
} PkeyTrap;
void pkey_init(volatile uint8_t *flag, uint8_t bit);
/* The traps, by slot 1 .. 31 (slot 0 unused), copied. */
void pkey_trap(const PkeyTrap traps[PKEY_NTRAP]);
/* The slots whose key was taken since the last call, bit n slot n; cleared. */
uint32_t pkey_taken(void);
#endif
