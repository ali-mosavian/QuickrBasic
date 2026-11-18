# PLAY: Waves and Volume
keys: play waves, W command, Q command, V command, pulse width

Syntax
    W n      wave
    Q n      pulse width
    V n      volume

Sound commands of a PLAY string, for the voice that string plays.

## W n: the wave (0 to 3)
    0   pulse (default): the PC speaker's sound
    1   triangle: soft, flute-like
    2   sawtooth: bright, buzzy
    3   noise: a hiss, for drums and explosions

## Q n: pulse width, 1 to 99 (default 50)
The percentage of each cycle the pulse is high. 50 is a hollow square
wave; a small number sounds thin and nasal. Only wave 0 uses it.

## V n: volume, 0 to 15 (default 8)
Sets one voice only, so the voices can be balanced against each other.

Values outside the ranges are Illegal function call.

## Example
    PLAY "T120 O3 L4 W0 C E G P4 W1 C E G P4 W2 C E G P4 W3 C E G"
    PLAY "W0 L8 Q50 C C Q25 C C Q10 C C Q5 C C"

See also [PLAY voices](play voices), [envelopes](play envelope),
[filters](play filter)
