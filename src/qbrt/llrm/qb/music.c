/* The music queues (music.h): a ring for each voice that the clock tick
   empties: the program writes `head` and the tick writes `tail`, so neither
   needs the other to stop.  SOUND (QB rt B$SOND) queues in the first. */
#include <string.h>

#include "music.h"
#include "nhstutil.h"
#include "song.h"

/* The queues count time in parts of a clock tick: a synthesizer that renders its own samples ticks them from its
   sample count (music_fine_tick, after MGL's mixer and its module player), else the clock tick runs a tick's parts
   at once. */
enum { FINE = MUSIC_FINE, FINE_KEPT = 2 };

typedef struct Note {
    unsigned sound, quiet; /* parts of tone, then of silence */
    byte synth;            /* through dev_voice, else the speaker */
    DevVoice v[DEV_LAYERS]; /* v[0].hertz 0 for a rest; its layers after it */
} Note;

enum {
    QUEUE = 19
}; /* one place is left empty: 18 notes wait before a background PLAY does, as
      with BCOM45 */

typedef struct Voice {
    Note queue[QUEUE];
    volatile unsigned head, tail;
    volatile unsigned sound_left, quiet_left;
    long owed_sound, owed_quiet;
    unsigned long end; /* the tick the queue's last note ends at */
    /* the tick's: the note now is the synthesizer's; it is gated; the synthesizer has sounded since the last cut */
    byte synth, gated, used;
    DevVoice now[DEV_LAYERS];
} Voice;

static Voice voices[DEV_VOICES];
static volatile unsigned long music_ticks;
static byte tick_started;
static volatile byte fine_live; /* clock ticks the synthesizer's ticking is trusted for */
byte music_foreground = 1;

#pragma aux music_tick "B$MUSICTICK"
#pragma aux B_SOND "B$SOND"

static void note_off(Voice *v, unsigned index)
{
    if (!v->synth)
        dev_tone(0);
    else if (v->gated) {
        v->gated = 0;
        v->now[0].gate = 0;
        dev_voice(index, v->now);
    }
}

/* A note run into by the one before it (legato), at its pitch, is that note held: not struck again. */
static void note_on(Voice *v, unsigned index, const Note *n)
{
    const int tied = v->synth && v->gated && n->synth && v->now[0].hertz == n->v[0].hertz;
    const unsigned char trigger = (unsigned char)(v->now[0].trigger + !tied);

    v->synth = n->synth;
    if (!n->v[0].hertz)
        return;
    if (!n->synth) {
        dev_tone(n->v[0].hertz);
        return;
    }
    song_taken(index);
    memcpy(v->now, n->v, (1 + n->v[0].layers) * sizeof n->v[0]);
    v->now[0].gate = 1;
    v->now[0].trigger = trigger;
    v->gated = v->used = 1;
    dev_voice(index, v->now);
}

/* Whether the note ending runs into the next (legato) at its own pitch: it is held, not let go. */
static int tied(const Voice *v)
{
    const Note *const next = &v->queue[v->tail];

    return v->synth && v->gated && !v->quiet_left && v->tail != v->head && next->synth && next->sound &&
           next->v[0].hertz == v->now[0].hertz;
}

/* A voice's tick: counts the note out, and starts the next. */
static void voice_tick(Voice *v, unsigned index)
{
    if (v->sound_left && --v->sound_left == 0 && !tied(v))
        note_off(v, index);
    else if (!v->sound_left && v->quiet_left)
        v->quiet_left--;
    /* a note too short to last a tick is passed over */
    while (!v->sound_left && !v->quiet_left && v->tail != v->head) {
        const Note *next = &v->queue[v->tail];

        v->sound_left = next->sound;
        v->quiet_left = next->quiet;
        if (v->sound_left)
            note_on(v, index, next);
        v->tail = (v->tail + 1) % QUEUE;
    }
}

static void part_tick(void)
{
    unsigned i;

    music_ticks++;
    for (i = 0; i < DEV_VOICES; i++)
        voice_tick(&voices[i], i);
    song_tick();
}

/* The clock tick: its parts, unless the synthesizer is ticking them. */
void music_tick(void)
{
    unsigned i;

    if (fine_live) {
        fine_live--;
        return;
    }
    for (i = 0; i < FINE; i++)
        part_tick();
}

void music_fine_tick(void)
{
    fine_live = FINE_KEPT;
    part_tick();
}

/* The notes waiting: PLAY(n) and ON PLAY (events.c).  The first voice's, whatever n is: QB's n is a dummy. */
unsigned music_waiting(void)
{
    return (voices[0].head + QUEUE - voices[0].tail) % QUEUE;
}

/* What a wait for the tick does between looks at the queue: nothing where the tick runs by itself (a timer interrupt, a
   thread); a host with one thread that runs it from outside (wasm32) says so with its own. */
