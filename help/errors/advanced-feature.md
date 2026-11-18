# Advanced feature unavailable
keys: Advanced feature unavailable, error 73

Error 73

The statement belongs to a feature QuickrBasic does not have.

See also: [error codes](error codes)

## Details

Cause: The statement is part of a feature this QuickrBasic does not
have.

Remedy: Remove it or use another statement.

The message is kept for compatibility with QBasic programs that trap
error 73.

## Example
    PRINT "start"
    ON ERROR GOTO oops
    ERROR 73             ' raise it by hand
    END
    oops:
    PRINT "error"; ERR   ' error 73
    RESUME NEXT
