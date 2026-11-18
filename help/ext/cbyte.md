# CBYTE Function
keys: CBYTE

Syntax
    CBYTE(numeric-expression)

Rounds the number to the nearest whole number and returns it as a
[BYTE](BYTE) value, 0 to 255.

See also [BYTE](BYTE), [DEFBYTE](DEFBYTE), [CINT](CINT)

## Details
Halves go to the even whole number, as CINT does: 2.5 gives 2 and
3.5 gives 4.

Errors
    Overflow        the rounded result is below 0 or above 255
    Type mismatch   the argument is a string

## Example
    PRINT CBYTE(254.6)    ' 255
    PRINT CBYTE(2.5)      ' 2
    PRINT CBYTE(256)      ' Overflow
