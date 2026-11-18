# ERROR Statement
keys: ERROR

Syntax
    ERROR errorcode

ERROR raises a run-time error with the given number (1 to 255), as
if the program itself had failed. The error is handled by the active
[ON ERROR](ON ERROR) routine or, with none, stops the program with
that error's message. For a number without a standard message the
text is "Unprintable error". Use a number above 200 for errors of
your own.

See also [ON ERROR](ON ERROR), [ERR](ERR), [error codes](error codes)

## Details
ERROR raises a run-time error with the given number (1 to 255), as
if the program itself had failed. The error is handled by the active
[ON ERROR](ON ERROR) routine or, with none, stops the program with
that error's message. For a number without a standard message the
text is "Unprintable error". Use a number above 200 for errors of
your own.

Raising an error is a simple way to test a handler, or to leave deep
code.

## Example
    ON ERROR GOTO handler
    ERROR 250
    END
    handler:
    PRINT "got"; ERR      ' got 250
    RESUME NEXT
