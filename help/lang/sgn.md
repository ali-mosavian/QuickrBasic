# SGN Function
keys: SGN

Syntax
    SGN(numexpr)

Returns the sign of numexpr as an INTEGER: -1 if negative, 0 if zero,
1 if positive.

See also [ABS](ABS), [INT](INT), [FIX](FIX)

## Details

Returns the sign of numexpr as an INTEGER: -1 if negative, 0 if zero,
1 if positive.

SGN(0.0001) is 1 and SGN(-0.0) is 0.

## Example
    PRINT SGN(-7.5); SGN(0); SGN(300)   ' -1  0  1
