# CLS Statement
keys: CLS

Syntax
    CLS [0 | 1 | 2]

Clears part of the screen and homes the cursor.

    CLS or CLS 0   the whole screen
    CLS 1          the graphics viewport (set with VIEW)
    CLS 2          the text viewport (set with VIEW PRINT)

The cleared area is filled with the current background color.

## Example
    COLOR 15, 1
    CLS
    PRINT "Fresh screen"

See also [COLOR](COLOR), [VIEW](VIEW), [LOCATE](LOCATE)
