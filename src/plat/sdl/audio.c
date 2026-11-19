/* The speaker and the synthesizer's voices (plat/synth), rendered in SDL's audio callback; with no audio device the
 * program runs silent. The runtime's tones and voices come here by wrap.h, and go on to its own device. */
#include "sdl.h"
#include "plat/synth/synth.h"

enum { RATE = 44100, CHUNK = 1024 };

static Synth synth = {.rate = RATE};
static SDL_AudioStream *stream;

void __real_dev_tone(unsigned hertz);
void __real_dev_voice(unsigned voice, const DevVoice *v);
int host_tick_fine_fire(void); /* the runtime's (dev/host/tick.h) */

enum { FINE_RATE = 4661 }; /* music.h's MUSIC_FINE_RATE */

static void tick(void)
{
    host_tick_fine_fire();
}

void __wrap_dev_tone(const unsigned hertz)
{
    synth_tone(&synth, hertz);
    __real_dev_tone(hertz);
}

/* the callback runs with the stream locked */
void __wrap_dev_voice(const unsigned voice, const DevVoice *const v)
{
    if (stream)
        SDL_LockAudioStream(stream);
    synth_voice(&synth, voice, v);
    if (stream)
        SDL_UnlockAudioStream(stream);
    __real_dev_voice(voice, v);
}

static void fill(void *const user, SDL_AudioStream *const s, const int more, const int total)
{
    int16_t buf[CHUNK * 2]; /* a frame its left sample then its right */

    for (int n = more / (int)(2 * sizeof buf[0]); n > 0;) {
        const int chunk = n < CHUNK ? n : CHUNK;

        synth_render_stereo(&synth, buf, chunk);
        SDL_PutAudioStreamData(s, buf, chunk * (int)(2 * sizeof buf[0]));
        n -= chunk;
    }
}

void sdl_audio_open(void)
{
    const SDL_AudioSpec spec = {.format = SDL_AUDIO_S16, .channels = 2, .freq = RATE};

    synth_init(&synth, RATE);
    synth_clock(&synth, tick, FINE_RATE);
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, fill, nullptr);
    if (stream)
        SDL_ResumeAudioStreamDevice(stream);
}
