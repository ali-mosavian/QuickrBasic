# CINT and CLNG Functions
keys: CINT, CLNG

Syntax
    CINT(n)
    CLNG(n)

Rounds the numeric expression n to the nearest whole number. A
fraction of exactly .5 goes to the even neighbour: CINT(2.5) is 2,
CINT(3.5) is 4. This differs from INT (rounds down) and FIX (cuts
the fraction).

INTEGER and LONG are one 32-bit type here, so CINT and CLNG are the
same function and accept the same range, -2147483648 to
2147483647; anything outside is an error. QBasic's CINT stopped at
32767. For a 16-bit result use SHORT, for 0 to 255 use CBYTE.

## Example
    PRINT CINT(2.5), CINT(-2.5), CLNG(40000.6)    ' 2  -2  40001

See also [INT](INT), [FIX](FIX), [CDBL](CDBL), [CBYTE](CBYTE),
[data types](data types)
