# ATN Function
keys: ATN

Syntax
    ATN(n)

Returns the arctangent of n as an angle in radians, between -PI/2
and PI/2. The result is a SINGLE unless n is a DOUBLE. An angle in
degrees is radians * 180 / PI, and PI itself is 4 * ATN(1).

## Example
    CONST pi = 3.141593
    PRINT ATN(1) * 180 / pi     ' 45

See also [SIN](SIN), [COS](COS), [TAN](TAN)
