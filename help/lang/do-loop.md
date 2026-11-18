# DO...LOOP Statement
keys: DO, LOOP, DO...LOOP

Syntax
    DO [{WHILE | UNTIL} condition]
      statements
    LOOP

    DO
      statements
    LOOP [{WHILE | UNTIL} condition]

DO repeats its block. The condition may stand on the DO line, where
it is tested before each pass, or on the LOOP line, where it is
tested after, so the block runs at least once. WHILE keeps looping
while the condition is true; UNTIL keeps looping while it is false.
With no condition the loop never ends by itself: leave it with
[EXIT](EXIT) DO, GOTO, or END.

## Example
    DO
      INPUT "Number (0 to stop)"; n
      sum = sum + n
    LOOP UNTIL n = 0
    PRINT sum

See also [FOR...NEXT](FOR), [WHILE...WEND](WHILE), [EXIT](EXIT)
