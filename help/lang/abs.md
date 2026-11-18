# ABS Function
keys: ABS

Syntax
    ABS(n)

Returns the absolute value of the numeric expression n: n itself
when it is zero or more, -n when it is less. The result has n's
type.

See also [SGN](SGN), [INT](INT), [FIX](FIX), [data types](data types)

## Details
Returns the absolute value of the numeric expression n: n itself
when it is zero or more, -n when it is less. The result has n's
type.

The fraction is kept: ABS(-0.25) is .25. A string argument is a
Type mismatch.

## Example
    PRINT ABS(-7.5), ABS(3)    ' 7.5  3
