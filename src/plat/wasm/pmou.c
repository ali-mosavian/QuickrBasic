/* The mouse on wasm: the page's pointer, written to the shared block (wasm.h); the display draws its
 * cursor (frame.c). */
#include "plat/pmou.h"
#include "wasm.h"

bool pmou_there(void) { return wasm_irq.keyboard; }

bool pmou_init(void)
{
    wasm_irq.mouse_shown = 0;
    wasm_irq.mouse_on = wasm_irq.keyboard; /* the page's, not node's */
    return wasm_irq.mouse_on;
}

void pmou_show(void) { __atomic_add_fetch(&wasm_irq.mouse_shown, 1, __ATOMIC_RELEASE); }
void pmou_hide(void) { __atomic_sub_fetch(&wasm_irq.mouse_shown, 1, __ATOMIC_RELEASE); }

PmouState pmou_state(void)
{
    return (PmouState){
        .row = __atomic_load_n(&wasm_irq.mouse_row, __ATOMIC_RELAXED),
        .col = __atomic_load_n(&wasm_irq.mouse_col, __ATOMIC_RELAXED),
        .buttons = __atomic_load_n(&wasm_irq.mouse_buttons, __ATOMIC_RELAXED),
        .x = __atomic_load_n(&wasm_irq.mouse_x, __ATOMIC_RELAXED),
        .y = __atomic_load_n(&wasm_irq.mouse_y, __ATOMIC_RELAXED),
    };
}

/* The page writes the place before it adds to the count. */
PmouPress pmou_press(const uint8_t button)
{
    const uint32_t n = __atomic_exchange_n(&wasm_irq.press_count[button], 0, __ATOMIC_ACQUIRE);
    const uint32_t xy = __atomic_load_n(&wasm_irq.press_xy[button], __ATOMIC_RELAXED);
    const uint16_t cell = __atomic_load_n(&wasm_irq.press_cell[button], __ATOMIC_RELAXED);
    return (PmouPress){
        .count = (uint16_t)n,
        .row = (uint8_t)cell,
        .col = (uint8_t)(cell >> 8),
        .x = (uint16_t)xy,
        .y = (uint16_t)(xy >> 16),
    };
}
