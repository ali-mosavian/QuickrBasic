/* Event trapping's interface. How it works: event.c. */
#ifndef CORE_VM_EVNT_H
#define CORE_VM_EVNT_H
#include "vm/exec.h"

/* The slots: TIMER, KEY(1) .. KEY(31), COM(1), COM(2), PEN, STRIG(0, 2, 4, 6); KEY 15 .. 25 the
   user's keys. */
enum {
    EV_TIMER = 0,
    EV_KEY = 1,
    NKEYS = 31,
    EV_COM = EV_KEY + NKEYS,
    NCOMS = 2,
    EV_PEN = EV_COM + NCOMS,
    EV_STRIG = EV_PEN + 1,
    NSTRIGS = 4,
    NEV = EV_STRIG + NSTRIGS,
    UKEY_FIRST = 15,
    UKEY_LAST = 25,
    NUKEY = UKEY_LAST - UKEY_FIRST + 1,
};
enum : uint8_t { EF_PENDING = 1, EF_RUNNING = 2 }; /* its flags */

typedef struct {
    EventSlot slot[NEV];  /* g_evtab */
    uint32_t interval;    /* g_evti: ON TIMER's ticks */
    uint32_t count;       /* g_evtc: its count down */
    uint16_t ukey[NUKEY]; /* g_evuk: KEY 15 .. 25's key: shift flags, scan code */
    bool on;              /* g_evon: events are counted */
    uint32_t tick;        /* the platform's tick count last counted */
} Evnt;

/* ev_clr: no events: a run's start, READY. */
void evnt_clear(Session *S);
/* ev_tick: the ticks and trapped keys since the last poll counted; SF_TICK cleared, SF_EV set
   for an event to run. */
void evnt_poll(Evnt *ev, Vm *vm);
/* ev_run: vm.stop's SF_EV: an ON slot's pending event runs, a GOSUB to its line coming back to
   the op at vm.opip; true: vm.ip moved to it. */
bool evnt_run(Session *S);
/* ev_ret: RETURN from an event's frame: its slot's next event can run. */
void evnt_return(Evnt *ev, Vm *vm, uint8_t slot);
#endif
