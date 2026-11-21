# PLAY: Vibrato and Glide
keys: play pitch, U command, K command, vibrato, glide, portamento

Syntax
    U depth
    K time [, over [, under [, hundredths]]]

Move a note's pitch: U wavers it, K slides into it.

See also [PLAY voices](play voices), [FM](play fm),
[stereo, reverb and unison](play space)

## Details
U n: vibrato, 0 to 15 (default 0)
The pitch wavers 6 times a second, once the note has sounded a fifth
of a second: short notes stay steady, held ones sing. Each step is
0.2 percent of the pitch each way; 15 is half a semitone.

K time, over, under, hundredths: glide (default 0,0,0,0)
Each note slides to its pitch in the time:

    time   0   1   2   3   4   5   6   7
    ms     0  10  20  30  40  60  80 100
    time   8   9  10  11  12  13  14  15
    ms   120 150 200 250 300 400 500 800

Where the slide starts:
    over 0, under 0   from the last note's pitch
    over 1 to 15      from above: 1 + over/4 times the note's pitch
    under 1 to 24     from so many semitones below, whatever over is
    hundredths 1-1600 from so many hundredths of the pitch, whatever
                      over and under are: 140 falls as a tom does

K1,8 on a plain sine (W4 Z1,0) is a kick drum. K8,0,2 scoops each note
up two semitones, as a singer does. The glide stays set: put K0
before a sound that should not slide.

## Example
    PLAY "T100 O4 W4 Z1,9,3,11,4 H0,4,12,2 L8 C D E F U0 G1 U6 G1"
    PLAY "T120 O3 L4 W2 Y1,2000,4 H0,4,12,2 K8 C G > C < G"
    PLAY "T100 O4 L4 W0 Q30 H1,4,10,2 K8,0,2 E G A2"
    ' a held note with vibrato, a slide from note to note, scoops
