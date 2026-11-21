# PLAY String Commands
keys: PLAY string, music macro language, macro language, music commands

A string given to [PLAY](PLAY) is a list of commands. Spaces are
ignored; case does not matter. n is a number.

See also [PLAY](PLAY), [SOUND](SOUND), [Voices](play voices)

## Details
Notes
```
A-G        play that note in the current octave
#  +       after a note: sharp
-          after a note: flat
n          after a note: its length, 1 to 64 (as for L)
.          after a note: lengthen it by half; ".." by three quarters
N n        play note number n, 0 to 84; 0 is a rest
P n        pause for a length n, 1 to 64
```

Octave
```
O n        set the octave, 0 to 6 (default 4)
>          up one octave
<          down one octave
```
Octave 3 starts at middle C.

Length and speed
```
L n        default note length: 1 whole, 2 half, 4 quarter ... 64
T n        tempo in quarter notes a minute, 32 to 255 (default 120)
MN         normal: a note sounds for 7/8 of its length
ML         legato: a note sounds for all of it
MS         staccato: a note sounds for 3/4 of it
```

Foreground and background
```
MF         wait for the music to finish before the next statement
MB         play in the background; the program continues
```
In the background the notes queue; PLAY(0) counts the notes waiting.

Calling and variables
```
X variable$   play the commands in a string variable
=variable;    use a numeric variable's value for n
```
A variable goes into the string through VARPTR$, which gives the
string the variable's address:
```
t% = 160: m$ = "C D E"
PLAY "T=" + VARPTR$(t%) + "X" + VARPTR$(m$)
```

QuickrBasic voices
PLAY a$, b$, ... plays up to 32 voices at once, a string for each.
Each voice has its own octave, length, tempo and sound. These commands
choose the sound and switch the voice to the synthesizer:
```
V n          volume, 0 to 15 (default 8)
W n          wave: 0 pulse, 1 triangle, 2 sawtooth, 3 noise, 4 FM
Q n          pulse width, percent high, 1 to 99 (default 50)
H a,d,s,r    envelope: attack, decay, sustain level, release, 0 to 15
Y m,f,r,s,t  filter: m 0 off, 1 low-pass, 2 band-pass, 3 high-pass
             f cutoff in Hz, 30 to 12000; r resonance, 0 to 15; s the
             cutoff a note starts at, 0 none; t its time, 0 to 15
Z r,d,f,l,b,s  FM's modulator: ratio 0.01 to 15.99; depth, fall,
             level, start 0 to 15; feedback 0 to 7
U n          vibrato, 0 to 15
K t,q,u,p    glide: time 0 to 15; from over, 0 to 15; from under,
             0 to 24 semitones; from p hundredths, 0 to 1600
S n          stereo place, 1 left to 15 right (default 8)
R n          reverb, 0 to 15
I n          unison, 0 to 15
J n,p        layer, 0 to 3: the commands after it set it; its pitch,
             hundredths of the note's, 0 to 1600
```
Under ML a note followed by one of the same pitch is held, not struck
again. See [Voices](play voices), [FM](play fm),
[vibrato and glide](play pitch), [stereo and reverb](play space).

Example
```
PLAY "T140 O3 L8 C D E F G4 G4 A A A A G2"
```

## Example
    PLAY "T140 O3 L8 C D E F G4 G4 A A A A G2"
    t% = 160: m$ = "C D E"
    PLAY "T=" + VARPTR$(t%) + "X" + VARPTR$(m$)
