# RIGHT$ Function
keys: RIGHT$, right

Syntax
    RIGHT$(text$, n)

Returns the last n characters of text$.

See also [LEFT$](LEFT$), [MID$](MID$), [LEN](LEN)

## Details

Returns the last n characters of text$. If n is 0, the result is ""; if
n is at least LEN(text$), the whole string. A negative n is "Illegal
function call".

LEN(text$) gives the count to pass, so RIGHT$(a$, LEN(a$) - 1) drops
the first character. n may be any numeric expression; it is rounded to
an integer.

## Example
    f$ = "REPORT.TXT"
    PRINT RIGHT$(f$, 3)            ' TXT
