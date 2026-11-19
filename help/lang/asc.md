# ASC Function
keys: ASC

Syntax
    ASC(s$)

Returns the character code, 0 to 255, of the first character of s$.
An empty s$ gives Illegal function call. ASC is the inverse of
[CHR$](CHR$).

See also [CHR$](CHR$), [ASCII](ASCII), [LEFT$](LEFT$)

## Details
Returns the character code, 0 to 255, of the first character of s$.
An empty s$ gives Illegal function call. ASC is the inverse of
[CHR$](CHR$).

Only the first character counts: ASC("apple") is 97, as ASC("a").

## Example
    PRINT ASC("A"), ASC("apple")    ' 65  97
    IF ASC(k$) = 27 THEN PRINT "Esc"
