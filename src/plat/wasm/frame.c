/* The display on wasm: the page's thread calls frame() in its own instance of the module, on the
 * shared memory, with its own stack (render_stack).
 */
#include "plat/view/view.h"
#include "wasm.h"

enum { STACK = 64 << 10 };

alignas(16) static uint8_t stack[STACK];

[[clang::export_name("render_stack")]]
uint8_t *render_stack(void)
{
    return stack + STACK;
}

[[clang::export_name("frame")]]
const ViewFrame *frame(const int blink)
{
    const ViewPointer pointer = {
        .shown = __atomic_load_n(&wasm_irq.mouse_shown, __ATOMIC_ACQUIRE) > 0,
        .row = wasm_irq.mouse_row,
        .col = wasm_irq.mouse_col,
    };

    return view_frame(blink, pointer);
}
