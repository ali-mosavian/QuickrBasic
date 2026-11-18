# DEFSHORT Statement
keys: DEFSHORT

Syntax
    DEFSHORT letter[-letter] [, letter[-letter]]...

Makes every variable and function whose name starts with one of the
letters a [SHORT](SHORT).

See also [DEFINT](DEFtype), [DEFBYTE](DEFBYTE)

## Details
A type character or an AS clause on a name overrides DEFSHORT. A
SHORT has no type character, so DEFSHORT is the way to get SHORT
variables without naming each one.

It covers the rest of the program from where it appears. Storing a
value outside -32768 to 32767 is an Overflow error.

## Example
    DEFSHORT S
    s1 = 20000
    PRINT s1              ' 20000
    s1 = s1 * 2           ' Overflow: 40000 does not fit in 16 bits
