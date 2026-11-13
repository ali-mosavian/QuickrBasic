/* The clock on DOS32: INT 21h's date and time, the BIOS tick count, the tick (ptick.asm). */
#include "dos.h"
#include "plat/pclk.h"

extern volatile uint8_t *ptick_flag;
extern uint8_t ptick_bit;
CDECL void ptick_hook(void);
CDECL void ptick_unhook(void);

enum : uint32_t { JDAY = 5184000 }; /* a day's 60 Hz jiffies */

static uint32_t toff; /* the jiffy clock's offset (the asm's io.toff) */

void pclk_init(volatile uint8_t *flag, uint8_t bit)
{
    ptick_bit = bit;
    ptick_flag = flag;
    ptick_hook();
}

void pclk_done(void) { ptick_unhook(); }

uint32_t pclk_ticks(void)
{
    volatile uint8_t *p = pdos_lin(0x46C);
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

void pclk_time(PclkTime *t)
{
    PdosRegs r = {.eax = 0x2C00};
    pdos_int(0x21, &r);
    *t = (PclkTime){.hour = r.ecx >> 8, .min = r.ecx, .sec = r.edx >> 8, .hund = r.edx};
}

void pclk_date(PclkDate *d)
{
    PdosRegs r = {.eax = 0x2A00};
    pdos_int(0x21, &r);
    *d = (PclkDate){.year = r.ecx, .month = r.edx >> 8, .day = r.edx};
}

bool pclk_set_time(const PclkTime *t)
{
    PdosRegs r = {.eax = 0x2D00, .ecx = t->hour << 8 | t->min, .edx = t->sec << 8 | t->hund};
    pdos_int(0x21, &r);
    return (r.eax & 0xFF) == 0;
}

bool pclk_set_date(const PclkDate *d)
{
    PdosRegs r = {.eax = 0x2B00, .ecx = d->year, .edx = d->month << 8 | d->day};
    pdos_int(0x21, &r);
    return (r.eax & 0xFF) == 0;
}

/* The ticks as jiffies: ticks * 3295 / 1000, without a 64-bit product. */
static uint32_t jiffies(void)
{
    uint32_t t = pclk_ticks();
    return t * 3 + t * 295 / 1000;
}

uint32_t pclk_jiffies(void) { return (jiffies() + toff) % JDAY; }

void pclk_set_jiffies(uint32_t j) { toff = j - jiffies() + JDAY; }
