# BYTE Data Type
keys: BYTE, AS BYTE

Syntax
    DIM name AS BYTE
    ... AS BYTE            (TYPE fields, parameters, arrays)

A BYTE holds a whole number from 0 to 255. QuickrBasic adds it to
QBasic's types.

## Rules
 - There is no type-suffix character. Declare a BYTE with AS BYTE, or
   make untyped names bytes with [DEFBYTE](DEFBYTE).
 - Arithmetic is done as for INTEGER, in 32 bits. The check comes when
   the value is stored: outside 0 to 255 raises Overflow.
 - A fraction is rounded to the nearest whole number, as CINT does.
 - A BYTE takes one byte in a TYPE, in a file (GET, PUT, binary
   files) and in an array, one byte per element.
 - PLAY and DRAW can read a BYTE through =variable, as with other
   numbers: "O=" + [VARPTR$](VARPTR$)(n).
 - [CBYTE](CBYTE) converts a number to a BYTE value.

## Example
    DIM level AS BYTE
    level = 200
    level = level + 50     ' 250: fits
    level = level + 10     ' Overflow

See also [SHORT](SHORT), [data types](data types), [TYPE](TYPE)
