# Sound

New to music or sound effects? Start with the [tutorial](play-tutorial.md).

A program that uses none of this sounds as it did in QBasic: one square wave, the same notes at the
same ticks. On DOS the PC speaker plays the first voice only.

`PLAY a$, b$, c$` plays up to three voices at once, as PCjr and Tandy BASIC did. Each voice keeps
its own octave, length, tempo and style; `MF` and `MB` apply to all. `PLAY(n)` counts the first
voice's notes.

These commands, in any `PLAY` string, set the voice's sound. Any of them, or a second string, plays
the voice through the synthesizer instead of the speaker.

| Command | Sets | Range | Default |
|---|---|---|---|
| `V`n | volume | 0–15 | 8 |
| `W`n | wave: 0 pulse, 1 triangle, 2 sawtooth, 3 noise | 0–3 | 0 |
| `Q`n | pulse width, percent high | 1–99 | 50 |
| `H`a`,`d`,`s`,`r | envelope: attack, decay, sustain level, release, the SID's scale | 0–15 each | `H0,0,15,0` |
| `Y`m[`,`f[`,`r]] | filter: 0 off, 1 low-pass, 2 band-pass, 3 high-pass; cutoff Hz; resonance | m 0–3, f 30–12000, r 0–15 | off, 1000, 0 |

Attack runs from 2 ms (0) to 8 s (15); decay and release from 6 ms to 24 s. Each number may be
`=var;` as in the other commands. A new program starts with every voice at its defaults.

```basic
lead$ = "T140 W0 Q25 H0,8,10,4 V12 L8 O4 C E G > C < G E C4"
bass$ = "T140 W2 Y1,600,10 V10 L4 O2 C G C G"
drum$ = "T140 W3 H0,3,0,2 V6 L8 O5 C P8 C P8 C P8 C P8"
PLAY lead$, bass$, drum$
```
