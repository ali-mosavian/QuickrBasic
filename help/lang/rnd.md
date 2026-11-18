# RND Function
keys: RND

Syntax
    RND[(n)]

Returns a random SINGLE number from 0 up to, but not including, 1.

  n < 0    restarts the sequence from n: the same n, the same number
  n = 0    repeats the last number
  n > 0    the next number (the same as leaving n out)

To get a whole number from low to high:

    INT(RND * (high - low + 1)) + low

Without [RANDOMIZE](RANDOMIZE), each run starts with the same numbers.

## Example
    RANDOMIZE TIMER
    FOR i = 1 TO 5: PRINT INT(RND * 100); : NEXT

See also [RANDOMIZE](RANDOMIZE), [INT](INT)
