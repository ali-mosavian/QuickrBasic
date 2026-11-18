# MOD Operator
keys: MOD

Syntax
    dividend MOD divisor

Returns the remainder of the whole-number division. Both operands are
rounded to whole numbers first. The result has the sign of the
dividend. A divisor of 0 is a Division by zero error.

MOD ranks with integer division (\), below * and /.

## Example
    PRINT 17 MOD 5          ' 2
    PRINT -17 MOD 5         ' -2
    IF n MOD 2 = 0 THEN PRINT "even"

See also [operators](operators), [INT](INT)
