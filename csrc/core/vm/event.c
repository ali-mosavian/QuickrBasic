/* Event trapping (src/vm/event.asm): ON TIMER, KEY, COM, PEN, STRIG GOSUB. */
#include "vm/event.h"
#include "session.h"
#include "io/repl.h"
#include "mem/string.h"
#include "plat/pclk.h"
#include "plat/pkey.h"
#include "vm/flow.h"

enum : uint32_t {
    TICKS_A_DAY = 0x1800B0, /* the BIOS's */
    TICKS_PER_10000_SECONDS = 182065,
};
enum { TIMER_MAX_SECONDS = 86400, STRIG_MAX = 7 /* STRIG(0) .. STRIG(6), two numbers a slot */ };

/* KEY n's scan code: F1..F10, up, left, right, down, then the user's, F11, F12. */
static const uint8_t keyscan[NKEYS + 1] = {
    [1] = 0x3B,
    0x3C,
    0x3D,
    0x3E,
    0x3F,
    0x40,
    0x41,
    0x42,
    0x43,
    0x44,
    0x48,
    0x4B,
    0x4D,
    0x50,
    [30] = 0x85,
    0x86,
};

/* The slot of kind (EK_*) and n. Illegal function call: no such n. */
static uint32_t slot_of(Session *const S, const uint8_t kind, const int32_t n)
{
    switch (kind) {
    case EK_TIMER:
        return EV_TIMER;

    case EK_KEY:
        if (n < 1 || n > NKEYS)
            repl_error(S, E_IQ);

        return EV_KEY + (uint32_t)n - 1;

    case EK_COM:
        if (n < 1 || n > NCOMS)
            repl_error(S, E_IQ);

        return EV_COM + (uint32_t)n - 1;

    case EK_PEN:
        return EV_PEN;

    default:
        if (n < 0 || n > STRIG_MAX)
            repl_error(S, E_IQ);

        return EV_STRIG + (uint32_t)n / 2;
    }
}

/* Can slot s's event run now? */
static bool runnable(const EventSlot *const s)
{
    return s->line && s->state == ES_ON && s->flags == EF_PENDING;
}

/* Slot i's event can run now: SF_EV. */
static void signal(Session *const S, const uint32_t i)
{
    if (runnable(&S->evnt.slot[i]))
        S->vm.stop |= SF_EV;
}

static bool is_user_key(const uint32_t i) { return i >= UKEY_FIRST && i <= UKEY_LAST; }

/* The keys ON KEY traps, to the platform: a slot's while it is not OFF. */
static void traps(Session *const S)
{
    PkeyTrap t[PKEY_NTRAP] = {};

    for (uint32_t i = EV_KEY; i < PKEY_NTRAP; i++) {
        if (S->evnt.slot[i].state == ES_OFF)
            continue;
        if (is_user_key(i)) {
            const uint16_t k = S->evnt.ukey[i - UKEY_FIRST];
            t[i] = (PkeyTrap){.scan = (uint8_t)(k >> 8), .shift = k & 0x0F};
        } else
            t[i] = (PkeyTrap){.scan = keyscan[i], .shift = PKEY_ANY};
    }

    pkey_trap(t);
}

/* ONEV kind num line ( n -- ): ON event(n) GOSUB's line, 0 none; ON TIMER's n the seconds,
   1..TIMER_MAX_SECONDS. Illegal function call: no such n. Label not defined. */
void evnt_onev(Session *const S)
{
    const int32_t n = exec_popi(&S->vm);
    const uint32_t i = slot_of(S, exec_u8(&S->vm), n);

    if (i == EV_TIMER) {
        if (n < 1 || n > TIMER_MAX_SECONDS)
            repl_error(S, E_IQ);

        S->evnt.interval = S->evnt.count = ((uint32_t)n * TICKS_PER_10000_SECONDS + 5000) / 10000;
    }

    uint32_t line = 0;

    if (!rd16(addr_of(S->vm.ip)))
        S->vm.ip += sizeof(LineRef);
    else
        line = flow_target(S);

    S->evnt.slot[i].line = line;
}

/* event(n) ON, OFF, STOP ( n kind_state -- ): the kind * 4 + the state. OFF drops one pending;
   ON runs one. */
void evnt_evst(Session *const S)
{
    const uint32_t kind_state = exec_pop(&S->vm).u;
    const uint32_t i = slot_of(S, (uint8_t)(kind_state >> 2), exec_popi(&S->vm));
    EventSlot *const s = &S->evnt.slot[i];
    const uint8_t old = s->state;
    s->state = kind_state & 3;

    if (s->state == ES_OFF)
        s->flags &= ~EF_PENDING;
    if (i == EV_TIMER && old == ES_OFF) /* the TIMER started: a whole interval */
        S->evnt.count = S->evnt.interval;

    signal(S, i);
    traps(S);
}

/* KEY n, s ( n s -- ): a user key's, s its shift flags and scan code; another n's text is the
   screen's, not kept. Illegal function call: n not 1..25, 30, 31. */
void evnt_keydef(Session *const S)
{
    const Desc s = strg_get(S, exec_popa(&S->vm));
    const uint32_t n = exec_pop(&S->vm).u;

    if (n < 1 || (n > UKEY_LAST && n < 30) || n > NKEYS)
        repl_error(S, E_IQ);
    if (!is_user_key(n))
        return;
    if (s.len < 2)
        repl_error(S, E_IQ);

    S->evnt.ukey[n - UKEY_FIRST] = rd16(s.ptr);
    traps(S);
}

/* Slot i's event happened. */
static void pend(Session *const S, const uint32_t i)
{
    S->evnt.slot[i].flags |= EF_PENDING;
    signal(S, i);
}

void evnt_poll(Session *const S)
{
    S->vm.stop &= ~SF_TICK;
    const uint32_t now = pclk_ticks();
    const uint32_t n = now >= S->evnt.tick ? now - S->evnt.tick : now + TICKS_A_DAY - S->evnt.tick;
    S->evnt.tick = now;
    const uint32_t keys = pkey_taken();

    if (!S->evnt.on)
        return;
    if (S->evnt.slot[EV_TIMER].state != ES_OFF && S->evnt.interval && n) {
        if (n < S->evnt.count)
            S->evnt.count -= n;
        else {
            S->evnt.count = S->evnt.interval - (n - S->evnt.count) % S->evnt.interval;
            pend(S, EV_TIMER);
        }
    }

    for (uint32_t i = EV_KEY; i < PKEY_NTRAP && keys >> i; i++)
        if (keys >> i & 1)
            pend(S, i);
}

bool evnt_run(Session *const S)
{
    S->vm.stop &= ~SF_EV;
    S->vm.ip = S->vm.opip;

    for (uint32_t i = 0; i < NEV; i++) {
        EventSlot *const s = &S->evnt.slot[i];

        if (runnable(s)) {
            s->flags = EF_RUNNING; /* no longer pending */
            flow_push(S, FRAME_EVENT)->event = (uint8_t)i;
            flow_jump(S, s->line);
            break;
        }
    }

    return true;
}

void evnt_return(Session *const S, const uint8_t slot)
{
    S->evnt.slot[slot].flags &= ~EF_RUNNING;
    signal(S, slot);
}

void evnt_clear(Session *const S)
{
    memset(S->evnt.slot, 0, sizeof S->evnt.slot);
    S->evnt.on = true;
    S->evnt.tick = pclk_ticks();
    pkey_taken();
    traps(S);
}
