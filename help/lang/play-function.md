# PLAY Function
keys: PLAY function, PLAY(n)

Syntax
    PLAY(n)

Returns how many notes are waiting to be played in the background
music queue. Zero means the music has finished.

n is ignored; any number works. In QuickrBasic the count covers
voice 1; see [play voices](play voices) for the others.

## Example
    PLAY "MB L4 CDEFGAB"           ' MB: play in the background
    DO WHILE PLAY(0) > 0
        LOCATE 1, 1: PRINT PLAY(0); "notes left "
    LOOP

See also [PLAY](PLAY), [PLAY string](PLAY string),
[ON event](ON event)
