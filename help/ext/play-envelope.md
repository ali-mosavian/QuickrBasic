# PLAY: Envelope
keys: play envelope, H command, ADSR, envelope

Syntax
    H attack, decay, sustain, release

Shapes the loudness of every note the voice plays next. All four
numbers are 0 to 15.

See also [PLAY voices](play voices), [waves](play waves),
[filters](play filter)

## Details
The numbers are as on the SID. The default is H0,0,15,0: instant
attack, full sustain, instant release.

    attack    time from silence to full volume: 2 ms (0) to 8 s (15)
    decay     time to fall to the sustain level: 6 ms to 24 s
    sustain   the level held while the note lasts: 0 silent, 15 full
    release   time to fade after the note ends: 6 ms to 24 s

Give all four numbers; fewer is Illegal function call.

With sustain 0 a note is a pluck: it dies in the decay however long
it is held. The times, for 0 to 15:

    attack    2 8 16 24 38 56 68 80 100 250 500 800 ms, 1 3 5 8 s
    decay     three times the attack's: 6 ms to 24 s

## Example
An organ, a pluck, a swell and a bell:

    PLAY "T90 O3 L2 W2 H0,0,15,0 C H0,6,0,4 C H10,0,15,8 C"
    PLAY "W1 H0,9,0,9 > C"
