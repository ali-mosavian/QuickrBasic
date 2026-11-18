# LOG Function
keys: LOG

Syntax
    LOG(n)

Returns the natural logarithm (base e) of n. n must be greater than
zero; otherwise it is an Illegal function call.

The result is SINGLE unless n is DOUBLE. For another base b use
LOG(n) / LOG(b).

## Example
    PRINT LOG(2.718282)         ' 1
    PRINT LOG(1000) / LOG(10)   ' 3 (base 10)

See also [EXP](EXP), [SQR](SQR)
