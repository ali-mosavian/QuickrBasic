# DEFBYTE Statement
keys: DEFBYTE

Syntax
    DEFBYTE letter[-letter] [, letter[-letter]]...

Makes every variable and function whose name starts with one of the
letters a [BYTE](BYTE), unless a type character or an AS clause says
otherwise. A BYTE has no type character, so DEFBYTE is the only way
to get BYTE variables without naming each one.

Like the other DEFtype statements it covers the rest of the program
from where it appears.

## Example
    DEFBYTE B-C
    b1 = 255
    PRINT b1
    c = c + 1             ' c was 0: now 1
    b1 = b1 + 1           ' Overflow

See also [DEFINT](DEFtype), [DEFSHORT](DEFSHORT), [BYTE](BYTE)
