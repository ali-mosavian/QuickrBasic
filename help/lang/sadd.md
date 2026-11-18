# SADD, SSEG Functions
keys: SADD, SSEG

Syntax
    SADD(stringvar$)
    SSEG(stringvar$)

SADD returns the offset of the first character of a string's text;
SSEG returns its segment. Together they locate the characters for
PEEK, POKE and CALL ABSOLUTE.

stringvar$ is a string variable or one element of a string array. A
fixed-length string is not allowed.

The text moves when the program changes or grows strings, so use the
address at once and do not keep it.

## Example
    a$ = "ABC"
    DEF SEG = SSEG(a$)
    PRINT CHR$(PEEK(SADD(a$) + 1))      ' B
    DEF SEG

See also [VARPTR](VARPTR), [DEF SEG](DEF SEG), [PEEK](PEEK),
[POKE](POKE), [FRE](FRE)
