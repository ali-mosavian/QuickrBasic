/* The IDE in an SDL window.
 *
 * # How it works
 * The IDE runs on its own thread, as on wasm's worker: its keys come down a pipe (host_term_feed),
 * its mouse is set as the window's moves (host_mouse_set), its tones are audio.c's. The main
 * thread is SDL's: each frame it draws plat/view's frame of the screen. SIGINT and SIGALRM are the
 * IDE thread's, the STOP key and the tick, as on the host; Ctrl+C in the window is SIGINT too.
 * Started from Finder (in /), it works in ~/Documents/QuickrBasic, the demos copied there.
 * A test's, a time T a frame's number or seconds as Ns (1.5s):
 * - QB_KEYS: BIOS keys in hex, typed at KEYS_FRAMES, or at T as T:key; FFFF the STOP key.
 * - QB_MOUSE: T:X,Y[:B], the mouse at pixel X,Y at T, its buttons B (1 left, 2 right, 4 middle), 0
 *   none; a press is a B then a 0.
 * - QB_SHOT: file[@T], comma apart: the frame drawn at T to file as a PPM, then, the last taken,
 *   exit. No T: SHOT_FRAMES, or SHOT_AFTER past the last key.
 */
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    SHOT_AFTER = 30,
    KEY_STOP = 0xFFFF,
};

static void *run(void *const arg) { exit(qb_main()); }

static int keys_out = -1;

static void send(const uint16_t key) { (void)!write(keys_out, &key, sizeof key); }

/* A script's time at s, its frame; *end past it. */
static uint32_t when(const char *const s, char **const end)
{
    const double t = strtod(s, end);

    if (**end != 's')
        return (uint32_t)t;

    ++*end;
    return (uint32_t)(t * 1000 / FRAME_MS + 0.5);
}

/* QB_KEYS' keys due at frame now typed; the frame of its last. */
static uint32_t keys(const char *k, const uint32_t now)
{
    uint32_t last = KEYS_FRAMES;

    while (k && *k) {
        char *end;
        const size_t len = strcspn(k, " ,");
        const char *const colon = memchr(k, ':', len);
        const uint32_t at = colon ? when(k, &end) : KEYS_FRAMES;
        const unsigned long key = strtoul(colon ? colon + 1 : k, &end, 16);

        if (at == now && key == KEY_STOP)
            kill(getpid(), SIGINT);
        else if (at == now)
            send((uint16_t)key);

        last = at > last ? at : last;
        k += len;
        k += *k != 0;
    }

    return last;
}

/* The window's mouse: the cell and the pixel under it, its buttons as INT 33h's (left, right,
   middle). A text mode's pixel is the cell's times 8, a graphics mode's its own; a graphics mode's
   cell is 8 wide and as high as its height has it (8, 14 or 16). */
static void mouse_at(
    const ViewFrame *const f,
    const float fx,
    const float fy,
    const uint8_t buttons
)
{
    if (!f->width || !f->height)
        return;

    const uint32_t x = fx < 0 ? 0 : fx >= f->width ? f->width - 1 : (uint32_t)fx;
    const uint32_t y = fy < 0 ? 0 : fy >= f->height ? f->height - 1 : (uint32_t)fy;
    const uint32_t cell_w = f->cell_h ? VIEW_CELL_W : 8;
    const uint32_t cell_h = f->cell_h          ? f->cell_h
                            : f->height >= 480 ? 16
                            : f->height >= 350 ? 14
                                               : 8;
    const uint32_t row = y / cell_h, col = x / cell_w;

    host_mouse_set(
        (uint8_t)row,
        (uint8_t)col,
        (uint16_t)(f->cell_h ? col * 8 : x),
        (uint16_t)(f->cell_h ? row * 8 : y),
        buttons
    );
}

static void point(SDL_Renderer *const r, SDL_Event *const e, const ViewFrame *const f)
{
    SDL_ConvertEventToRenderCoordinates(r, e);
    const SDL_MouseButtonFlags b = SDL_GetMouseState(nullptr, nullptr);
    mouse_at(
        f,
        e->type == SDL_EVENT_MOUSE_MOTION ? e->motion.x : e->button.x,
        e->type == SDL_EVENT_MOUSE_MOTION ? e->motion.y : e->button.y,
        (uint8_t)((b & SDL_BUTTON_LMASK ? 1 : 0) | (b & SDL_BUTTON_RMASK ? 2 : 0) |
                  (b & SDL_BUTTON_MMASK ? 4 : 0))
    );
}

/* QB_MOUSE's moves due at frame now made. */
static void mouse(const char *m, const uint32_t now, const ViewFrame *const f)
{
    while (m && *m) {
        char *end;
        const uint32_t at = when(m, &end);
        const float x = strtof(end + (*end == ':'), &end);
        const float y = strtof(end + (*end == ','), &end);
        const unsigned long b = *end == ':' ? strtoul(end + 1, &end, 10) : 0;

        if (at == now)
            mouse_at(f, x, y, (uint8_t)b);

        m = end + strcspn(end, " ");
        m += strspn(m, " ");
    }
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
    static const char *const demos[] = {"WELCOME.BAS", "GORILLA.BAS", "NIBBLES.BAS", "MOTHLITE.BAS", "RUSTLINE.BAS"};

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

/* QB_SHOT's shots due at frame now taken, at dflt those with no time; true when none is left. */
static bool shots(const char *s, const uint32_t now, const uint32_t dflt, const ViewFrame *const f)
{
    bool done = true;

    while (s && *s) {
        const size_t len = strcspn(s, ",");
        const char *at = nullptr;

        for (size_t i = 0; i < len; i++)
            if (s[i] == '@')
                at = s + i;

        char *end, path[4096];
        const uint32_t due = at ? when(at + 1, &end) : dflt;

        if (due == now) {
            snprintf(path, sizeof path, "%.*s", (int)(at ? at - s : (ptrdiff_t)len), s);
            shot(path, f);
        }

        done = done && due <= now;
        s += len;
        s += *s != 0;
    }

    return done;
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

        const uint32_t last = keys(getenv("QB_KEYS"), ++frames);
        mouse(getenv("QB_MOUSE"), frames, f);

        if (shot_to &&
            shots(shot_to, frames, last > KEYS_FRAMES ? last + SHOT_AFTER : SHOT_FRAMES, f))
            exit(0);

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

        /* a graphics mode's program draws its own (PEN) */
        pointer.shown || !f->cell_h ? SDL_HideCursor() : SDL_ShowCursor();

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
