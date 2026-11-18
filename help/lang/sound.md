# SOUND Statement
keys: SOUND

Syntax
    SOUND frequency, duration

Plays one tone on the PC speaker.

See also [PLAY](PLAY), [BEEP](BEEP), [play voices](play voices)

## Details

Plays one tone on the PC speaker.

  frequency   in hertz, 37 to 32767.
  duration    in clock ticks, 0 to 65535; 18.2 ticks make a second.

Values outside the ranges are an Illegal function call. The program
waits for the tone to end. SOUND always uses the speaker, not the
six PLAY voices.

One tick is 1 / 18.2 second, so SOUND 440, 18 is about one second of
A. For music strings use PLAY.

## Example
    FOR f% = 200 TO 1200 STEP 100
        SOUND f%, 2                 ' ten short rising tones
    NEXT f%
