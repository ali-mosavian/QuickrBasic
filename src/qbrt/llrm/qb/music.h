/* The music waiting to be played (music.c): a queue for each voice, which the clock tick plays through the speaker
   (dev_tone) or the synthesizer (dev_voice). PLAY (play.c) and SOUND fill it. */
#ifndef QB_MUSIC_H
#define QB_MUSIC_H

#include "dev.h"
#include "qb.h"

/* a minute of the BIOS clock's ticks (18.2 a second), in thousandths */
#define CLOCK_TICKS_PER_MINUTE 1092390L
/* a minute of the music's ticks, in thousandths: the BIOS clock's, unless the host ticks it faster
   (wasm: 100 a second) */
#ifndef TICKS_PER_MINUTE
#define TICKS_PER_MINUTE CLOCK_TICKS_PER_MINUTE
#endif

/* the parts of a tick the queues count in, and how often a synthesizer that ticks them does: 16ths of a hertz */
#define MUSIC_FINE 16
#define MUSIC_FINE_RATE 4661 /* 16 * 18.2065 * 16 */
/* A part of a tick has passed, by the synthesizer's count of its samples: the clock tick leaves the queues to it. */
void music_fine_tick(void);

/* music in the foreground (MF, the default): each note waits for the one sounding to end */
extern byte music_foreground;

/* Queues in `voice` a note of `v` and its v->layers layers after it (v->hertz 0 a rest), through the synthesizer if
   `synth`, `sound` and then `quiet` thousandths of a tick long; the parts of a tick are carried to the voice's next
   note. */
void music_queue(unsigned voice, byte synth, const DevVoice *v, long sound, long quiet);
/* The tick `voice`'s queue ends at, now if it is empty. */
unsigned long music_end(unsigned voice);
/* A voice with nothing queued carries no parts of a tick: voices that start together stay together. */
void music_align(unsigned voice);
/* The notes waiting in the first voice. */
unsigned music_waiting(void);
/* What is queued is not played, and the speaker and the voices are quiet. */
void music_clear(void);
/* The program has ended: music_clear, the tick stops, and the queues and songs' tables are given back. */
void music_halt(void);
/* The synthesizer open and the tick running, for notes that are not queued (song.c). */
void music_start(void);
/* Whether `voice` has nothing of PLAY's queued or sounding: a song may take it. */
int music_idle(unsigned voice);

/* PLAY's note n, 1 to 84, in Hz (play.c). */
unsigned play_hertz(int note);
/* `commands`, PLAY's, set a fresh sound and its layers, which `sound` takes; a note among them is an Illegal function
   call (play.c). */
void play_sound(SD *commands, DevVoice sound[DEV_LAYERS]);

#endif
