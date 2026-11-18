# IMP Operator
keys: IMP

Syntax
    expression1 IMP expression2

Logical implication, bit by bit: false only where expression1 has a 1
and expression2 a 0.

See also [NOT](NOT), [operators](operators)

## Details
A result bit is 0 only where expression1 has a 1 and expression2 has
a 0.

    1st  2nd  result
     1    1     1
     1    0     0
     0    1     1
     0    0     1

Operands are rounded to whole numbers. Used on comparisons it reads
"if a then b": it is false only when a is true and b is false.
IMP has the lowest precedence of the logical operators.

## Example
    PRINT (3 > 2) IMP (1 > 2)     ' 0: true implies false
    PRINT (3 < 2) IMP (1 > 2)     ' -1: false implies anything
