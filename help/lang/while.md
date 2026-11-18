# WHILE, WEND Statements
keys: WHILE, WEND

Syntax
    WHILE condition
        statements
    WEND

Repeats the statements as long as condition is true (not 0).

See also [DO](DO), [FOR](FOR), [UNTIL](UNTIL)

## Details

Repeats the statements as long as condition is true (not 0). The
test is made first, so the body may never run. Each WHILE needs its
own WEND. DO ... LOOP is the more general loop and has EXIT DO.

WHILE loops nest. WEND closes the nearest open WHILE; a WEND with no
WHILE is "WEND without WHILE", and a WHILE with no WEND is "WHILE
without WEND".

## Example
    n% = 1
    WHILE n% < 100
        n% = n% * 2
    WEND
    PRINT n%                            ' 128
