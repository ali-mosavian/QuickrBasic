/* The clock: the date and time of day, and the 18.2 Hz tick that ON TIMER counts.
 *
 * # How it works
 * pclk_init gives the tick a flag and a bit: each tick ORs the bit into the flag (exec's stop
 * flags) and the core polls it. pclk_ticks counts ticks since midnight and wraps there. pclk_done
 * takes the tick's handler out before the exit; pkey's trapping shares that handler.
 *
 * The jiffy clock (TI, TI$) is the ticks plus an offset pclk_set_jiffies sets, mod 5184000 (a
 * day of 60 Hz jiffies, as the asm's plt_rdtim).
 *
 * On DOS32 the time and date are INT 21h's, the ticks the BIOS count at 0040:006Ch, the tick
 * INT 1Ch.
 */
#ifndef PLAT_PCLK_H
#define PLAT_PCLK_H
#include "base.h"

typedef struct {
    uint8_t hour, min, sec, hund;
} PclkTime;
typedef struct {
    uint16_t year; /* 1980 .. 2099 */
    uint8_t month, day;
} PclkDate;

/* Each tick from now on ORs bit into *flag (exec's stop flags): the core polls it. The tick's
   handler (also pkey's trapping) is in; pclk_done takes it out, before the exit. */
void pclk_init(volatile uint8_t *flag, uint8_t bit);
void pclk_done(void);
/* The ticks since midnight, 18.2 a second; it wraps at midnight. */
uint32_t pclk_ticks(void);
void pclk_time(PclkTime *t);
void pclk_date(PclkDate *d);
/* Set the clock; false: the host refused the value. */
bool pclk_set_time(const PclkTime *t);
bool pclk_set_date(const PclkDate *d);
/* The jiffy clock (TI, TI$): the ticks plus an offset pclk_set_jiffies sets, mod 5184000
   (a day of 60 Hz jiffies, as the asm's plt_rdtim). */
uint32_t pclk_jiffies(void);
void pclk_set_jiffies(uint32_t j);
/* Microseconds, only ever forward: gfx's clock. */
uint64_t pclk_us(void);
#endif
