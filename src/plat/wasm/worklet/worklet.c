/* The synthesizer (plat/synth) for the page's AudioWorklet (web/synth.mjs), built alone to synth.wasm: a voice's
   DevVoice is written at worklet_init's address, then worklet_voice; worklet_render's frames, left then right, are at its address. */
#include "plat/synth/synth.h"

enum { QUANTUM = 128 }; /* an AudioWorklet's frames a call */

static_assert(sizeof(DevVoice) == 28, "wasm.mjs's VOICE_BYTES");

static Synth synth;
static DevVoice in;
static int16_t out[QUANTUM * 2]; /* a frame its left sample then its right */

[[clang::export_name("worklet_init")]] DevVoice *worklet_init(const uint32_t rate)
{
    synth_init(&synth, rate);
    return &in;
}

[[clang::export_name("worklet_voice")]] void worklet_voice(const uint32_t voice)
{
    synth_voice(&synth, voice, &in);
}

[[clang::export_name("worklet_render")]] int16_t *worklet_render(const int32_t count)
{
    synth_render_stereo(&synth, out, count < QUANTUM ? count : QUANTUM);
    return out;
}
