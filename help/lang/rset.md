# RSET Statement
keys: RSET

Syntax
    RSET stringvariable = text$

Puts text$ into a string variable, flush right. Its length does not
change: a shorter text is padded on the left with spaces and a longer
one is cut at its left, keeping the right-hand characters. Works on
fixed-length strings and on FIELD variables.

## Example
    a$ = "          "              ' ten spaces
    RSET a$ = "42"
    PRINT "["; a$; "]"             ' [        42]

See also [LSET](LSET), [FIELD](FIELD)
