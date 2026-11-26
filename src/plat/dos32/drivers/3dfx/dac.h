/* An SST card's RAMDAC, through its dacData register: the ICS5342, or the AT&T 20C409 and TI
 * TVP3409, which share a protocol. Its clocks are PLLs: the pixel clock, and the graphics clock
 * the chips run at.
 *
 * # How it works
 * A DAC session holds the video unit in reset and the DRAM's refresh off, so nothing else uses
 * the memory bus, and maps fbiInit2's reads to the DAC's data: a read is a write of its register
 * to dacData, then fbiInit2's low byte. The ICS is found by its PLLs' power-on words; the AT&T's
 * and TI's registers are indexed once their command register's backdoor (the read mask read four
 * times) turns indexing on.
 */
#ifndef TDFX_DAC_H
#define TDFX_DAC_H
#include "plat/dos32/drivers/3dfx/tdfx.h"

typedef enum : uint8_t { TDFX_DAC_NONE, TDFX_DAC_ICS, TDFX_DAC_ATT } TdfxDac;

/* A PLL's: 14.318 MHz times (m + 2) over (n + 2) times 2 to the p; loop, the AT&T's filter
   bits. */
typedef struct {
    uint8_t m, n, p, loop;
} TdfxClock;

/* The PLL nearest mhz, as Glide picks it: false, out of its range. */
bool tdfx_clock(float mhz, TdfxClock *out);
float tdfx_clock_mhz(const TdfxClock *c);
/* The card's DAC. */
TdfxDac tdfx_dac(Tdfx *c);
/* The pixel clock set, its output 16 bits a pixel. */
void tdfx_dac_video(Tdfx *c, TdfxDac dac, const TdfxClock *clock);
/* The graphics clock set, the chips reset about it. */
void tdfx_dac_graphics(Tdfx *c, TdfxDac dac, const TdfxClock *clock);
#endif
