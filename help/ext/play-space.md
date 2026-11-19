# PLAY: Stereo, Reverb and Unison
keys: play space, S command, R command, I command, stereo, reverb, unison

Syntax
    S place      1 to 15
    R amount     0 to 15
    I spread     0 to 15

Place a voice between the speakers, put it in a room, or double it.

See also [PLAY voices](play voices), [vibrato and glide](play pitch),
[writing a song](play song)

## Details
S n: stereo place (default 8)
1 is the far left, 8 the middle, 15 the far right. Keep the bass and
the kick in the middle; spread chords to the sides.

R n: reverb (default 0)
How much of the voice a room answers. There is one room for all the
voices; its tail lasts about a second. 2 to 5 puts a lead in a space.
Drums and bass blur with it: leave them at 0.

I n: unison (default 0)
A second copy of the voice, n times 2 cents sharp. The two drift in
and out of step, as a section of players: 2 thickens, 6 to 8 is a
string section. Noise has none.

On DOS a Sound Blaster is mono and has no unison.

## Example
    PLAY "T120 O4 L4 W0 Q30 H0,4,9,2 S1 C S4 E S8 G S12 > C S15 E"
    PLAY "T100 O4 L4 W4 Z1,9,3,11,4 H0,4,12,1 R0 C E G P4 R8 C E G P2"
    PLAY "T90 O3 L1 W2 Y1,2400,4 H3,5,11,4 I0 C I8 C"
    ' left to right; dry, then in a room; one voice, then a section
