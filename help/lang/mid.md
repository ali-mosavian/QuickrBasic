# MID$ Function and Statement
keys: MID$

Syntax
    MID$(string, start [, length])
    MID$(stringvariable, start [, length]) = replacement

As a function it returns length characters of string from position
start (counted from 1). Without length, or with a length that runs
past the end, everything from start on is returned. A start beyond
the end gives "". start below 1 or a negative length is an Illegal
function call.

As a statement it overwrites part of stringvariable with the first
characters of replacement. At most length characters are replaced,
and never more than fit before the end of the variable or than
replacement holds. The variable's length does not change.

## Example
    a$ = "QuickrBasic"
    PRINT MID$(a$, 7, 3)        ' Bas
    MID$(a$, 1, 5) = "Quick!!"
    PRINT a$                    ' Quick!Basic

See also [LEFT$](LEFT$), [RIGHT$](RIGHT$), [INSTR](INSTR), [LSET](LSET)
