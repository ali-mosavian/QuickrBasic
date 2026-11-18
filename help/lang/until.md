# UNTIL Keyword
keys: UNTIL

Syntax
    DO UNTIL condition ... LOOP
    DO ... LOOP UNTIL condition

UNTIL repeats a DO loop until condition is true. At the top it can
skip the loop at once; at the bottom the body runs at least once.
UNTIL is WHILE with the test reversed.

## Example
    DO
        INPUT "Pick 1-3: ", n%
    LOOP UNTIL n% >= 1 AND n% <= 3

See also [DO](DO), [WHILE](WHILE)
