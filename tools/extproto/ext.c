/* The prototype extension: plain C functions, its glue (ext_qx.c) generated from ext.bas. Data
   with pointers (relocations), a call back into the host, a render on another thread. */
#include "ext_qx.h"

static int16_t *level; /* in the host's memory: render reads it */
static uint32_t phase;
static char shouted[64];

static void QX_CALL render(int16_t *const out, const int32_t frames)
{
    for (int32_t i = 0; i < frames; i++)
        out[i] = (int16_t)((phase++ & 1) ? *level : -*level);
}

static QxAudio audio = {.rate = 48000, .render = render};

void ext_init(void)
{
    level = qx_host->alloc(sizeof *level);
    *level = 0;
    qx_host->audio_open(&audio);
}

void ext_start(void) {}

void ext_stop(void) {}

int32_t ext_add(const int32_t a, const int32_t b) { return a + b; }

void ext_hello(void)
{
    static const char text[] = "hello from the extension";
    qx_host->say(text, sizeof text - 1);
}

void ext_loud(const int32_t n, ext_level *const l)
{
    *level = (int16_t)n;
    l->now = (int16_t)n;

    if (n > l->peak)
        l->peak = (int16_t)n;
}

void ext_bump(int32_t *const n) { ++*n; }

QxStr shout_text(const QxStr s)
{
    if (s.len > sizeof shouted) {
        qx_fail(5); /* Illegal function call */
        return (QxStr){};
    }

    for (uint32_t i = 0; i < s.len; i++)
        shouted[i] = s.ptr[i] >= 'a' && s.ptr[i] <= 'z' ? (char)(s.ptr[i] - 32) : s.ptr[i];

    return (QxStr){shouted, s.len};
}
