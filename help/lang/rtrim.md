# RTRIM$ Function
keys: RTRIM$, rtrim

Syntax
    RTRIM$(text$)

Returns text$ without its trailing spaces. [LTRIM$](LTRIM$) trims the
front.

## Example
    PRINT "[" + RTRIM$("hi   ") + "]"     ' [hi]

See also [LTRIM$](LTRIM$), [LEFT$](LEFT$), [RIGHT$](RIGHT$)
