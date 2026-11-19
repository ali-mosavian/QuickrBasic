# ON ERROR Statement
keys: ON ERROR, ON ERROR GOTO, error trapping

Syntax
    ON ERROR GOTO {line | label}
    ON ERROR GOTO 0
    ON ERROR RESUME NEXT

Sends a run-time error to a handler in the program instead of
stopping it.

See also [RESUME](RESUME), [ERROR](ERROR), [error codes](error codes)

## Details
A run-time error normally stops the program. After ON ERROR GOTO the
program jumps to line or label instead; that code is the error
handler. Inside it, [ERR](ERR) holds the error number and [ERL](ERL)
the line.

  GOTO 0         turn trapping off again; later errors stop the run
  RESUME NEXT    ignore errors: run on with the statement after the
                 one that failed

The handler must leave with [RESUME](RESUME), or with END or STOP.
RETURN or GOTO out of it is wrong: the program stays "in" the handler
and the next error is fatal.

An error inside the handler is not trapped.

## Example
    ON ERROR GOTO Oops
    OPEN "NOFILE.DAT" FOR INPUT AS #1
    PRINT "opened"
    END
    Oops:
    PRINT "Error"; ERR      ' Error 53  (File not found)
    RESUME NEXT
