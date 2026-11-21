/* The Mac's and Linux's host: dlopen the slice, then drive it. */
#include "drive.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DriveOut out;

static void *alloc(const uint32_t n) { return calloc(1, n); }

static void result_str(QxCall *const c, const char *const p, const uint32_t len)
{
    (void)c;
    snprintf(out.text, sizeof out.text, "%.*s", (int)len, p);
}

static bool audio_open(QxAudio *const a)
{
    out.render = a->render;
    return true;
}

static void say(const char *const p, const uint32_t len) { printf("say: %.*s\n", (int)len, p); }

static const QxHost host = {sizeof host, QX_ABI, alloc, result_str, audio_open, say};

int main(const int argc, char **const argv)
{
    void *const lib = argc > 1 ? dlopen(argv[1], RTLD_NOW | RTLD_LOCAL) : nullptr;

    if (!lib) {
        printf("dlopen: %s\n", dlerror());
        return 1;
    }

    const QxModule *(*const init)(const QxHost *) = (const QxModule *(*)(const QxHost *))dlsym(lib, "qx_init_ext");
    const int failed = drive(init(&host), &host, &out);
    int16_t samples[4];
    out.render(samples, 4);
    printf("drive: %s; render: %d %d %d %d\n", failed ? "failed" : "all as expected", samples[0], samples[1],
           samples[2], samples[3]);
    return failed;
}
