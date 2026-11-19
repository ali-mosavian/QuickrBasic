# EQV Operator
keys: EQV

Syntax
    a EQV b

EQV is a bitwise "same as": each result bit is 1 where the bits of
a and b are equal, 0 where they differ. Both operands are rounded to
integers. It is the inverse of XOR: a EQV b equals NOT (a XOR b).

See also [operators](operators), [AND](AND), [OR](OR), [XOR](XOR),
[IMP](IMP), [NOT](NOT)

## Details
EQV is a bitwise "same as": each result bit is 1 where the bits of
a and b are equal, 0 where they differ. Both operands are rounded to
integers. It is the inverse of XOR: a EQV b equals NOT (a XOR b).

    a  b  a EQV b
    0  0     1
    0  1     0
    1  0     0
    1  1     1

For true/false values (-1 and 0) it tests whether two conditions
agree. Only [IMP](IMP) ranks lower; see [operators](operators).

## Example
    PRINT (3 > 2) EQV (5 > 4)     ' -1: both true
    PRINT 12 EQV 10               ' -7
