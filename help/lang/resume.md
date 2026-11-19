# RESUME Statement
keys: RESUME

Syntax
    RESUME [NEXT | line | label]
    RESUME 0

Leaves an error handler and carries on with the program.

See also [ON ERROR](ON ERROR), [ERR](ERR), [error codes](error codes)

## Details

Leaves an error handler and carries on with the program.

  RESUME         run the statement that caused the error again
                 (RESUME 0 is the same)
  RESUME NEXT    continue with the statement after it
  RESUME label   continue at line or label

Use it only inside an error handler set by [ON ERROR](ON ERROR).
Elsewhere it is "RESUME without error". If the handler ends without
RESUME, END or STOP, the error is "No RESUME".

Resuming without fixing the cause loops forever.

## Example
    ON ERROR GOTO Retry
    OPEN "A:DATA.TXT" FOR INPUT AS #1    ' no disk: error 71
    END
    Retry:
    PRINT "Insert a disk": SLEEP
    RESUME
