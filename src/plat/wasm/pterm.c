/* The keyboard on wasm: the page's keys, through the shared ring (wasm.h), as the host's terminal
 * (host.h) to the host's pkey.c.
 *
 * # How it works
 * The page writes a key at tail and notifies; a read waits on tail with Atomics.wait.
 */
#include "plat/host/host.h"
#include "wasm.h"

WasmIrq wasm_irq;

[[clang::export_name("irq")]]
WasmIrq *irq(void)
{
    return &wasm_irq;
}

void host_term_open(void) {}
void host_term_pause(const bool yes) {}
bool host_term_on(void) { return wasm_irq.keyboard; }

static uint32_t tail(void) { return __atomic_load_n(&wasm_irq.tail, __ATOMIC_ACQUIRE); }

bool host_term_ready(void) { return tail() != wasm_irq.head; }

uint16_t host_term_key(void)
{
    const uint32_t head = wasm_irq.head;

    while (tail() == head)
        __builtin_wasm_memory_atomic_wait32((int32_t *)&wasm_irq.tail, (int32_t)head, -1);

    const uint16_t k = wasm_irq.keys[head % WASM_NKEY];
    __atomic_store_n(&wasm_irq.head, head + 1, __ATOMIC_RELEASE);
    return k;
}
