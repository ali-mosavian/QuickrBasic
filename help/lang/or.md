# OR Operator
keys: OR

Syntax
    a OR b

Combines two numbers bit by bit: a result bit is 1 when the bit in a,
in b or in both is 1.

  a  b  a OR b
  0  0     0
  0  1     1
  1  0     1
  1  1     1

Both operands are rounded to integers first. In a condition, a
true comparison is -1 (all bits set) and a false one 0, so OR joins
conditions naturally.

OR binds weaker than AND and NOT, stronger than XOR, EQV and IMP.

## Example
    PRINT 12 OR 5                  ' 13
    IF x < 0 OR x > 9 THEN PRINT "out of range"
    flags = flags OR 4             ' set bit 2

See also [operators](operators), [AND](AND), [XOR](XOR), [NOT](NOT)
