# COMMON Statement
keys: COMMON

Syntax
    COMMON [SHARED] variable[()] [AS type] [, variable[()] [AS type]] ...

Names the variables whose values pass to a program started with
CHAIN. The chained program lists the same variables, in the same
order and types, in its own COMMON.

SHARED also makes them visible inside every SUB and FUNCTION, as
DIM SHARED does. An array is listed with empty parentheses, its
size set afterwards by DIM or REDIM.

## Example
    COMMON SHARED score AS LONG, name$
    score = 100
    CHAIN "LEVEL2.BAS"

See also [CHAIN](CHAIN), [DIM](DIM), [SHARED](SHARED)
