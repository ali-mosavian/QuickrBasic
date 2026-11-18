# RIGHT$ Function
keys: RIGHT$, right

Syntax
    RIGHT$(text$, n)

Returns the last n characters of text$. If n is 0, the result is ""; if
n is at least LEN(text$), the whole string. A negative n is "Illegal
function call".

## Example
    f$ = "REPORT.TXT"
    PRINT RIGHT$(f$, 3)            ' TXT

See also [LEFT$](LEFT$), [MID$](MID$), [LEN](LEN)
