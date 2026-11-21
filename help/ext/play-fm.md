# PLAY: FM
keys: play fm, Z command, FM, modulator

Syntax
    W4
    Z ratio [, depth [, fall [, level [, feedback [, start]]]]]

Wave 4 is a sine bent by a second sine, the modulator. Z sets the
modulator, and with it the tone: basses, brass, organs, bells, drums.

See also [PLAY voices](play voices), [waves](play waves),
[envelopes](play envelope), [vibrato and glide](play pitch)

## Details
Bending the sine adds harmonics: the further, the brighter.

    ratio      0.01 to 15.99 (default 1): the modulator's pitch, so
               many times the note's. 1 is full, like a sawtooth; 2
               hollow, like a square; high odd ones ring like metal;
               one with hundredths, as 1.41, rings off the note's
               harmonics, as a drum
    depth      0 to 15 (default 8): how far it bends. 0 is a plain
               sine, 4 warm, 8 bright, 12 and over harsh
    fall       0 to 15 (default 0): how fast the bend falls at each
               note, on the envelope's decay scale
    level      0 to 15 (default 15): the part of the depth the bend
               falls to and holds. 15 is no fall, 0 down to a sine
    feedback   0 to 7 (default 0): the modulator bends itself, towards
               a sawtooth, then noise
    start      0 to 15 (default 15): the part of the depth the bend
               starts at. Under the level, it rises to it

A number left off the end keeps its last value.

The bend falls apart from the volume, which H shapes: a note can
start bright and end round, as a plucked string does.

Instruments
    W4 Z1,12,4,4 H0,5,5,1        a plucked bass
    W4 Z1,9,3,11,4 H0,4,12,1     brass
    W4 Z1,4,3,13,4 H0,4,11,1     an organ
    W4 Z7,6,7,0 H0,9,0,9         a bell
    W4 Z1,0 H0,4,3,3 K1,8        a kick drum: a sine falling fast

## Example
    PLAY "T120 O3 L2 W4 H0,0,15,2 Z1,0 C Z1,4 C Z1,8 C Z1,12 C"
    PLAY "T100 O2 L2 W4 H0,6,6,3 Z1,12,0,15 C Z1,12,4,4 C Z1,12,7,0 C"
    ' the depth rising, then the bend falling faster and further
