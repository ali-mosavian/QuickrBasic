/* The IDE in an SDL window (window.c): the program runs on its own thread, SDL on the main one. The
 * main thread draws plat/view's frame, and sends the keys down the keyboard's pipe (host.h). */
#ifndef PLAT_SDL_H
#define PLAT_SDL_H
#include <SDL3/SDL.h>
#include "base.h"

/* The window's main. */
int sdl_main(void);

/* keys.c: an event's BIOS key (scan code << 8 | character), -1 none; the shift flags (PKEY_*). */
int32_t sdl_bios_key(const SDL_KeyboardEvent *e);
/* The key at s as the keyboard's scan code, 0 none. */
uint8_t sdl_scan(SDL_Scancode s);
uint8_t sdl_shift(SDL_Keymod mod);

/* audio.c: the speaker and the synthesizer's voices (plat/synth). */
void sdl_audio_open(void);

/* player.c: gfx's lists played on the renderer's GPU device, set as pgfx's player: false, none. */
bool sdl_player_init(SDL_GPUDevice *dev);
/* The gfx canvas shown is w by h: false, none. */
bool sdl_player_size(uint32_t *w, uint32_t *h);
/* The gfx canvas shown drawn by r, filling its logical size. On the window's thread. */
void sdl_player_draw(SDL_Renderer *r);
/* The gfx canvas shown, w by h, bytes R, G, B and A in malloc's memory: false, none. */
bool sdl_player_pixels(uint8_t **rgba, uint32_t *w, uint32_t *h);

#endif
