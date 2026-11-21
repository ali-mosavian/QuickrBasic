/* The synthesizer: the speaker's square wave, as QB's, and 32 voices after the Commodore SID's (a pulse, triangle,
   saw, noise or two-operator FM wave each, its ADSR envelope and its own resonant filter), mixed to 16-bit samples. Portable and
   self-contained: the Mac's audio callback (plat/sdl/audio.c), the page's AudioWorklet (built alone to synth.wasm),
   DOS's Sound Blaster interrupt and the tests render the same samples. All integer: no FPU state to keep in an
   interrupt, and no 64-bit division (a library call on i386). */
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

#include "qbrt/llrm/qb/dev/voice.h"

enum { SYNTH_CURVE = 6 }; /* SID's exponential fall: a step for each band of levels */
enum { SYNTH_SINE = 4096 }; /* FM's sine, a period: its steps 67 dB under the note */

typedef struct SynthOsc {
    DevVoice set;
    uint32_t phase, step; /* a period is 2^32 */
    uint32_t noise;       /* SID's 23-bit shift register */
    int32_t level;        /* the envelope, 0 to 2^24 */
    uint8_t stage;
    /* worked out when the voice is set: the envelope's steps a sample, the gain (2^15 is 1) */
    int32_t attack, decay[SYNTH_CURVE], release[SYNTH_CURVE], fade[SYNTH_CURVE], gain;
    uint32_t ratio, depth; /* FM: the modulator's phase steps a carrier's, the phase a full modulator bends it */
    /* FM: the modulator's level (0 to 2^24), its step down a sample and where it stops; its last two samples and what
       their sum bends its own phase by */
    int32_t bend, bend_step, bend_floor, slope, fed[2];
    uint32_t feedback;
    /* the pitch: the step now, on its way to `step` by `slide` a sample; the vibrato's phase and step; samples since
       the note began */
    uint32_t at, lfo, lfo_step, age;
    /* the unison's second voice: its phase, its step and its modulator's last two samples */
    uint32_t phase2, step2;
    int32_t fed2[2];
    int32_t slide;
    int32_t f, damping, low, band; /* the filter's: f and damping 2^16 is 1, its state in 256ths of a sample */
    /* FM with a fine ratio: the ratio (2^16 is 1), the modulator's own phase and step, and the unison's */
    uint32_t mul, mphase, mstep, mphase2, mstep2;
    uint8_t more; /* a voice's: how many of its layers may be sounding */
    /* the cutoff's sweep: where it is (2^-20 octaves over 1 Hz), its step a sample and the samples it has left; the
       cutoff's own f */
    int32_t cut_at, cut_step, f_end;
    uint32_t cut_left;
} SynthOsc;

enum { SYNTH_COMBS = 4, SYNTH_PASSES = 2, SYNTH_COMB_MOST = 2100, SYNTH_PASS_MOST = 256 };

/* The room: Schroeder's reverb, four combs side by side, each fed back through a low-pass, then two all-passes. */
typedef struct SynthRoom {
    int32_t comb[SYNTH_COMBS][SYNTH_COMB_MOST], pass[SYNTH_PASSES][SYNTH_PASS_MOST];
    int32_t damp[SYNTH_COMBS];
    uint16_t comb_len[SYNTH_COMBS], pass_len[SYNTH_PASSES], comb_at[SYNTH_COMBS], pass_at[SYNTH_PASSES];
    uint32_t live; /* the samples its tail has left: 0 and it is empty */
} SynthRoom;

typedef struct Synth {
    uint32_t rate;
    uint32_t per_hertz, rest; /* 2^32 / rate, as quotient and remainder: a phase step without 64-bit division */
    /* the speaker: the tone wanted, written by any thread; the one sounding, its phase and its fade */
    uint32_t want, hertz, phase;
    int32_t gain;
    /* the clock: its call, the samples between two (2^16 is 1) and those left to the next */
    void (*tick)(void);
    uint32_t tick_step, tick_left;
    /* the limiter: what the mix is turned down by (2^24 is all of it), and what a sample gives back */
    int32_t held, release;
    SynthOsc osc[DEV_VOICES];
    SynthOsc layer[DEV_VOICES][DEV_LAYERS - 1]; /* each voice's layers after its own sound */
    int16_t sine[SYNTH_SINE];
    SynthRoom room;
} Synth;

void synth_init(Synth *s, uint32_t rate);
/* The speaker's tone, 0 silence: safe from another thread than synth_render's. */
void synth_tone(Synth *s, uint32_t hertz);
/* Voice `voice` set (dev_voice), and v->layers layers after v: not safe while synth_render runs. */
void synth_voice(Synth *s, unsigned voice, const DevVoice *v);
/* `tick` is called from synth_render, `sixteenths` 16ths of a hertz, by the count of its samples: a sequencer's
   clock with no timer, as exact as the samples. It may set voices. */
void synth_clock(Synth *s, void (*tick)(void), uint32_t sixteenths);
void synth_render(Synth *s, int16_t *out, int count);
/* The same in stereo, a frame its left sample then its right: each voice where its pan puts it. synth_render is
   every voice in the middle. */
void synth_render_stereo(Synth *s, int16_t *out, int frames);

#endif
