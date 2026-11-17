/* The synthesizer (synth.h). Integer oscillators and envelopes, so every build renders the same samples; the filter
   is a Chamberlin state-variable one, run twice a sample so it stays stable up to its highest cutoff. */
#include "plat/synth/synth.h"

/* no fused multiply-adds: the page's wasm has none, and the Mac's samples are the page's */
#pragma STDC FP_CONTRACT OFF

enum {
    /* the speaker: QB's square wave, faded in and out over 2 ms so it does not click */
    SPEAKER_LOUD = 4000,
    FADES_A_SECOND = 500,
    /* a voice at volume 15 */
    VOICE_LOUD = 6000,
    HIGHEST_NIBBLE = 15,
    /* SID's exponential decay: below each level the step is divided by the next divisor */
    CURVE_STEPS = 5,
};

#define SYNTH_FULL (1 << 24)
#define PI 3.14159265f

enum Stage { STAGE_ATTACK, STAGE_DECAY, STAGE_RELEASE, STAGE_CUT };

/* SID's times from silence to full (attack) and from full to silence (decay, release), ms */
static const uint16_t attack_ms[16] = {2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000};
static const uint16_t decay_ms[16] = {6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000};
static const struct {
    int32_t below;
    int32_t divisor;
} curve[CURVE_STEPS] = {{93, 2}, {54, 4}, {26, 8}, {14, 16}, {6, 30}};

void synth_init(Synth *const s, const uint32_t rate)
{
    *s = (Synth){.rate = rate};
    for (int i = 0; i < DEV_VOICES; i++) {
        s->osc[i].noise = 0x7FFFF8;
        s->osc[i].stage = STAGE_CUT;
    }
}

void synth_tone(Synth *const s, const uint32_t hertz)
{
    __atomic_store_n(&s->want, hertz, __ATOMIC_RELAXED);
}

/* sin(x) for 0 <= x <= pi / 4 */
static float small_sin(const float x)
{
    const float x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42)));
}

void synth_voice(Synth *const s, const unsigned voice, const DevVoice *const v)
{
    if (voice >= DEV_VOICES)
        return;
    SynthOsc *const o = &s->osc[voice];
    const bool attack = v->gate && (!o->set.gate || v->trigger != o->set.trigger);

    o->set = *v;
    o->step = (uint32_t)(((uint64_t)v->hertz << 32) / s->rate);
    if (!v->hertz)
        o->stage = STAGE_CUT;
    else if (attack)
        o->stage = STAGE_ATTACK;
    else if (!v->gate)
        o->stage = STAGE_RELEASE;

    /* twice a sample: up to a sixth of the rate */
    const float highest = (float)s->rate / 6;
    const float cutoff = v->cutoff < highest ? v->cutoff : highest;
    o->f = 2 * small_sin(PI * cutoff / (2 * (float)s->rate));
    o->damping = 1.4f - (float)v->resonance * (1.3f / HIGHEST_NIBBLE);
}

/* The speaker's next sample, as QB's square wave always was. */
static int32_t speaker(Synth *const s)
{
    const uint32_t want = __atomic_load_n(&s->want, __ATOMIC_RELAXED);
    const int32_t fade = (int32_t)(s->rate / FADES_A_SECOND);

    if (want != s->hertz && s->gain == 0) { /* a new tone starts from silence */
        s->hertz = want;
        s->phase = 0;
    }
    s->gain += want == s->hertz && s->hertz ? s->gain < fade : -(s->gain > 0);
    s->phase = (s->phase + s->hertz) % s->rate;
    return (s->phase < s->rate / 2 ? SPEAKER_LOUD : -SPEAKER_LOUD) * s->gain / fade;
}

/* The step a sample of `ms` from silence to full takes. */
static int32_t step_of(const Synth *const s, const uint32_t ms)
{
    const int32_t samples = (int32_t)((uint64_t)ms * s->rate / 1000);
    return SYNTH_FULL / (samples > 0 ? samples : 1);
}

