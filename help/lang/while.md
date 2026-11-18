# WHILE, WEND Statements
keys: WHILE, WEND

Syntax
    WHILE condition
        statements
    WEND

Repeats the statements as long as condition is true (not 0). The
test is made first, so the body may never run. Each WHILE needs its
own WEND. DO ... LOOP is the more general loop and has EXIT DO.

## Example
    n% = 1
    WHILE n% < 100
        n% = n% * 2
    WEND
    PRINT n%                            ' 128

See also [DO](DO), [FOR](FOR), [UNTIL](UNTIL)
