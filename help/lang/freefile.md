# FREEFILE Function
keys: FREEFILE

Syntax
    FREEFILE

FREEFILE returns the lowest file number not in use by an open file.

See also [OPEN](OPEN), [CLOSE](CLOSE)

## Details
FREEFILE returns the lowest file number not in use by an open file.
Use it so that a SUB does not have to know which numbers the rest of
the program has taken. Call it just before OPEN; a number already
returned but not yet opened is returned again.

## Example
    n = FREEFILE
    OPEN "out.txt" FOR OUTPUT AS #n
    PRINT n                ' 1 when no file is open
    PRINT #n, "hello"
    CLOSE #n
