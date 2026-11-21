# PLAY: Layers
keys: play layers, J command, layers

Syntax
    J layer [, pitch]

A voice sounds up to three layers with its own sound, each its own
wave, envelope, filter, FM, glide and volume, at its own pitch: a
drum's thump and its hiss, or sines at a bell's partials.

See also [PLAY voices](play voices), [FM](play fm),
[envelopes](play envelope)

## Details
    layer      0 to 3: the layer the commands after it set, 0 the
               voice's own sound, until the next J or the string's end
    pitch      0 to 1600: sounds the layer at so many hundredths of
               the note's pitch. 100 the note, 200 an octave over, 159
               a drum's second ring. 0 silences it and those after it

Layers come on in order, 1 before 2. J0,0 is the voice alone again.
U, I, R and S are the voice's, whatever layer J chose.

Each note starts its layers together, every wave from its beginning,
so every hit sounds the same. A layer costs as much as a voice.

## Example
    snare$ = "W4 Z1,0 H0,4,0,3 K1,4 V12 J1,250 W3 H2,4,0,4 Y2,3000,4 V9"
    PLAY snare$ + " O2 E"
    ' a sine's thump falling, and a hiss
