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
uint8_t sdl_shift(SDL_Keymod mod);

/* audio.c: the speaker, a square wave of the program's tone. */
void sdl_audio_open(void);

#endif
