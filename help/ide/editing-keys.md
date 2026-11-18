# Editing Keys
keys: editing keys, cursor keys, WordStar keys, keyboard

Keys that move the cursor and change text.

    Arrows move   Home, End line ends   Ctrl+Y delete line   Tab complete

See also [selecting text](selecting text), [completion](completion),
[syntax checking](syntax checking).

## Details
    Arrows            move one character or line
    Home, End         start, end of the line
    Ctrl+Left, Right  one word back, forward
    PgUp, PgDn        a screen up, down
    Ctrl+Home, End    first, last line of the program
    Enter             split the line; indent follows the block
    Backspace         delete before the cursor
    Del               delete at the cursor
    Ctrl+Y            delete the whole line
    Ins               switch insert and overwrite
    Tab               complete the word, else indent

WordStar keys work as well:

    Ctrl+E, X, S, D   up, down, left, right
    Ctrl+A, F         word left, word right
    Ctrl+R, C         PgUp, PgDn
    Ctrl+G, V         Del, Ins
    Ctrl+Q S, D       start, end of the line
    Ctrl+Q R, C       first, last line
    Ctrl+Q Y          delete to the end of the line

Leaving a line commits it: keywords are capitalised, trailing
spaces dropped and, if checking is on, the syntax checked.

## Example
    Type:   print "hi"   then press Down
    The line becomes  PRINT "hi"
    Ctrl+Y         delete the line
