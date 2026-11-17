/* The mouse on the host: a window's (host.h), none without one. */
#include "plat/host/host.h"
#include "plat/pmou.h"

static struct {
    bool on;
    int32_t shown;
    uint32_t state; /* row, col, buttons: a byte each, from the low */
} mouse;

void host_mouse_on(void) { mouse.on = true; }

void host_mouse_set(const uint8_t row, const uint8_t col, const uint8_t buttons)
{
    __atomic_store_n(
        &mouse.state,
        row | (uint32_t)col << 8 | (uint32_t)buttons << 16,
        __ATOMIC_RELAXED
    );
}

bool host_mouse_shown(void) { return __atomic_load_n(&mouse.shown, __ATOMIC_RELAXED) > 0; }

bool pmou_init(void)
{
    __atomic_store_n(&mouse.shown, 0, __ATOMIC_RELAXED);
    return mouse.on;
}

void pmou_show(void) { __atomic_add_fetch(&mouse.shown, 1, __ATOMIC_RELAXED); }
void pmou_hide(void) { __atomic_sub_fetch(&mouse.shown, 1, __ATOMIC_RELAXED); }

PmouState pmou_state(void)
{
    const uint32_t s = __atomic_load_n(&mouse.state, __ATOMIC_RELAXED);
    return (PmouState){.row = (uint8_t)s, .col = (uint8_t)(s >> 8), .buttons = (uint8_t)(s >> 16)};
}
