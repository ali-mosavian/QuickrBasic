# No RESUME / RESUME without error
keys: No RESUME, RESUME without error, error 19, error 20

Errors 19 and 20

A handler ended without RESUME, or RESUME ran with no error.

See also: [RESUME](RESUME), [ON ERROR](ON ERROR), [ERR](ERR)

## Details

Cause: An error handler ended without RESUME, or RESUME ran when no
error was being handled.

Remedy: End each handler with RESUME, RESUME NEXT or RESUME label. Do
not run into the handler's code in normal flow: put END or GOTO before
it.

Error 19 No RESUME; error 20 RESUME without error.

## Example
    ON ERROR GOTO h
    OPEN "NOPE" FOR INPUT AS #1
    PRINT "done"
    END
    h:
    PRINT "oops"
    RESUME NEXT          ' fixed; without it: error 19
