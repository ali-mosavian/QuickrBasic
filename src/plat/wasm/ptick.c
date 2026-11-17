/* The 18.2 Hz tick on wasm: the page's timer ORs the bit in (wasm.h), as an interrupt would. */
#include "plat/pclk.h"
#include "wasm.h"

void pclk_init(volatile uint8_t *const flag, const uint8_t bit)
{
    wasm_irq.tick_bit = bit;
    __atomic_store_n(&wasm_irq.tick, flag, __ATOMIC_RELEASE);
}

void pclk_done(void) { __atomic_store_n(&wasm_irq.tick, nullptr, __ATOMIC_RELEASE); }
