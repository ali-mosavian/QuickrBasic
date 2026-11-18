# MID$ Function and Statement
keys: MID$

Syntax
    MID$(string, start [, length])
    MID$(stringvariable, start [, length]) = replacement

As a function, returns part of a string. As a statement, overwrites
part of a string variable.

See also [LEFT$](LEFT$), [RIGHT$](RIGHT$), [INSTR](INSTR), [LSET](LSET)

## Details
Function: returns length characters of string from position start
(counted from 1). Without length, or with a length that runs past the
end, everything from start on is returned. A start beyond the end
gives "". start below 1 or a negative length is an Illegal function
call.

Statement: overwrites part of stringvariable with the first
characters of replacement. At most length characters are replaced,
and never more than fit before the end of the variable or than
replacement holds. The variable's length does not change.

## Example
    a$ = "QuickrBasic"
    PRINT MID$(a$, 7, 3)        ' Bas
    MID$(a$, 6, 1) = "!"
    PRINT a$                    ' Quick!Basic
