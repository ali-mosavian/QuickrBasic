# PLAY: Waves and Volume
keys: play waves, W command, Q command, V command, pulse width

Syntax
    W n      wave 0 to 4
    Q n      pulse width 1 to 99
    V n      volume 0 to 15

Sound commands of a PLAY string, for the voice that string plays.

See also [PLAY voices](play voices), [envelopes](play envelope),
[filters](play filter), [FM](play fm)

## Details
W n: the wave (0 to 4)
    0   pulse (default): the PC speaker's sound
    1   triangle: soft, flute-like
    2   sawtooth: bright, buzzy
    3   noise: a hiss, for drums and explosions; the note's pitch
        sets its colour
    4   FM: a sine bent by a second sine. See [FM](play fm)

Q n: pulse width, 1 to 99 (default 50)
The percentage of each cycle the pulse is high. 50 is a hollow square
wave; a small number sounds thin and nasal. Only wave 0 uses it.

V n: volume, 0 to 15 (default 8)
Sets one voice only, so the voices can be balanced against each other.

Values outside the ranges are Illegal function call.

## Example
    PLAY "T120 O3 L4 W0 C E G P4 W1 C E G P4 W2 C E G P4 W3 C E G"
    PLAY "W0 L8 Q50 C C Q25 C C Q10 C C Q5 C C"
    ' the first four waves in turn, then the pulse growing thinner
