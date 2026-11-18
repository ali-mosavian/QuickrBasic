# COS Function
keys: COS

Syntax
    COS(angle)

Returns the cosine of an angle given in radians. A SINGLE unless
the argument is a DOUBLE. For degrees, multiply by PI / 180 first.

See also [SIN](SIN), [TAN](TAN), [ATN](ATN)

## Details
Returns the cosine of an angle given in radians. A SINGLE unless
the argument is a DOUBLE. For degrees, multiply by PI / 180 first.

## Example
    CONST pi = 3.141593
    PRINT COS(60 * pi / 180)    ' .5
