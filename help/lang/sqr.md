# SQR Function
keys: SQR

Syntax
    SQR(numexpr)

Returns the square root of numexpr.

See also [EXP](EXP), [LOG](LOG), [ABS](ABS)

## Details

Returns the square root of numexpr. The result is SINGLE, or DOUBLE
if numexpr is DOUBLE. A negative argument is an Illegal function
call.

For other roots use exponents: x ^ (1 / 3) is the cube root of a
positive x.

## Example
    PRINT SQR(16); SQR(2)               ' 4  1.414214
