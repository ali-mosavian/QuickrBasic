/* The keyboard on the host: the BIOS's keys from a queue tests fill (host.h); DOS's console
   input (pkey_poll) from standard input, as DOS reads it when redirected. */
#include <poll.h>
#include <unistd.h>
#include "plat/host/host.h"
#include "plat/pkey.h"

enum { NKEY = 64 };
enum : uint16_t { KEY_ENTER = 0x1C0D }; /* what a read gets with the queue empty */

static struct {
    uint16_t keys[NKEY];
    uint32_t head, count;
} queue;

static struct {
    PkeyTrap traps[PKEY_NTRAP];
    uint32_t taken;
    volatile uint8_t *flag;
    uint8_t bit;
} trap;

/* A trap set for key's scan code takes it, as DOS32's tick does at the buffer's head. */
static bool trapped(const uint16_t key)
{
    const uint8_t scan = (uint8_t)(key >> 8);

    for (uint32_t i = 1; i < PKEY_NTRAP; i++) {
        const PkeyTrap t = trap.traps[i];

        if (t.scan && t.scan == scan && (t.shift == PKEY_ANY || t.shift == (pkey_shift() & 0x0F))) {
            trap.taken |= 1u << i;

            if (trap.flag)
                *trap.flag |= trap.bit;

            return true;
        }
    }

    return false;
}

void host_key_push(const uint16_t key)
{
    if (queue.count == NKEY || trapped(key))
        return;

    queue.keys[(queue.head + queue.count++) % NKEY] = key;
}

static uint16_t next_key(void)
{
    if (!queue.count)
        return KEY_ENTER;

    const uint16_t k = queue.keys[queue.head];
    queue.head = (queue.head + 1) % NKEY;
    queue.count--;
    return k;
}

bool pkey_ready(void) { return queue.count > 0; }

PkeyKey pkey_read(void)
{
    const uint16_t k = next_key();
    return (PkeyKey){.ch = (uint8_t)k, .scan = (uint8_t)(k >> 8)};
}

uint16_t pkey_wait(void) { return next_key(); }
uint8_t pkey_shift(void) { return 0; }
void pkey_flush(void) { queue.count = 0; }

uint8_t pkey_poll(void)
{
    struct pollfd p = {.fd = STDIN_FILENO, .events = POLLIN};

    uint8_t b;

    if (poll(&p, 1, 0) <= 0 || read(STDIN_FILENO, &b, 1) != 1)
        return 0;

    return b;
}

void pkey_init(volatile uint8_t *const flag, const uint8_t bit)
{
    trap.flag = flag;
    trap.bit = bit;
}

void pkey_trap(const PkeyTrap traps[PKEY_NTRAP])
{
    for (uint32_t i = 0; i < PKEY_NTRAP; i++)
        trap.traps[i] = traps[i];
}

uint32_t pkey_taken(void)
{
    const uint32_t t = trap.taken;
    trap.taken = 0;
    return t;
}
