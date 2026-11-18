# LONG Data Type
keys: LONG, &

Syntax
    DIM x AS LONG
    x& = 100000

A whole number from -2147483648 to 2147483647, in 4 bytes. The type
suffix is &, and DEFLNG makes untyped names LONG.

In QuickrBasic LONG and [INTEGER](INTEGER) are one type: they hold the
same range and may be mixed freely, and neither wraps at 32767. The
two names exist so QBasic programs run unchanged. The 16-bit type is
[SHORT](SHORT).

Assigning a fraction rounds it to the nearest whole number. A value
outside the range is an Overflow error. [CLNG](CLNG) converts to it.

## Example
    n& = 2000000000
    PRINT n& + 147483647        ' 2147483647

See also [INTEGER](INTEGER), [SHORT](SHORT), [CLNG](CLNG),
[data types](data types)
