/* The QB device interface (interface.toml): the screen's text, the keyboard, the speaker and the clock,
   as runtime/qb uses them. A platform (dos/m16, dos/m32, headless, sdl) defines every function; the
   graphics screen's are in gfxdev.h. Portable code includes this and no platform's own header. */
#ifndef QB_DEV_H
#define QB_DEV_H

#include "platform.h"
#include "voice.h"

/* Whether standard output is the screen and not a file or a pipe. */
int dev_stdout_is_screen(void);

/* The text screen: cells of a character and an attribute from 0, 0 at the top left. */
unsigned dev_text_size(void);   /* rows in the high byte, columns in the low */
unsigned dev_text_cursor(void); /* row in the high byte */
void dev_text_move(unsigned row, unsigned column);
void dev_text_cursor_show(int visible);
unsigned dev_text_get(unsigned row, unsigned column); /* the attribute in the high byte */
void dev_text_put(unsigned row, unsigned column, unsigned character, unsigned attribute);
void dev_text_write(unsigned row, unsigned column, const char *text, unsigned count,
                    unsigned attribute);
/* The rows `top` to `bottom` move up `lines`, the rows freed are blanks of `attribute`; no lines, or
   as many as the rows, clears them. */
void dev_text_scroll(unsigned top, unsigned bottom, unsigned lines, unsigned attribute);
/* The text screen's pages, counted from the one shown at the last reset (the program's page 0): the one drawn on
   and the one shown, 0 past the pages the screen has; one copied over another. */
void dev_text_pages_reset(void);
int dev_text_pages(unsigned active, unsigned visible);
int dev_text_copy(unsigned from, unsigned to);

/* The keyboard. */
int dev_key_ready(void);
/* The next byte typed, waited for; an extended key is 0 and then its scan code. At the end of
   redirected input it is 26 (Ctrl-Z), again and again. */
unsigned char dev_key_read(void);
/* The scan code of the next key if it is an extended one (a function or cursor key), else -1; also
   -1 with no key waiting. The key stays. */
int dev_key_peek_scan(void);
/* The next key is read and dropped. */
void dev_key_discard(void);

/* The speaker: a tone of `hertz` until the next call; 0 is silence. */
void dev_tone(unsigned hertz);
/* The synthesizer's voice 0 to DEV_VOICES - 1 (PLAY a$, b$, ...) set to `v` until the next call for it; a platform
   with only the speaker plays voice 0 there. */
void dev_voice(unsigned voice, const DevVoice *v);
/* A note for the synthesizer is queued: a platform that starts it on demand does so now, outside the tick. Called
   for every such note. dev_synth_close stops it when the program ends. */
void dev_synth_open(void);
void dev_synth_close(void);
/* The platform's audio device made the host's (plat/paud.h), at the start; none on some. */
void dev_audio_init(void);

/* Memory for a buffer, taken when its feature is first used: bytes of it, zeroed; Out of memory
   when there is none. Given back (null: nothing). Not from an interrupt. */
void *dev_take(unsigned long bytes);
void dev_give(void *block);
/* The same, null when there is none: from any thread (the host's tick). */
void *dev_try(unsigned long bytes);

/* The clock. */
long dev_clock(void); /* the time of day in hundredths of a second since midnight */
long dev_clock_fine(void); /* the same in tenths of a millisecond */
void dev_date(unsigned *year, unsigned *month, unsigned *day);
void dev_time(unsigned *hour, unsigned *minute, unsigned *second);
/* Set the clock; 0 if it took the date or time, else the clock refused it (an impossible one). */
int dev_date_set(unsigned year, unsigned month, unsigned day);
int dev_time_set(unsigned hour, unsigned minute, unsigned second);
/* From now on the clock's tick (18.2 a second) calls music_tick (play.c); undone by dev_tick_stop.
   Called once. */
void dev_tick_start(void);
void dev_tick_stop(void);
/* The tick does not call music_tick between these two. */
void dev_tick_hold(void);
void dev_tick_release(void);

/* Whether the printer is there and ready. */
int dev_printer_ready(void);

/* The platform's own spelling of the above (symbol names, macros); an empty file where there is none. */
#include "devsym.h"

#endif
