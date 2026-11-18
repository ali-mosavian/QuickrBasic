# INSTR Function
keys: INSTR

Syntax
    INSTR([start,] string1, string2)

Returns the position, counted from 1, where string2 first occurs in
string1, or 0 when it does not.

start is the position to begin searching from (default 1). An empty
string2 is found at start; a start beyond the end of string1 gives 0.
The search tells upper and lower case apart. A start of 0 or less is
an Illegal function call.

## Example
    a$ = "one, two, three"
    PRINT INSTR(a$, ",")        ' 4
    PRINT INSTR(5, a$, ",")     ' 9
    PRINT INSTR(a$, "four")     ' 0

See also [MID$](MID$), [LEFT$](LEFT$), [UCASE$](UCASE$), [LEN](LEN)
