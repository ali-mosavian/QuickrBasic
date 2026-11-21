# PLAY: Voices
keys: play voices, voices, synthesizer, SID

Syntax
    PLAY string1$ [, string2$ [, ... string32$]]

Plays up to 32 voices at once, one string for each, on a built-in
synthesizer. A program that never uses the extra commands sounds as in
QBasic: one square wave.

See also [PLAY](PLAY), [PLAY string](PLAY string),
[writing a song](play song), [SOUND](SOUND)

## Details
Voices
 - Strings go to voices 1, 2, 3 ... in order.
 - A string left out leaves its voice playing whatever it was:
   PLAY , , fx$ plays voice 3 only, and the music on 1 and 2 goes on.
 - Each voice keeps its own octave, length, tempo, style and sound.
   Set the tempo in every string. MF and MB apply to all voices.
 - Each voice queues 18 notes. PLAY(n) counts those waiting in
   voice 1; a rest counts.
 - A new program starts with every voice at its defaults.

Sound commands
Allowed in any PLAY string. Using one, or a second string, sends the
voice through the synthesizer. Each sets the notes that follow it.
Each number may be =var; as usual.

    V n            volume 0-15 (default 8)
    W n            wave 0-4
    Q n            pulse width 1-99 percent (default 50)
    H a,d,s,r,f    envelope, each 0-15 (default 0,0,15,0)
    Y m,f,r,s,t    filter
    Z r,d,f,l,b,s  FM's modulator
    U n            vibrato 0-15
    K t,q,u,p      glide
    S n            stereo place 1-15 (default 8, the middle)
    R n            reverb 0-15
    I n            unison 0-15
    J n,p          layer 0-3, its pitch in hundredths of the note's

See [waves](play waves), [envelopes](play envelope),
[filters](play filter), [FM](play fm), [vibrato and glide](play pitch),
[stereo, reverb and unison](play space), [layers](play layers).
Recipes: [examples](play examples).

Timing
A synthesizer voice's notes are timed to 3.4 ms, a 16th of a clock
tick. Voices stay together when their strings add up to the same time.
Under ML a note followed by one of the same pitch is held, not struck
again: a tie.

Loudness
The voices are added. A sum over full scale is turned down at once
and climbs back in a fifth of a second: nothing clips, but the loudest
voice ducks the rest. Balance the voices with V.

Hardware
On DOS the voices play on a Sound Blaster when the BLASTER variable
names one (DSP 2.00 or later): mono, and without unison. Without one
the PC speaker plays voice 1 only, as a square wave, and the sound
commands do nothing. SOUND and the plain speaker PLAY always use the
speaker. Put the melody first.

## Example
    lead$ = "T140 W4 Z1,9,3,11,4 H0,4,12,1 U8 R4 V11 L8 O4 C E G > C2"
    bass$ = "T140 W4 Z1,12,4,4 H0,5,5,1 V11 L4 O2 C G C G"
    PLAY lead$, bass$
    ' an FM brass lead over an FM bass
