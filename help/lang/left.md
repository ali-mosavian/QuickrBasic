# LEFT$ Function
keys: LEFT$

Syntax
    LEFT$(string, n)

Returns the first n characters of string. If n is at least LEN(string)
the whole string comes back; if n is 0 the result is "". A negative n
is an Illegal function call.

## Example
    a$ = "QuickrBasic"
    PRINT LEFT$(a$, 6)      ' Quickr
    PRINT LEFT$(a$, 99)     ' QuickrBasic

See also [RIGHT$](RIGHT$), [MID$](MID$), [LEN](LEN), [INSTR](INSTR)
