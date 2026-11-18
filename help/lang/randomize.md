# RANDOMIZE Statement
keys: RANDOMIZE

Syntax
    RANDOMIZE [seed]

Starts RND's sequence from seed. The same seed always gives the same
sequence.

See also [RND](RND), [TIMER](TIMER)

## Details
A fixed seed is useful for repeating a test. Without seed, the
program asks for one ("Random-number seed?") when it runs.

For a different sequence each run, seed from the clock:

    RANDOMIZE TIMER

A program that never calls RANDOMIZE gets the same numbers every run.

## Example
    RANDOMIZE TIMER
    PRINT INT(RND * 6) + 1         ' a die
