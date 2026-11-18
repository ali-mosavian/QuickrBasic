# Illegal function call
keys: Illegal function call, error 5

Error 5

An argument is outside what the statement or function accepts.

See also: [error codes](error codes), [Overflow](Overflow)

## Details

Cause: An argument is outside what a statement or function accepts: a
negative LEFT$ length, a SCREEN mode or COLOR value that does not
exist, a bad subscript to MID$, a negative SQR or LOG, and the like.

Remedy: Print the arguments just before the failing line and compare
them with the ranges in the keyword's topic.

Common causes: negative length, bad SCREEN or COLOR value, SQR or LOG
of a negative number, MID$ start of 0.

Trap it with ON ERROR; ERR returns the number.

## Example
    PRINT LEFT$("abc", -1)   ' error 5
    PRINT LEFT$("abc", 1)    ' a
