# Division by zero
keys: Division by zero, error 11

Error 11

A division, MOD or integer division has a zero divisor.

See also: [MOD](MOD), [operators](operators)

## Details

Cause: A division, [MOD](MOD) or integer division has a divisor of
zero. At compile time the divisor is a constant zero.

Remedy: Test the divisor before dividing.

A constant zero divisor is reported when the program is compiled; a
variable one when it runs.

Trap it with ON ERROR; ERR returns the number.

## Example
    d% = 0
    PRINT 10 \ d%        ' error 11
    IF d% <> 0 THEN PRINT 10 \ d%   ' fixed
