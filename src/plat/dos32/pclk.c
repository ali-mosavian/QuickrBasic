/* The clock on DOS32.
 *
 * # How it works
 * Time and date are INT 21h's, the ticks the BIOS count; the tick is INT 1Ch (ptick.asm).
 *
 * Microseconds are the ticks and the timer chip's count within one, read together: a tick between
 * the two reads is read again. The chip is in square-wave mode, counting down by two from 65536 to
 * 0 in each half of the tick; the status's output pin says which half.
 */
#include "dos.h"
#include "plat/pclk.h"
#include "plat/psys.h"

extern volatile uint8_t *ptick_flag;
extern uint8_t ptick_bit;
CDECL void ptick_hook(void);
CDECL void ptick_unhook(void);

enum : uint32_t { JDAY = 5184000 }; /* a day's 60 Hz jiffies */

static uint32_t toff; /* the jiffy clock's offset (the asm's io.toff) */

static PdosRegs int21(const PdosRegs in)
{
    PdosRegs r = in;
    pdos_int(INT_DOS, &r);
    return r;
}

void pclk_init(volatile uint8_t *const flag, const uint8_t bit)
{
    ptick_bit = bit;
    ptick_flag = flag;
    ptick_hook();
}

void pclk_done(void) { ptick_unhook(); }

uint32_t pclk_ticks(void)
{
    volatile const uint8_t *const p = pdos_lin(BDA_TICKS);
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

void pclk_time(PclkTime *const t)
{
    const PdosRegs r = int21((PdosRegs){.eax = pdos_fn(DOS_GET_TIME, 0)});

    *t = (PclkTime){
        .hour = r.ecx >> 8,
        .min = r.ecx,
        .sec = r.edx >> 8,
        .hund = r.edx,
    };
}

void pclk_date(PclkDate *const d)
{
    const PdosRegs r = int21((PdosRegs){.eax = pdos_fn(DOS_GET_DATE, 0)});

    *d = (PclkDate){.year = r.ecx, .month = r.edx >> 8, .day = r.edx};
}

bool pclk_set_time(const PclkTime *const t)
{
    const PdosRegs r = int21((PdosRegs){.eax = pdos_fn(DOS_SET_TIME, 0),
                                        .ecx = t->hour << 8 | t->min,
                                        .edx = t->sec << 8 | t->hund});

    return (r.eax & 0xFF) == 0;
}

bool pclk_set_date(const PclkDate *const d)
{
    const PdosRegs r = int21(
        (PdosRegs){.eax = pdos_fn(DOS_SET_DATE, 0), .ecx = d->year, .edx = d->month << 8 | d->day}

    );
    return (r.eax & 0xFF) == 0;
}

/* The ticks as jiffies: ticks * 3295 / 1000, without a 64-bit product. */
static uint32_t jiffies(void)
{
    const uint32_t t = pclk_ticks();
    return t * 3 + t * 295 / 1000;
}

uint32_t pclk_jiffies(void) { return (jiffies() + toff) % JDAY; }

void pclk_set_jiffies(const uint32_t j) { toff = j - jiffies() + JDAY; }

enum : uint16_t { PIT_COUNT0 = 0x40, PIT_CONTROL = 0x43 };
enum : uint8_t { PIT_READ_BACK0 = 0xC2, PIT_OUT_HIGH = 0x80 };
enum : uint32_t { PIT_STEPS = 65536, PIT_HZ = 1193182, TICK_US = 54925, MICRO = 1000000 };

/* Forward until midnight. */
uint64_t pclk_us(void)
{
    static uint64_t last;
    uint32_t ticks, status, left;

    do {
        ticks = pclk_ticks();
        psys_outp(PIT_CONTROL, PIT_READ_BACK0);
        status = psys_inp(PIT_COUNT0);
        left = psys_inp(PIT_COUNT0);
        left |= (uint32_t)psys_inp(PIT_COUNT0) << 8;
    } while (ticks != pclk_ticks());

    const uint32_t half = status & PIT_OUT_HIGH ? 0 : PIT_STEPS;
    const uint32_t steps = (half + PIT_STEPS - (left ? left : PIT_STEPS)) / 2;
    uint64_t now = ((uint64_t)ticks * PIT_STEPS + steps) * MICRO / PIT_HZ;

    /* the chip reloads before the tick's interrupt counts it: that moment reads a tick behind */
    if (now < last)
        now = last - now > TICK_US / 2 ? now + TICK_US : last;

    last = now;
    return now;
}
