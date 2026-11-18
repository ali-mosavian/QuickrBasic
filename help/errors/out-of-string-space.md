# Out of string space
keys: Out of string space, String formula too complex, error 14, error 16

Errors 14 and 16

String memory is exhausted, or an expression is too complex.

See also: [Out of memory](Out of memory), [LEN](LEN)

## Details

Cause: Strings have used all the memory they can have, or one string
expression is too large and nested to evaluate.

Remedy: Free strings you no longer need (set them to ""), or split the
expression over several statements.

Error 14 Out of string space; error 16 String formula too complex.

Trap it with ON ERROR; ERR returns the number.

## Example
    a$ = SPACE$(32000)
    b$ = a$ + a$ + a$ + a$   ' may give error 14
    a$ = ""                  ' free before building more
