/* The clock: the date and time of day, and the 18.2 Hz tick ON TIMER counts. On DOS32 the time and
   date are INT 21h's, the ticks the BIOS count at 0040:006Ch, the tick INT 1Ch. */
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

/* Each tick from now on ORs bit into *flag (exec's stop flags): the core polls it. */
void pclk_init(volatile uint8_t *flag, uint8_t bit);
/* The ticks since midnight, 18.2 a second; it wraps at midnight. */
uint32_t pclk_ticks(void);
void pclk_time(PclkTime *t);
void pclk_date(PclkDate *d);
/* Set the clock; false: the host refused the value. */
bool pclk_set_time(const PclkTime *t);
bool pclk_set_date(const PclkDate *d);
#endif
