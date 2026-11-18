# SPACE$ Function
keys: SPACE$

Syntax
    SPACE$(n)

Returns a string of n blanks.

See also [SPC](SPC), [STRING$](STRING$), [TAB](TAB)

## Details

Returns a string of n blanks. n may be 0, which gives an empty
string; a negative n is an Illegal function call.

n may be any numeric expression; it is rounded. SPACE$ builds a
string, so it can be assigned or joined, unlike SPC.

## Example
    PRINT "[" + SPACE$(4) + "]"         ' [    ]
