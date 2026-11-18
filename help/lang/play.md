# PLAY Statement
keys: PLAY, PLAY ON, PLAY OFF, PLAY STOP

Syntax
    PLAY commandstring$[, commandstring2$ ...]
    PLAY {ON | OFF | STOP}

Plays music described by a string of commands: notes, length, tempo,
octave, and so on. The commands are listed under
[PLAY string](PLAY string).

QuickrBasic has six voices. Give each its own string and they sound
together; a single string plays on voice 1. See
[play voices](play voices).

PLAY ON, OFF and STOP control the event set up by ON PLAY: see
[ON event](ON event). The count of notes still waiting is the
[PLAY function](PLAY function).

## Example
    PLAY "T120 O3 L8 C D E F G4"
    PLAY "O3 L4 C E G", "O4 L4 E G >C"      ' two voices

See also [PLAY string](PLAY string), [play voices](play voices),
[SOUND](SOUND), [BEEP](BEEP)
