/* Event trapping (src/vm/event.asm): ON TIMER, KEY, COM, PEN, STRIG GOSUB.

   A slot per event (EventSlot, defs.h): 0 TIMER, 1 .. 31 KEY(n), 32 .. 33 COM(n), 34 PEN, 35 .. 38
   STRIG(0, 2, 4, 6). The platform's tick and its trapped keys set exec.stop's SF_TICK; the STOP
   check then counts them (evnt_poll), and an event an ON slot can run sets SF_EV, so the check
   runs it (evnt_run): a GOSUB to its line that comes back to the op that checked. Its slot runs
   no other event till that RETURN. COM, PEN and STRIG are accepted and never fire. */
#ifndef CORE_VM_EVNT_H
#define CORE_VM_EVNT_H
#include "vm/exec.h"

enum { NEV = 39, EV_KEY = 1, EV_COM = 32, EV_PEN = 34, EV_STRIG = 35, NUKEY = 11 };
enum : uint8_t { ES_OFF = 0, ES_ON = 1, ES_STOP = 2 };    /* a slot's state */
enum : uint8_t { EF_PENDING = 1, EF_RUNNING = 2 };        /* its flags */

typedef struct {
    EventSlot slot[NEV]; /* g_evtab */
    uint32_t interval;   /* g_evti: ON TIMER's ticks */
    uint32_t count;      /* g_evtc: its count down */
    uint16_t ukey[NUKEY]; /* g_evuk: KEY 15 .. 25's key: shift flags, scan code */
    bool on;             /* g_evon: events are counted */
    uint32_t tick;       /* the platform's tick count last counted */
} Evnt;
extern Evnt evnt;

/* ev_clr: no events: a run's start, READY. */
void evnt_clear(void);
/* ev_tick: the ticks and trapped keys since the last poll counted; SF_TICK cleared, SF_EV set
   for an event to run. */
void evnt_poll(void);
/* ev_run: exec.stop's SF_EV: an ON slot's pending event runs, a GOSUB to its line coming back to
   the op at exec.opip; true: exec.ip moved to it. */
bool evnt_run(void);
/* ev_ret: RETURN from an event's frame: its slot's next event can run. */
void evnt_return(const EventFrame *f);
#endif
