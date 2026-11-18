# LPOS Function
keys: LPOS

Syntax
    LPOS(n)

Returns the column where the next LPRINT will print on printer n,
counted from 1.

See also [LPRINT](LPRINT), [POS](POS), [WIDTH](WIDTH)

## Details
n is 0 or 1 for the first printer; the others report 1. A negative n
or one above 3 is an Illegal function call.

## Example
    LPRINT "abc";
    PRINT LPOS(0)           ' 4
