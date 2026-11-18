# CLS Statement
keys: CLS

Syntax
    CLS [0 | 1 | 2]

Clears part of the screen and homes the cursor.

See also [COLOR](COLOR), [VIEW](VIEW), [LOCATE](LOCATE)

## Details
Clears part of the screen and homes the cursor.

    CLS or CLS 0   the whole screen
    CLS 1          the graphics viewport (set with VIEW)
    CLS 2          the text viewport (set with VIEW PRINT)

The cleared area is filled with the current background color.

CLS also moves the cursor to row 1, column 1 of the cleared area.

## Example
    COLOR 15, 1
    CLS                    ' screen turns blue
    PRINT "Fresh screen"   ' Fresh screen, top-left
