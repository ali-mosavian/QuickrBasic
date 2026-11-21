# PLAY: Filter
keys: play filter, Y command, filter, resonance, cutoff

Syntax
    Y mode [, cutoff [, resonance [, from [, time]]]]

Filters the voice's sound: low-pass, band-pass or high-pass, with a
cutoff and a resonance.

See also [PLAY voices](play voices), [waves](play waves),
[envelopes](play envelope)

## Details
    mode        0 off (default), 1 low-pass, 2 band-pass, 3 high-pass
    cutoff      30 to 12000 Hz (default 1000)
    resonance   0 to 15 (default 0): emphasis around the cutoff
    from        0, or 30 to 12000 Hz (default 0): the cutoff each note
                starts at, moving to the cutoff. 0 is no sweep
    time        0 to 15 (default 0): how long the sweep takes, on the
                envelope's decay scale

A low-pass darkens a bright wave, a high-pass thins it, a band-pass
keeps a narrow band. High resonance makes the cutoff whistle. A
number left off the end keeps its last value. Y1,800,6,6000,4 closes
from 6000 Hz to 800 in 114 ms, as a pluck's brightness falls.

## Example
    PLAY "T120 O2 L4 W2 Y1,3000 C Y1,1200 C Y1,500 C Y1,200 C"
    PLAY "Y1,800,0 C Y1,800,8 C Y1,800,15 C Y0 C"
    ' the sawtooth darkens as the cutoff falls, then whistles
