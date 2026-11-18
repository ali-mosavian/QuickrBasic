# FIX Function
keys: FIX

Syntax
    FIX(n)

FIX drops the fraction of n, rounding toward zero. It differs from
[INT](INT), which rounds down, for negative numbers only.

    FIX(2.8)  = 2      INT(2.8)  = 2
    FIX(-2.8) = -2     INT(-2.8) = -3

## Example
    PRINT FIX(-7.9)    ' -7

See also [INT](INT), [CINT](CINT), [SGN](SGN)
