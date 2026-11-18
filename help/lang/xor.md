# XOR Operator
keys: XOR

Syntax
    a XOR b

Exclusive or: a bit of the result is 1 when exactly one operand has
that bit set. On whole numbers it works on every bit, so a XOR b XOR b
returns a. In a condition, -1 is true and 0 false, so it is true when
exactly one side is.

    a  b  a XOR b
    0  0    0
    0  1    1
    1  0    1
    1  1    0

Its precedence is below AND and OR; see [operators](operators).

## Example
    flags% = flags% XOR 4               ' flip bit 2
    PRINT 6 XOR 3                       ' 5

See also [AND](AND), [OR](OR), [NOT](NOT), [EQV](EQV), [IMP](IMP),
[operators](operators)
