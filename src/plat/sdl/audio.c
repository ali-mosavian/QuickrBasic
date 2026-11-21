/* The host's audio device on the Mac (plat/paud.h): SDL's stream, each stream open mixed in its callback; the speaker
 * and the synthesizer's voices (plat/synth) one of them. With no audio device the program runs silent. The runtime's
 * tones and voices come here by wrap.h, and go on to its own device. */
#include "sdl.h"
#include "plat/paud.h"
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
    const uint32_t token = paud_lock();
    synth_voice(&synth, voice, v);
    paud_unlock(token);
    __real_dev_voice(voice, v);
}

static bool device_start(void) { return true; }

static void device_stop(void) {}

static uint32_t device_lock(void)
{
    SDL_LockAudioStream(stream);
    return 0;
}

static void device_unlock(const uint32_t token) { SDL_UnlockAudioStream(stream); }

static const PaudDevice device = {
    .rate = RATE,
    .stereo = true,
    .start = device_start,
    .stop = device_stop,
    .lock = device_lock,
    .unlock = device_unlock,
};

static void QX_CALL synth_stream(int16_t *const out, const int32_t frames)
{
    synth_render_stereo(&synth, out, frames);
}

static void fill(void *const user, SDL_AudioStream *const s, const int more, const int total)
{
    int16_t buf[CHUNK * 2]; /* a frame its left sample then its right */

    for (int n = more / (int)(2 * sizeof buf[0]); n > 0;) {
        const int chunk = n < CHUNK ? n : CHUNK;

        paud_mix(buf, chunk);
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

    if (!stream)
        return;

    uint32_t rate;
    bool stereo;
    paud_device(&device);
    paud_open(synth_stream, &rate, &stereo);
    SDL_ResumeAudioStreamDevice(stream);
}
