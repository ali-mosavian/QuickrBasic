# Division by zero
keys: Division by zero, error 11

Error 11

## Cause

A division, [MOD](MOD) or integer division has a divisor of zero. At
compile time the divisor is a constant zero.

## Remedy

Test the divisor before dividing.

## Example
    IF d% <> 0 THEN PRINT n% \ d%

See also: [MOD](MOD), [operators](operators)
