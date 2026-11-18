# INT Function
keys: INT

Syntax
    INT(n)

Returns the largest whole number not greater than n. It rounds toward
minus infinity: INT(2.7) is 2 and INT(-2.1) is -3.

The result has the type of n, so INT does not change a floating-point
number into an integer type. Use [CINT](CINT) or [CLNG](CLNG) for that.

## Example
    PRINT INT(8.9), INT(-8.9)       ' 8  -9
    PRINT INT(RND * 6) + 1          ' a die throw

See also [FIX](FIX), [CINT](CINT), [data types](data types)
