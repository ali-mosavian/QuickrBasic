/* The IDE in an SDL window.
 *
 * # How it works
 * The IDE runs on its own thread, as on wasm's worker: its keys come down a pipe (host_term_feed),
 * its mouse is set as the window's moves (host_mouse_set), its tones are audio.c's. The main
 * thread is SDL's: each frame it draws plat/view's frame of the screen. SIGINT and SIGALRM are the
 * IDE thread's, the STOP key and the tick, as on the host; Ctrl+C in the window is SIGINT too.
 * Started from Finder (in /), it works in ~/Documents/QuickrBasic, the demos copied there.
 * A test's: QB_KEYS, BIOS keys in hex, typed at KEYS_FRAMES; QB_SHOT=file, the frame drawn at
 * SHOT_FRAMES to file as a PPM, then exit.
 */
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "plat/host/host.h"
#include "plat/pmou.h"
#include "plat/view/view.h"
#include "sdl.h"

int qb_main(void);

/* BLINK_MS: the VGA's blink, a phase each 16 frames of 60 Hz; ALT_TAP_MS: an Alt released is held
   that long, so the editor's poll of the shift flags sees a tap. */
enum {
    STACK = 64 << 20,
    FRAME_MS = 16,
    BLINK_MS = 267,
    ALT_TAP_MS = 60,
    START_W = 1080,
    START_H = 600,
    KEYS_FRAMES = 60,
    SHOT_FRAMES = 90,
};

static void *run(void *const arg) { exit(qb_main()); }

static int keys_out = -1;

static void send(const uint16_t key) { (void)!write(keys_out, &key, sizeof key); }

/* The window's mouse: the cell under it, its buttons as INT 33h's (left, right, middle). */
static void point(SDL_Renderer *const r, SDL_Event *const e, const ViewFrame *const f)
{
    if (!f->cell_h)
        return;

    SDL_ConvertEventToRenderCoordinates(r, e);
    const SDL_MouseButtonFlags b = SDL_GetMouseState(nullptr, nullptr);
    const float x = e->type == SDL_EVENT_MOUSE_MOTION ? e->motion.x : e->button.x;
    const float y = e->type == SDL_EVENT_MOUSE_MOTION ? e->motion.y : e->button.y;
    const uint32_t rows = f->height / f->cell_h, cols = f->width / VIEW_CELL_W;
    const uint32_t row = y < 0 ? 0 : (uint32_t)y / f->cell_h,
                   col = x < 0 ? 0 : (uint32_t)x / VIEW_CELL_W;
    host_mouse_set(
        (uint8_t)(row < rows ? row : rows - 1),
        (uint8_t)(col < cols ? col : cols - 1),
        (uint8_t)((b & SDL_BUTTON_LMASK ? 1 : 0) | (b & SDL_BUTTON_RMASK ? 2 : 0) |
                  (b & SDL_BUTTON_MMASK ? 4 : 0))
    );
}

/* Finder's start, in /: ~/Documents/QuickrBasic, the app's demos copied in where missing. */
static void home(void)
{
    char at[4096], to[4096];
    const char *const user = getenv("HOME");

    if (!user || !getcwd(at, sizeof at) || at[0] != '/' || at[1])
        return;

    snprintf(to, sizeof to, "%s/Documents/QuickrBasic", user);
    mkdir(to, 0755);

    if (chdir(to))
        return;

    const char *const base = SDL_GetBasePath(); /* the bundle's Resources */
    static const char *const demos[] = {"WELCOME.BAS", "GORILLA.BAS", "NIBBLES.BAS"};

    for (uint32_t i = 0; base && i < sizeof demos / sizeof demos[0]; i++) {
        struct stat st;

        if (!stat(demos[i], &st))
            continue;

        snprintf(at, sizeof at, "%sdemos/%s", base, demos[i]);
        FILE *const in = fopen(at, "rb");
        FILE *const out = in ? fopen(demos[i], "wb") : nullptr;
        char buf[8192];
        size_t n;

        while (out && (n = fread(buf, 1, sizeof buf, in)) > 0)
            fwrite(buf, 1, n, out);

        if (out)
            fclose(out);

        if (in)
            fclose(in);
    }
}

/* The frame to path as a PPM. */
static void shot(const char *const path, const ViewFrame *const f)
{
    FILE *const out = fopen(path, "wb");

    if (!out)
        return;

    fprintf(out, "P6 %u %u 255\n", f->width, f->height);

    for (uint32_t i = 0; i < f->width * f->height; i++) {
        const uint32_t p = f->rgba[i];
        const uint8_t rgb[3] = {(uint8_t)p, (uint8_t)(p >> 8), (uint8_t)(p >> 16)};

        fwrite(rgb, 1, 3, out);
    }

    fclose(out);
}

