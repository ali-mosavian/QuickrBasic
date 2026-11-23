/* The clock on DOS32.
 *
 * # How it works
 * Time and date are INT 21h's, the ticks the BIOS count; the tick is INT 1Ch (ptick.asm).
 */
#include "dos.h"
#include "plat/pclk.h"

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

/* The BIOS's ticks, 54925 us each, until the PIT's count is read: forward until midnight. */
uint64_t pclk_us(void) { return (uint64_t)pclk_ticks() * 54925; }
