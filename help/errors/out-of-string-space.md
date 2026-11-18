# Out of string space
keys: Out of string space, String formula too complex, error 14, error 16

Errors 14 and 16

## Cause

Strings have used all the memory they can have, or one string
expression is too large and nested to evaluate.

## Remedy

Free strings you no longer need (set them to ""), or split the
expression over several statements.

See also: [Out of memory](Out of memory), [LEN](LEN)
