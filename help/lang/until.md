# UNTIL Keyword
keys: UNTIL

Syntax
    DO UNTIL condition ... LOOP
    DO ... LOOP UNTIL condition

UNTIL repeats a DO loop until condition is true.

See also [DO](DO), [WHILE](WHILE)

## Details

UNTIL repeats a DO loop until condition is true. At the top it can
skip the loop at once; at the bottom the body runs at least once.
UNTIL is WHILE with the test reversed.

UNTIL cond is WHILE NOT cond. A DO loop with UNTIL at the end always
runs its body once.

## Example
    DO
        INPUT "Pick 1-3: ", n%      ' asks again until 1, 2 or 3
    LOOP UNTIL n% >= 1 AND n% <= 3
