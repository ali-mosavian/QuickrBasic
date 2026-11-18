# RSET Statement
keys: RSET

Syntax
    RSET stringvariable = text$

Puts text$ into a string variable, flush right.

See also [LSET](LSET), [FIELD](FIELD)

## Details

Puts text$ into a string variable, flush right. Its length does not
change: a shorter text is padded on the left with spaces and a longer
one is cut at its left, keeping the right-hand characters. Works on
fixed-length strings and on FIELD variables.

Use LSET for left-justified text. Neither statement allocates: the
string variable must already have the length you want.

## Example
    a$ = "          "              ' ten spaces
    RSET a$ = "42"
    PRINT "["; a$; "]"             ' [        42]
