# Test Scripts: Keys, Mouse and Shots
keys: test scripts, QB_KEYS, QB_MOUSE, QB_SHOT, scripted input

In the windowed build (QuickrBasic.app) the environment variables
QB_KEYS, QB_MOUSE and QB_SHOT drive the IDE without a person.

See also [PEN](PEN), [command line](command line)

## Details
They test programs and take pictures. A time T is a frame number (60 a
second) or seconds: 1.5s.

    QB_KEYS    keys to type, separated by commas or spaces: HEXKEY
               typed after about a second, or T:HEXKEY at time T
    QB_MOUSE   T:X,Y[:B] puts the mouse at pixel X,Y at time T with
               buttons B (1 left, 2 right, 4 middle, sum them; 0
               none). A click is a B, then a 0.
    QB_SHOT    file[@T], comma apart: the screen at T saved as a
               PPM file; when the last is saved the program exits

A key is its BIOS code in hex: scan code, then ASCII. FFFF is the
STOP key (Ctrl+Break). The mouse is what PEN reads.

## Example
    QB_KEYS="1s:1E61,2s:FFFF"
    QB_SHOT="out.ppm@3s"

    ' types "a" at 1 s, stops at 2 s, saves the screen at 3 s, exits
