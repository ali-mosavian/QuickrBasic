/* The synthesizer: the speaker's square wave, as QB's, and three voices after the Commodore SID's (a pulse, triangle,
   saw or noise wave each, its ADSR envelope and its own resonant filter), mixed to 16-bit samples. Portable and
   self-contained: the Mac's audio callback (plat/sdl/audio.c), the page's AudioWorklet (built alone to synth.wasm) and
   the tests render the same samples. */
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

#include "qbrt/llrm/qb/dev/voice.h"

typedef struct SynthOsc {
    DevVoice set;
    uint32_t phase, step; /* a period is 2^32 */
    uint32_t noise;       /* SID's 23-bit shift register */
    int32_t level;        /* the envelope, 0 to SYNTH_FULL */
    uint8_t stage;
    float f, damping, low, band; /* the filter's */
} SynthOsc;

typedef struct Synth {
    uint32_t rate;
    /* the speaker: the tone wanted, written by any thread; the one sounding, its phase and its fade */
    uint32_t want, hertz, phase;
    int32_t gain;
    SynthOsc osc[DEV_VOICES];
} Synth;

void synth_init(Synth *s, uint32_t rate);
/* The speaker's tone, 0 silence: safe from another thread than synth_render's. */
void synth_tone(Synth *s, uint32_t hertz);
/* Voice `voice` set (dev_voice): not safe while synth_render runs. */
void synth_voice(Synth *s, unsigned voice, const DevVoice *v);
void synth_render(Synth *s, int16_t *out, int count);

#endif
