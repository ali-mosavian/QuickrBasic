# PLAY: Six Voices
keys: play voices, voices, synthesizer, SID

Syntax
    PLAY string1$ [, string2$ [, ... string6$]]

QuickrBasic plays up to six voices at once, one string for each, on a
synthesizer modelled on the C64's SID chip. A program that never uses
the extra commands sounds as in QBasic: one square wave.

## Voices
 - Strings go to voices 1, 2, 3 ... in order.
 - A string left out leaves its voice playing whatever it was:
   PLAY , , fx$ plays voice 3 only, and the music on 1 and 2 goes on.
 - Each voice keeps its own octave, length, tempo and sound. Set them
   in every string. MF and MB apply to all voices.
 - PLAY(n) counts the notes waiting in voice 1.
 - A new program starts with every voice at its defaults.

## Sound commands
Allowed in any PLAY string. Using one, or a second string, sends the
voice through the synthesizer. Each number may be =var; as usual.

    V n         volume 0-15 (default 8)
    W n         wave 0-3
    Q n         pulse width 1-99 percent (default 50)
    H a,d,s,r   envelope, each 0-15 (default 0,0,15,0)
    Y m,f,r     filter

See [waves](play waves), [envelopes](play envelope) and
[filters](play filter). Drums, bass and effects: [examples](play examples).

## Hardware
On DOS the voices play on a Sound Blaster when the BLASTER variable
names one (DSP 2.00 or later). Without one the PC speaker plays voice
1 only, as a square wave, and V, W, Q, H and Y do nothing. SOUND and
the plain speaker PLAY always use the speaker. Put the melody first.

## Example
    lead$ = "T140 W0 Q25 H0,8,10,4 V12 L8 O4 C E G > C < G E C4"
    bass$ = "T140 W2 Y1,600,10 V10 L4 O2 C G C G"
    PLAY lead$, bass$

See also [PLAY](PLAY), [PLAY string](PLAY string), [SOUND](SOUND)
