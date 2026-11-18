# ERR and ERL Functions
keys: ERR, ERL

Syntax
    ERR
    ERL

In an error handler, ERR is the number of the error that sent
control there, and ERL is the line number of the statement that
raised it, or the nearest numbered line before it; 0 when the
program has no line numbers. Both keep their value until the next error.

See also [ON ERROR](ON ERROR), [ERROR](ERROR), [RESUME](RESUME),
[error codes](error codes)

## Details
In an error handler, ERR is the number of the error that sent
control there, and ERL is the line number of the statement that
raised it, or the nearest numbered line before it; 0 when the
program has no line numbers. Both keep their value until the next error.

Message numbers are listed under [error codes](error codes).

## Example
    ON ERROR GOTO handler
    OPEN "nofile" FOR INPUT AS #1
    END
    handler:
    PRINT "Error"; ERR; "at line"; ERL   ' Error 53 at line 0
    RESUME NEXT
