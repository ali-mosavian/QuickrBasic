# PLAY: Drums, Bass and Effects
keys: play examples, play effects, drums, sound effects

Short recipes for the six-voice synthesizer. Paste into the IDE and
run with Shift+F5.

## Drums: noise voice
    kick$ = "H0,4,0,3 Y1,300,4 O1 C"
    snare$ = "H0,4,0,4 Y0 O4 C"
    hat$ = "H0,1,0,1 Y3,6000 O6 C"
    bar$ = kick$ + hat$ + snare$ + hat$
    PLAY "T130 L8 W3 V8 " + bar$ + bar$

## Three voices
    PLAY "T130 O4 L8 W2 H0,5,9,3 V9 E G > C < G E G E C", _
         "T130 O2 L2 W1 H0,8,10,4 V11 C C", _
         "T130 L8 W3 V7 H0,4,0,3 O1 C P8 C P8 C P8 C P8"

## Effects
Each note lasts at least 55 ms, so at T255 use L16 or longer.

    PLAY "T255 L16 W0 Q25 H0,3,0,1 V10 O3 C E G > C E"   ' jump
    PLAY "T255 W0 Q50 H0,6,0,6 V10 O5 L16 B > E4"        ' coin
    PLAY "T255 L16 W2 H0,3,0,1 V10 N76 N70 N64 N58 N52"  ' laser
    PLAY "T255 W3 H0,10,0,10 V15 Y1,900,6 O1 C4 P4"      ' explosion

## Effects over music
Keep the tune on voices 1 and 2 in the background and fire effects on
voice 3 only: leaving the music's strings out leaves it playing.

    tune$ = "T130 O4 L8 W2 H0,5,9,3 V8 E G > C < G E G E C"
    bass$ = "T130 O2 L2 W1 V10 C C"
    jump$ = "T255 L16 W0 Q25 H0,3,0,1 V10 O3 C E G > C E"
    PLAY "MB" + tune$, bass$
    FOR i = 1 TO 3
      PLAY , , jump$
    NEXT
    PLAY "MF P64"

See also [PLAY voices](play voices), [PLAY string](PLAY string)
