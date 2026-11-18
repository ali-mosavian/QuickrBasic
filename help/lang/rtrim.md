# RTRIM$ Function
keys: RTRIM$, rtrim

Syntax
    RTRIM$(text$)

Returns text$ without its trailing spaces.

See also [LTRIM$](LTRIM$), [LEFT$](LEFT$), [RIGHT$](RIGHT$)

## Details

Returns text$ without its trailing spaces. [LTRIM$](LTRIM$) trims the
front.

Only blanks (ASCII 32) are trimmed, not tabs or other control
characters. Fixed-length strings often end in blanks; RTRIM$ removes
them before output.

## Example
    PRINT "[" + RTRIM$("hi   ") + "]"     ' [hi]
