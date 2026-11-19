# PLAY: Writing a Song
keys: play song, writing music, arranging, song

How to build a piece of several voices that stays in time and plays
while the program runs.

See also [PLAY voices](play voices), [examples](play examples),
[PLAY function](PLAY function)

## Details
Count in bars
A bar of 4/4 is a whole note: sixteen sixteenths. Write each voice's
bar so its lengths add up to exactly that. Voices whose bars are all
as long never drift apart. To hold a note over the bar line, use ML
and repeat the note: the two sound as one.

Give each voice a job
    1      the melody: what PLAY(0) counts and a PC speaker plays
    2      a second melody or harmony
    3-5    a chord, a note each
    6      the bass
    7      kick, snare and toms
    8      hats
    9      sound effects: PLAY , , , , , , , , fx$

Keep the sound apart from the notes
Put each instrument (tempo, W, Z, H, V ...) in a string of its own and
add it in front of each bar's notes.

Feed it a bar at a time
A voice queues 18 notes, so a song does not fit in one PLAY. Play in
the background (MB) and queue the next bar when PLAY(0) is small.
PLAY(0) counts voice 1's waiting notes; rests count.

Mix
The voices are added, and a sum over full scale is turned down. If
the music pumps on the kick, lower the volumes: chords 6 to 8 a voice,
lead and bass 10 to 12, drums 10 to 14.

## Example
    DIM lead$(1), bass$(1)
    brass$ = "T126 W4 Z1,9,3,11,4 H0,4,12,1 U8 R4 V11 L8 "
    deep$ = "T126 W4 Z1,12,4,4 H0,5,5,1 V11 L8 "
    lead$(0) = "O4 E G > C4 < G E G4"
    lead$(1) = "O4 F A > C4 < A F A4"
    bass$(0) = "O2 C C > C < C C C > C < C"
    bass$(1) = "O1 F F > F < F F F > F < F"
    bar = 0
    DO WHILE bar < 8
      IF PLAY(0) < 3 THEN
        PLAY "MB" + brass$ + lead$(bar MOD 2), deep$ + bass$(bar MOD 2)
        bar = bar + 1
      END IF
    LOOP
    PLAY "MF P1"
    ' eight bars, queued one at a time while the loop runs
