# Numeric constant errors
keys: Illegal type character in numeric constant, ~Illegal type character in numeric constant /decimal, ~Illegal type character in numeric constant /range, Overflow in numeric constant, Math overflow, integer literal is out of range, based integer does not fit its type, missing based-integer digits, missing exponent digits

## Cause

A number is written wrong: `%` or `&` on a number with a decimal
point, a suffix that does not fit the value, too many digits, `&H`
with no digits, or `E` with no exponent.

## Remedy

Use a suffix that fits (`%` integer, `&` long, `!` single, `#`
double), or drop it. Write `&HFF` and `1E5` in full.

See also: [data types](data types)
