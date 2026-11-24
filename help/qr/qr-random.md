# random Module
keys: ~quickr random, ~quickr random.seed, ~quickr random.int, ~quickr random.real, ~quickr random.pick

Numbers from one seeded sequence, RND's.

    seed(n as double)              the sequence started from n
    int(a, b) as int               a whole number from a to b
    real(a, b) as double           a number from a up to b
    pick(xs) as T                  one of the list's elements

See also [Quickr](quickr index), [math](quickr math)

## Details
The same seed gives the same numbers, run after run. Seed with
time.ticks() for a different sequence each run.

## Example
    ' OPTION DIALECT QUICKR
    import random
    import time
    random.seed(time.ticks())
    print random.int(1, 6)
