/* Songs (song.c): instruments, a song of timed notes played on the music's tick, and notes struck at once. Each note
   takes a voice of its own, so notes that overlap do not cut each other. */
#ifndef QB_SONG_H
#define QB_SONG_H

#include "qb.h"

enum {
    SONG_BEAT = 96,          /* a quarter's ticks: 32nds and triplets are whole */
    SONG_INSTRUMENTS = 64,
    SONG_NOTES = 4096,
};

/* instrument n, commands$: sound n set by PLAY's commands. */
void song_instrument(int n, SD *commands);
/* music events(), from: `count` records of `size` bytes from `events`, each starting with five 32-bit integers: at,
   length, sound, pitch, loud. Played from now; at its end again from the tick `from`, not when negative. */
void song_play(const unsigned char *events, unsigned count, unsigned size, long from);
/* music stop[, ticks]: the song's notes let go now, or after a fade of `ticks`. */
void song_stop(long ticks);
/* note n, pitch, length, loud: sound n struck at the next part of a tick. */
void song_note(int sound, int pitch, long length, int loud);
/* beat(): quarters since the song began, 0 with none. */
float song_beat(void);

/* The music's (music.c): a part of a tick has passed; PLAY took `voice`; everything stops. */
void song_tick(void);
void song_taken(unsigned voice);
void song_clear(void);

#endif