/* A falling envelope's step at its level: SID's, smaller as it falls. */
static int32_t falling(const SynthOsc *const o, int32_t step)
{
    const int32_t level = o->level >> 16; /* 0 to 255, as SID's */

    for (int i = CURVE_STEPS - 1; i >= 0; i--)
        if (level < curve[i].below)
            return step / curve[i].divisor > 0 ? step / curve[i].divisor : 1;
    return step;
}

static void envelope(const Synth *const s, SynthOsc *const o)
{
    const unsigned env = o->set.envelope;
    const int32_t sustain = (int32_t)(env >> 4 & HIGHEST_NIBBLE) * (SYNTH_FULL / HIGHEST_NIBBLE);

    switch (o->stage) {
    case STAGE_ATTACK:
        o->level += step_of(s, attack_ms[env >> 12 & HIGHEST_NIBBLE]);
        if (o->level >= SYNTH_FULL) {
            o->level = SYNTH_FULL;
            o->stage = STAGE_DECAY;
        }
        break;
    case STAGE_DECAY:
        if (o->level > sustain) {
            o->level -= falling(o, step_of(s, decay_ms[env >> 8 & HIGHEST_NIBBLE]));
            if (o->level < sustain)
                o->level = sustain;
        }
        break;
    case STAGE_RELEASE:
        o->level -= falling(o, step_of(s, decay_ms[env & HIGHEST_NIBBLE]));
        break;
    default:
        o->level -= SYNTH_FULL / (int32_t)(s->rate / FADES_A_SECOND);
    }
    if (o->level < 0)
        o->level = 0;
}

/* The wave's next sample, -32767 to 32767. */
static int32_t wave(SynthOsc *const o)
{
    const uint32_t was = o->phase;

    o->phase += o->step;
    switch (o->set.wave) {
    case DEV_TRIANGLE: {
        const int32_t x = (int32_t)(o->phase >> 16);
        return (x < 32768 ? x * 2 : (65535 - x) * 2) - 32767;
    }
    case DEV_SAW:
        return (int32_t)(o->phase >> 16) - 32768;
    case DEV_NOISE:
        /* the register steps 16 times a period, as SID's on bit 19 of its 24 */
        if (~was & o->phase & 1u << 27) {
            const uint32_t bit = (o->noise >> 22 ^ o->noise >> 17) & 1;
            o->noise = (o->noise << 1 | bit) & 0x7FFFFF;
        }
        return (int32_t)(o->noise >> 15 & 0xFF) * 257 - 32767;
    default:
        return o->phase >> 20 < o->set.width ? 32767 : -32767;
    }
}

static float filtered(SynthOsc *const o, const float in)
{
    float high = 0;

    for (int i = 0; i < 2; i++) {
        o->low += o->f * o->band;
        high = in - o->low - o->damping * o->band;
        o->band += o->f * high;
    }
    switch (o->set.filter) {
    case DEV_LOW_PASS:
        return o->low;
    case DEV_BAND_PASS:
        return o->band;
    default:
        return high;
    }
}

static int32_t voice(const Synth *const s, SynthOsc *const o)
{
    if (o->stage == STAGE_CUT && o->level == 0)
        return 0;
    envelope(s, o);
    const int32_t raw = wave(o);
    int32_t out = (int32_t)((int64_t)raw * (o->level >> 9) >> 15);
    if (o->set.filter != DEV_UNFILTERED)
        out = (int32_t)filtered(o, (float)out);
    return (int32_t)((int64_t)out * o->set.volume * VOICE_LOUD / (HIGHEST_NIBBLE * 32767));
}

void synth_render(Synth *const s, int16_t *const out, const int count)
{
    for (int i = 0; i < count; i++) {
        int32_t sum = speaker(s);

        for (int v = 0; v < DEV_VOICES; v++)
            sum += voice(s, &s->osc[v]);
        out[i] = (int16_t)(sum > 32767 ? 32767 : sum < -32767 ? -32767 : sum);
    }
}
