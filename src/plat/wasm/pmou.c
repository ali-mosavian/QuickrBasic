/* The mouse on wasm: the page's pointer, written to the shared block (wasm.h); the display draws its
 * cursor (frame.c). */
#include "plat/pmou.h"
#include "wasm.h"

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
    };
}
