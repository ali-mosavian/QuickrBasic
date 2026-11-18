# AND Operator
keys: AND

Syntax
    a AND b

Combines two numbers bit by bit. A result bit is 1 only where both
bits are 1. Both operands are rounded to integers first.

See also [operators](operators), [NOT](NOT), [OR](OR), [XOR](XOR)

## Details
Combines two numbers bit by bit. A result bit is 1 only where both
bits are 1. Both operands are rounded to integers first.

In a condition a true comparison is -1 (all bits set) and a false
one is 0, so AND also joins conditions:

    IF x > 0 AND x < 10 THEN PRINT "one digit"

Both sides are always evaluated; AND does not stop early.

AND is also the usual way to test or clear bits:

    PRINT 12 AND 10         ' prints 8
    flags = flags AND NOT 4 ' clears bit 2

    0 AND 0 = 0    0 AND 1 = 0    1 AND 0 = 0    1 AND 1 = 1

## Example
    x = 7
    IF x > 0 AND x < 10 THEN PRINT "one digit"    ' one digit
    PRINT 12 AND 10                              ' 8
