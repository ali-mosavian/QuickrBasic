# DEFSHORT Statement
keys: DEFSHORT

Syntax
    DEFSHORT letter[-letter] [, letter[-letter]]...

Makes every variable and function whose name starts with one of the
letters a [SHORT](SHORT), unless a type character or an AS clause says
otherwise. It covers the rest of the program from where it appears.

## Example
    DEFSHORT S
    s1 = 20000
    PRINT s1
    s1 = s1 * 2           ' Overflow: 40000 does not fit in 16 bits

See also [DEFINT](DEFtype), [DEFBYTE](DEFBYTE)