int sdl_main(void)
{
    home();
    const char *const shot_to = getenv("QB_SHOT");
    uint32_t frames = 0;
    int fds[2];

    if (pipe(fds))
        return 1;

    keys_out = fds[1];
    host_term_feed(fds[0]);

    host_mouse_on();

    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, STACK);
    pthread_t ide;

    if (pthread_create(&ide, &a, run, nullptr))
        return 1;

    sigset_t theirs;
    sigemptyset(&theirs);
    sigaddset(&theirs, SIGINT);
    sigaddset(&theirs, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &theirs, nullptr);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        return 1;

    SDL_Window *const w = SDL_CreateWindow(
        "QuickrBasic",
        START_W,
        START_H,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    SDL_Renderer *const r = SDL_CreateRenderer(w, nullptr);
    SDL_Texture *const t = SDL_CreateTexture(
        r,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        VIEW_MAX_W,
        VIEW_MAX_H
    );
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_PIXELART);
    SDL_SetRenderVSync(r, 1);
    sdl_audio_open();

    const ViewFrame *f = view_frame(true, (ViewPointer){});

    uint32_t shown_w = 0, shown_h = 0;
    uint64_t alt_up = 0; /* when a released Alt's flags are due, 0 none */

    for (uint64_t next = SDL_GetTicks();; next += FRAME_MS) {
        SDL_Event e;
        const uint64_t now = SDL_GetTicks();

        if (now < next) /* vsync's pace where there is none: an occluded window, no display */
            SDL_Delay((uint32_t)(next - now));
        else
            next = now;

        while (SDL_PollEvent(&e))
            switch (e.type) {
            case SDL_EVENT_QUIT:
                exit(0);

            case SDL_EVENT_KEY_DOWN: {
                host_term_set_shift(sdl_shift(e.key.mod));

                if (e.key.mod & SDL_KMOD_GUI)
                    break;

                if (e.key.mod & SDL_KMOD_CTRL &&
                    (e.key.scancode == SDL_SCANCODE_C || e.key.scancode == SDL_SCANCODE_PAUSE))
                    kill(getpid(), SIGINT);

                const int32_t key = sdl_bios_key(&e.key);

                if (key >= 0)
                    send((uint16_t)key);

                break;
            }

            case SDL_EVENT_KEY_UP:
                if (e.key.scancode == SDL_SCANCODE_LALT || e.key.scancode == SDL_SCANCODE_RALT)
                    alt_up = SDL_GetTicks() + ALT_TAP_MS;
                else
                    host_term_set_shift(sdl_shift(e.key.mod));

                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                host_term_set_shift(0);
                break;

            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                point(r, &e, f);

                break;

            default:
                break;
            }

        if (alt_up && SDL_GetTicks() >= alt_up) {
            alt_up = 0;
            host_term_set_shift(sdl_shift(SDL_GetModState()));
        }

        const bool blink = SDL_GetTicks() / BLINK_MS % 2 == 0;
        const PmouState m = pmou_state();
        const ViewPointer pointer = {.shown = host_mouse_shown(), .row = m.row, .col = m.col};

        f = view_frame(blink, pointer);

        if (++frames == KEYS_FRAMES && getenv("QB_KEYS"))
            for (const char *k = getenv("QB_KEYS"); *k;) {
                char *end;
                send((uint16_t)strtoul(k, &end, 16));
                k = *end ? end + 1 : end;
            }

        if (shot_to && frames == SHOT_FRAMES) {
            shot(shot_to, f);
            exit(0);
        }

        if (f->width != shown_w || f->height != shown_h) {
            shown_w = f->width;
            shown_h = f->height;
            SDL_SetRenderLogicalPresentation(
                r,
                (int)shown_w,
                (int)shown_h,
                SDL_LOGICAL_PRESENTATION_LETTERBOX
            );
        }

        pointer.shown ? SDL_HideCursor() : SDL_ShowCursor();

        SDL_UpdateTexture(
            t,
            &(SDL_Rect){0, 0, (int)shown_w, (int)shown_h},
            f->rgba,
            (int)shown_w * 4
        );

        SDL_RenderClear(r);
        SDL_RenderTexture(r, t, &(SDL_FRect){0, 0, (float)shown_w, (float)shown_h}, nullptr);

        SDL_RenderPresent(r);
    }
}
