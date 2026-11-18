# Internal error
keys: Internal error, error 51

Error 51

QuickrBasic reached a state it did not expect.

See also: [error codes](error codes)

## Details

Cause: QuickrBasic hit a state it did not expect. It is a fault in the
system, not in your program.

Remedy: Save your work, then reproduce with the smallest program you
can and report it.

Please report it with the smallest program that shows it.

## Example
    ON ERROR GOTO h
    ERROR 51             ' raise it by hand
    END
    h:
    PRINT ERR            ' 51
    RESUME NEXT
