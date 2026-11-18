# ASC Function
keys: ASC

Syntax
    ASC(s$)

Returns the character code, 0 to 255, of the first character of s$.
An empty s$ gives Illegal function call. ASC is the inverse of
[CHR$](CHR$).

## Example
    PRINT ASC("A"), ASC("apple")    ' 65  97
    IF ASC(k$) = 27 THEN PRINT "Esc"

See also [CHR$](CHR$), [ASCII](ASCII), [LEFT$](LEFT$)
