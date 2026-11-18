# TAN Function
keys: TAN

Syntax
    TAN(angle)

Returns the tangent of angle, given in radians.

See also [SIN](SIN), [COS](COS), [ATN](ATN)

## Details

Returns the tangent of angle, given in radians. The result is SINGLE,
or DOUBLE if angle is DOUBLE. It grows without bound near odd
multiples of pi / 2.

TAN(x) is SIN(x) / COS(x). The arctangent is ATN, and ATN(1) * 4 is
pi.

## Example
    PRINT TAN(3.141593 / 4)             ' 1
