# Sound

New to music or sound effects? Start with the [tutorial](play-tutorial.md).

A program that uses none of this sounds as it did in QBasic: one square wave, the same notes at the
same ticks. On DOS the voices play on a Sound Blaster when `BLASTER` names one (DSP 2.00 or later); without
one, the PC speaker plays the first voice only.

`PLAY a$, b$, ...` plays up to six voices at once, a string each, after PCjr and Tandy BASIC's
three. A string left out leaves its voice playing what it was: `PLAY , , fx$` plays voice 3 alone.
Each voice keeps its own octave, length, tempo and style; `MF` and `MB` apply to all. `PLAY(n)` counts
the first voice's notes.

These commands, in any `PLAY` string, set the voice's sound. Any of them, or a second string, plays
the voice through the synthesizer instead of the speaker.

| Command | Sets | Range | Default |
|---|---|---|---|
| `V`n | volume | 0–15 | 8 |
| `W`n | wave: 0 pulse, 1 triangle, 2 sawtooth, 3 noise, 4 FM | 0–4 | 0 |
| `Q`n | pulse width, percent high | 1–99 | 50 |
| `H`a`,`d`,`s`,`r | envelope: attack, decay, sustain level, release, the SID's scale | 0–15 each | `H0,0,15,0` |
| `Y`m[`,`f[`,`r]] | filter: 0 off, 1 low-pass, 2 band-pass, 3 high-pass; cutoff Hz; resonance | m 0–3, f 30–12000, r 0–15 | off, 1000, 0 |
| `Z`r[`,`d[`,`f[`,`l[`,`b]]]] | FM's modulator: its pitch, r times the note's; how far it bends the carrier; how fast the bend falls at each note (the envelope's decay scale) and the level it holds; how far it bends itself | r 1–15, d f l 0–15, b 0–7 | 1, 8, 0, 15, 0 |
| `U`n | vibrato: the pitch wavers 6 times a second, a fifth of a second into the note | 0–15 | 0 |
| `K`t[`,`q] | glide: a note slides to its pitch in t (10 ms to 800 ms), from the last note's, or from q quarters over its own | 0–15 each | 0, 0 |
| `I`n | unison: a second voice n times 2 cents sharp, the two beating as a section does (not on DOS, nor for noise) | 0–15 | 0 |

FM (`W4`) is a sine bent by a second sine, as a Yamaha chip's two operators. The bend has its own fall, apart from
the volume's envelope: `Z1,12,4,4` plucks a bass that starts bright and ends round, `Z1,9,3,11,5` holds a brass
bright, its modulator bending itself towards a saw, and `Z7,6,7,0` rings a bell. Depth 0 is a plain sine: with
`K4,8` it falls two octaves in 40 ms, a kick drum.

A synthesizer voice's notes are timed to a 16th of a clock tick (3.4 ms), counted in the synthesizer's own samples
on the Mac and on a Sound Blaster; the speaker's keep QB's whole ticks.

Attack runs from 2 ms (0) to 8 s (15); decay and release from 6 ms to 24 s. Each number may be
`=var;` as in the other commands. A new program starts with every voice at its defaults.

```basic
lead$ = "T140 W0 Q25 H0,8,10,4 V12 L8 O4 C E G > C < G E C4"
bass$ = "T140 W2 Y1,600,10 V10 L4 O2 C G C G"
drum$ = "T140 W3 H0,3,0,2 V6 L8 O5 C P8 C P8 C P8 C P8"
PLAY lead$, bass$, drum$
```
