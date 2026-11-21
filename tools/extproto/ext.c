/* A no-import extension: data with pointers (relocations), a call back into the host, a render. */
#include "qx.h"
#define EXPORT __attribute__((visibility("default")))

static const QxHost *host;
static int16_t *level; /* the host's memory, kept: render reads it */
static uint32_t phase;

static void QX_CALL add(QxCall *c) { c->result = c->arg[0] + c->arg[1]; }

static void QX_CALL hello(QxCall *c)
{
    static const char text[] = "hello from the extension";
    c->host->say(text, sizeof text - 1);
}

static void QX_CALL loud(QxCall *c) { *level = (int16_t)c->arg[0]; }

static void QX_CALL render(int16_t *out, int32_t frames)
{
    for (int32_t i = 0; i < frames; i++)
        out[i] = (int16_t)((phase++ & 1) ? *level : -*level);
}

static const QxEntry entries[] = {{"add", "II", add}, {"hello", "", hello}, {"loud", "I", loud}};
static const QxModule module = {sizeof module, 1, 3, entries, render};

EXPORT const QxModule *QX_CALL qx_init_ext(const QxHost *h)
{
    host = h;
    level = h->alloc(sizeof *level);
    *level = 0;
    return &module;
}
