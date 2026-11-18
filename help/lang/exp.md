# EXP Function
keys: EXP

Syntax
    EXP(n)

EXP returns e (2.718282...) raised to the power n. It is the inverse
of [LOG](LOG). The result is SINGLE unless n is DOUBLE. If n is above
about 88 (709 for DOUBLE) the result overflows: Overflow.

See also [LOG](LOG), [SQR](SQR)

## Details
EXP returns e (2.718282...) raised to the power n. It is the inverse
of [LOG](LOG). The result is SINGLE unless n is DOUBLE. If n is above
about 88 (709 for DOUBLE) the result overflows: Overflow.

## Example
    PRINT EXP(1)         ' 2.718282
    PRINT EXP(LOG(5))    ' 5
