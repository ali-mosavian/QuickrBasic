/* qxtest: the native ABI's test extension, plain C functions glued to qxtest.bas. */
#include "qxtest_qx.h"

static char shouted[256];
static int32_t started;
static int32_t channels;
static volatile int32_t frames; /* rendered, where the device plays */

void qxtest_init(void) {}

void qxtest_start(void) { started++; }

void qxtest_stop(void) {}

int32_t add(const int32_t a, const int32_t b) { return a + b; }

double half(const double d) { return d / 2; }

QxStr shout_text(const QxStr s)
{
    const uint32_t n = s.len < sizeof shouted ? s.len : sizeof shouted;

    for (uint32_t i = 0; i < n; i++)
        shouted[i] = s.ptr[i] >= 'a' && s.ptr[i] <= 'z' ? (char)(s.ptr[i] - 32) : s.ptr[i];

    return (QxStr){shouted, n};
}

void bump(int32_t *const n) { ++*n; }

void move(qxtest_pt *const p, const int32_t dx) { p->x = (int16_t)(p->x + dx); }

float total(const QxArray *const a)
{
    const float *const f = a->data;
    float sum = 0;

    for (uint32_t i = 0; i < a->count; i++)
        sum += f[i];

    return sum;
}

int32_t items(const QxArray *const a) { return (int32_t)a->count; }

void fail(const int32_t code) { qx_fail((uint8_t)code); }

int32_t runs(void) { return started; }

/* Silence, its frames counted. */
static void QX_CALL render(int16_t *const out, const int32_t n)
{
    for (int32_t i = 0; i < n * channels; i++)
        out[i] = 0;

    frames += n;
}

int32_t audio(void)
{
    QxAudio a = {.rate = 22050, .render = render};

    if (!qx_host->audio_open(&a))
        return 0;

    channels = a.stereo ? 2 : 1;
    return -1;
}

int32_t played(void)
{
    const uint32_t token = qx_host->audio_lock();
    const int32_t n = frames;
    qx_host->audio_unlock(token);
    return n;
}
