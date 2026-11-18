# No RESUME / RESUME without error
keys: No RESUME, RESUME without error, error 19, error 20

Errors 19 and 20

## Cause

An error handler ended without RESUME, or RESUME ran when no error was
being handled.

## Remedy

End each handler with RESUME, RESUME NEXT or RESUME label. Do not run
into the handler's code in normal flow: put END or GOTO before it.

See also: [RESUME](RESUME), [ON ERROR](ON ERROR), [ERR](ERR)
