# GOTO missing / Left parenthesis missing
keys: GOTO missing, Left parenthesis missing

## Cause

`ON expression` is not followed by GOTO or GOSUB, or a function or
array name needs its `(`.

## Remedy

Write `ON n GOTO a, b` and put arguments in parentheses.

See also: [ON](ON), [GOTO](GOTO)