#ifndef QB_IDLE
#define QB_IDLE() ((void)0)
#endif

static void wait_for_room(const Voice *v)
{
    while ((v->head + 1) % QUEUE == v->tail)
        QB_IDLE();
}

/* In the foreground a note waits for the queue ahead of it and for the note sounding to end, not for the silence that
   follows it; a rest counts as sound. */
static void wait_for_turn(const Voice *v)
{
    if (!tick_started) {
        dev_tick_start();
        tick_started = 1;
    }
    if (music_foreground)
        while (v->tail != v->head || v->sound_left)
            QB_IDLE();
    wait_for_room(v);
}

static unsigned long queue_end(const Voice *v)
{
    return v->end > music_ticks ? v->end : music_ticks;
}

unsigned long music_end(unsigned voice)
{
    return queue_end(&voices[voice]);
}

void music_align(unsigned voice)
{
    Voice *const v = &voices[voice];

    if (v->tail == v->head && !v->sound_left && !v->quiet_left)
        v->owed_sound = v->owed_quiet = 0;
}

/* A time in thousandths of a tick as whole ticks, the part left over kept in
   `owed` for the next time: a run of short notes takes as long as they should,
   though each is a whole number of ticks. */
static unsigned ticks_of(long thousandths, long *owed)
{
    long whole = (*owed + thousandths) / 1000;

    *owed = *owed + thousandths - whole * 1000;
    return (unsigned)whole;
}

/* The same in parts of a tick, for the synthesizer's notes. */
static unsigned parts_of(long thousandths, long *owed)
{
    return ticks_of(thousandths * FINE, owed);
}

void music_queue(unsigned voice, byte synth, const DevVoice *v, long sound, long quiet)
{
    Voice *const at = &voices[voice];
    Note *n;

    if (synth)
        dev_synth_open();
    wait_for_turn(at);
    n = &at->queue[at->head];
    n->synth = synth;
    memcpy(n->v, v, (1 + v->layers) * sizeof *v);
    if (synth) {
        /* the note's whole time on one count, so it starts where the bar puts it; its sound the nearest tick */
        const unsigned whole = parts_of(sound + quiet, &at->owed_sound);
        unsigned sounding = (unsigned)((sound * whole + (sound + quiet) / 2) / (sound + quiet ? sound + quiet : 1));

        if (!sounding && whole && sound)
            sounding = 1;
        n->sound = sounding;
        n->quiet = whole - sounding;
    } else {
        /* the speaker's: whole clock ticks, as QB's */
        n->sound = ticks_of(sound, &at->owed_sound) * FINE;
        n->quiet = ticks_of(quiet, &at->owed_quiet) * FINE;
    }
    at->end = queue_end(at) + n->sound + n->quiet;
    at->head = (at->head + 1) % QUEUE;
}

/* CLEAR's too (clear.c): the tick keeps running. */
void music_clear(void)
{
    static const DevVoice silence = {0};
    unsigned i;

    for (i = 0; i < DEV_VOICES; i++) {
        Voice *const v = &voices[i];

        v->tail = v->head;
        v->sound_left = v->quiet_left = 0;
        v->end = 0; /* what was queued ends now: a voice cleared is not behind the others */
        v->gated = 0;
        if (v->used) {
            v->used = 0;
            dev_voice(i, &silence);
        }
    }
    song_clear();
    dev_tone(0);
}

void music_start(void)
{
    dev_synth_open();
    if (!tick_started) {
        dev_tick_start();
        tick_started = 1;
    }
}

int music_idle(unsigned voice)
{
    const Voice *const v = &voices[voice];

    return v->tail == v->head && !v->sound_left && !v->quiet_left && !v->gated;
}

/* B$SOND: SOUND frequency, duration in clock ticks (18.2 a second).  In
   foreground music a note waits for the one sounding to end; in background
   music it joins the queue and the program goes on.  The note starts at the
   next tick.  A duration of 0 silences what is sounding. */
void B_SOND(short hertz, float duration)
{
    DevVoice v = {0};
    long ticks;

    if (duration < 0 || duration > 65535.5f)
        qb_error(BE_ILLFUN);
    ticks = (long)(duration * ((double)TICKS_PER_MINUTE / CLOCK_TICKS_PER_MINUTE) + 0.5);
    if (ticks == 0) {
        dev_tick_hold();
        music_clear();
        dev_tick_release();
        return;
    }
    if (hertz < 37)
        qb_error(BE_ILLFUN);
    v.hertz = (unsigned)hertz;
    music_queue(0, 0, &v, ticks * 1000, 0);
}

void music_halt(void)
{
    music_clear();
    dev_synth_close();
    if (tick_started)
        dev_tick_stop();
}
