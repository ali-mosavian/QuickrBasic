# CDBL and CSNG Functions
keys: CDBL, CSNG

Syntax
    CDBL(n)
    CSNG(n)

CDBL converts n to double precision (about 16 digits). CSNG converts
it to single precision (about 7 digits), rounding.

CDBL adds no accuracy: a SINGLE is already inexact, and widening it
keeps the inexactness.

## Example
    PRINT CDBL(.1!)       ' .1000000014901161
    PRINT CSNG(1 / 3)     ' .3333333

See also [CINT](CINT), [data types](data types)
