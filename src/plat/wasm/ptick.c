/* The 18.2 Hz tick on wasm: the page's timer ORs the bit in (wasm.h), as an interrupt would; and
 * the wait for the page's frame, which it notifies. */
#include "plat/host/host.h"
#include "plat/pclk.h"
#include "wasm.h"

void pclk_init(volatile uint8_t *const flag, const uint8_t bit)
{
    wasm_irq.tick_bit = bit;
    __atomic_store_n(&wasm_irq.tick, flag, __ATOMIC_RELEASE);
}

void pclk_done(void) { __atomic_store_n(&wasm_irq.tick, nullptr, __ATOMIC_RELEASE); }

void host_frame_wait(const uint32_t seen, const uint32_t us)
{
    if (seen)
        __builtin_wasm_memory_atomic_wait32(
            (int32_t *)&host_display()->frames,
            (int32_t)seen,
            (int64_t)us * 1000
        );
}
