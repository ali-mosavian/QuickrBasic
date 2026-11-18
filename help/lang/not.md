# NOT Operator
keys: NOT

Syntax
    NOT expression

Inverts every bit of the whole-number value of expression. For a
comparison result, NOT -1 is 0 and NOT 0 is -1. For other numbers
NOT n equals -(n + 1).

NOT ranks above AND, OR, XOR, EQV and IMP, and below comparisons:
NOT a = b means NOT (a = b).

## Example
    PRINT NOT 0                 ' -1
    PRINT NOT 5                 ' -6
    IF NOT (x > 10) THEN PRINT "x is 10 or less"

See also [IMP](IMP), [operators](operators)
