/* Event trapping (src/vm/event.asm): ON TIMER, KEY, COM, PEN, STRIG GOSUB. */
#include "vm/evnt.h"
#include "io/repl.h"
#include "mem/strg.h"
#include "plat/pclk.h"
#include "plat/pkey.h"
#include "vm/flow.h"

enum : uint32_t { TDAY = 0x1800B0 }; /* the BIOS's ticks a day */

/* KEY n's scan code: F1..F10, up, left, right, down, 15..25 the user's, F11, F12. */
static const uint8_t keyscan[32] = {
    0, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x48, 0x4B, 0x4D, 0x50,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x85, 0x86,
};

Evnt evnt;

/* ev_slot: the slot of kind (EK_*) and n. Illegal function call: no such n. */
static uint32_t slot_of(uint8_t kind, int32_t n)
{
    switch (kind) {
    case EK_TIMER: return 0;
    case EK_KEY:
        if ((uint32_t)(n - 1) >= 31)
            repl_error(E_IQ);
        return (uint32_t)n;
    case EK_COM:
        if ((uint32_t)(n - 1) >= 2)
            repl_error(E_IQ);
        return EV_COM + (uint32_t)n - 1;
    case EK_PEN: return EV_PEN;
    default:
        if ((uint32_t)n >= 8)
            repl_error(E_IQ);
        return EV_STRIG + (uint32_t)n / 2;
    }
}

/* ev_sig: slot i's event can run now: SF_EV. */
static void signal(uint32_t i)
{
    const EventSlot *s = &evnt.slot[i];
    if (s->line && s->state == ES_ON && s->flags == EF_PENDING)
        exec.stop |= SF_EV;
}

/* The keys ON KEY traps, to the platform: a slot's while it is not OFF. */
static void traps(void)
{
    PkeyTrap t[PKEY_NTRAP] = {};
    for (uint32_t i = EV_KEY; i < PKEY_NTRAP; i++) {
        if (!evnt.slot[i].state)
            continue;
        if (i >= 15 && i <= 25) {
            uint16_t k = evnt.ukey[i - 15];
            t[i] = (PkeyTrap){.scan = (uint8_t)(k >> 8), .shift = k & 0x0F};
        } else
            t[i] = (PkeyTrap){.scan = keyscan[i], .shift = PKEY_ANY};
    }
    pkey_trap(t);
}

/* ONEV kind num line ( n -- ): ON event(n) GOSUB's line, 0 none; ON TIMER's n the seconds,
   1..86400. Illegal function call: no such n. Label not defined. */
void evnt_onev(void)
{
    int32_t n = exec_popi();
    uint32_t i = slot_of(exec_u8(), n);
    if (!i) { /* in ticks */
        if ((uint32_t)(n - 1) >= 86400)
            repl_error(E_IQ);
        evnt.interval = evnt.count = ((uint32_t)n * 182065u + 5000) / 10000;
    }
    uint32_t line = 0;
    if (!rd16(addr_of(exec.ip)))
        exec.ip += sizeof(LineRef);
    else
        line = flow_target();
    evnt.slot[i].line = line;
}

/* event(n) ON, OFF, STOP ( n ks -- ): ks the kind * 4 + the state. OFF drops one pending; ON
   runs one. */
void evnt_evst(void)
{
    uint32_t ks = exec_pop().u;
    uint32_t i = slot_of((uint8_t)(ks >> 2), exec_popi());
    EventSlot *s = &evnt.slot[i];
    uint8_t old = s->state;
    s->state = ks & 3;
    if (!s->state)
        s->flags &= ~EF_PENDING;
    if (!i && !old) /* the TIMER started: a whole interval */
        evnt.count = evnt.interval;
    signal(i);
    traps();
}

/* KEY n, s ( n s -- ): KEY 15..25's key, s its shift flags and scan code; another n's text is
   the screen's, not kept. Illegal function call: n not 1..25, 30, 31. */
void evnt_keydef(void)
{
    Desc s = strg_get(exec_popa());
    uint32_t k = exec_pop().u - 1;
    if (k >= 25 && (k < 29 || k >= 31))
        repl_error(E_IQ);
    k -= 14;
    if (k >= NUKEY) /* 1..14, 30, 31 */
        return;
    if (s.len < 2)
        repl_error(E_IQ);
    evnt.ukey[k] = rd16(s.ptr);
    traps();
}

/* Slot i's event happened. */
static void pend(uint32_t i)
{
    evnt.slot[i].flags |= EF_PENDING;
    signal(i);
}

void evnt_poll(void)
{
    exec.stop &= ~SF_TICK;
    uint32_t now = pclk_ticks();
    uint32_t n = now >= evnt.tick ? now - evnt.tick : now + TDAY - evnt.tick;
    evnt.tick = now;
    uint32_t keys = pkey_taken();
    if (!evnt.on)
        return;
    if (evnt.slot[0].state && evnt.interval && n) {
        if (n < evnt.count)
            evnt.count -= n;
        else {
            evnt.count = evnt.interval - (n - evnt.count) % evnt.interval;
            pend(0);
        }
    }
    for (uint32_t i = EV_KEY; i < PKEY_NTRAP && keys >> i; i++)
        if (keys >> i & 1)
            pend(i);
}

bool evnt_run(void)
{
    exec.stop &= ~SF_EV;
    exec.ip = exec.opip;
    for (uint32_t i = 0; i < NEV; i++) {
        EventSlot *s = &evnt.slot[i];
        if (s->line && s->state == ES_ON && s->flags == EF_PENDING) {
            s->flags = EF_RUNNING; /* no longer pending */
            Addr f = flow_push(M_EV);
            ((EventFrame *)at(f))->slot = (uint8_t)i;
            flow_jump(s->line);
            break;
        }
    }
    return true;
}

void evnt_return(const EventFrame *f)
{
    evnt.slot[f->slot].flags &= ~EF_RUNNING;
    signal(f->slot);
}

void evnt_clear(void)
{
    memset(evnt.slot, 0, sizeof evnt.slot);
    evnt.on = true;
    evnt.tick = pclk_ticks();
    pkey_taken();
    traps();
}
