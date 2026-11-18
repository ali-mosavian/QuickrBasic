# SIN Function
keys: SIN

Syntax
    SIN(angle)

Returns the sine of angle, given in radians.

See also [COS](COS), [TAN](TAN), [ATN](ATN)

## Details

Returns the sine of angle, given in radians. The result is SINGLE,
or DOUBLE if angle is DOUBLE. Degrees times 3.141593 / 180 gives
radians.

For a DOUBLE result pass a DOUBLE: SIN(3.14159265358979#). Large
arguments lose accuracy.

## Example
    PRINT SIN(3.141593 / 2)             ' 1
