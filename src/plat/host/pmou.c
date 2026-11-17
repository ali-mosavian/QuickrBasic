/* The mouse on the host: a window's (host.h), none without one.
 *
 * # How it works
 * One writer (the window's thread or the terminal's) and the run's reads: the state is a word, and a
 * button's presses are a word the writer adds to and pmou_press takes whole.
 */
#include "plat/host/host.h"
#include "plat/pmou.h"

static struct {
    bool on;
    int32_t shown;
    uint64_t state;                /* row, col, buttons a byte each from the low, then x, y */
    uint64_t press[PMOU_NBUTTONS]; /* pack's: the count in the low 16 bits */
} mouse;

static uint64_t pack(
    const uint16_t lo,
    const uint8_t row,
    const uint8_t col,
    const uint16_t x,
    const uint16_t y
)
{
    return lo | (uint64_t)row << 16 | (uint64_t)col << 24 | (uint64_t)x << 32 | (uint64_t)y << 48;
}

void host_mouse_on(void) { mouse.on = true; }

void host_mouse_set(
    const uint8_t row,
    const uint8_t col,
    const uint16_t x,
    const uint16_t y,
    const uint8_t buttons
)
{
    const uint8_t was = (uint8_t)(__atomic_load_n(&mouse.state, __ATOMIC_RELAXED) >> 16);

    const uint64_t now =
        row | (uint64_t)col << 8 | (uint64_t)buttons << 16 | (uint64_t)x << 32 | (uint64_t)y << 48;
    __atomic_store_n(&mouse.state, now, __ATOMIC_RELAXED);

    for (uint8_t b = 0; b < PMOU_NBUTTONS; b++) {
        if (!(buttons & ~was & 1u << b))
            continue;

        uint64_t old = __atomic_load_n(&mouse.press[b], __ATOMIC_RELAXED);

        while (!__atomic_compare_exchange_n(
            &mouse.press[b],
            &old,
            pack((uint16_t)(old + 1), row, col, x, y),
            false,
            __ATOMIC_RELAXED,
            __ATOMIC_RELAXED
        ))
            ;
    }
}

bool host_mouse_shown(void) { return __atomic_load_n(&mouse.shown, __ATOMIC_RELAXED) > 0; }

bool pmou_init(void)
{
    __atomic_store_n(&mouse.shown, 0, __ATOMIC_RELAXED);
    return mouse.on;
}

bool pmou_there(void) { return mouse.on; }

void pmou_show(void) { __atomic_add_fetch(&mouse.shown, 1, __ATOMIC_RELAXED); }
void pmou_hide(void) { __atomic_sub_fetch(&mouse.shown, 1, __ATOMIC_RELAXED); }

PmouState pmou_state(void)
{
    const uint64_t s = __atomic_load_n(&mouse.state, __ATOMIC_RELAXED);
    return (PmouState){
        .row = (uint8_t)s,
        .col = (uint8_t)(s >> 8),
        .buttons = (uint8_t)(s >> 16),
        .x = (uint16_t)(s >> 32),
        .y = (uint16_t)(s >> 48),
    };
}

PmouPress pmou_press(const uint8_t button)
{
    const uint64_t p = __atomic_exchange_n(&mouse.press[button], 0, __ATOMIC_RELAXED);
    return (PmouPress){
        .count = (uint16_t)p,
        .row = (uint8_t)(p >> 16),
        .col = (uint8_t)(p >> 24),
        .x = (uint16_t)(p >> 32),
        .y = (uint16_t)(p >> 48),
    };
}
