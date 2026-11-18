# CBYTE Function
keys: CBYTE

Syntax
    CBYTE(numeric-expression)

Rounds the number to the nearest whole number (halves go to the even
one, as CINT does) and returns it as a [BYTE](BYTE) value, 0 to 255.
A result outside that range is an Overflow error. A string argument is
a Type mismatch.

## Example
    PRINT CBYTE(254.6)    ' 255
    PRINT CBYTE(2.5)      ' 2
    PRINT CBYTE(256)      ' Overflow

See also [BYTE](BYTE), [DEFBYTE](DEFBYTE), [CINT](CINT)
