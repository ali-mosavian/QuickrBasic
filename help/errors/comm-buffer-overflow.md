# Communication-buffer overflow
keys: Communication-buffer overflow, error 69

Error 69

Listed for compatibility; QuickrBasic never raises it.

See also: [error codes](error codes)

## Details

Cause: QuickrBasic has no serial ports, so this error is listed but
never raised.

Remedy: None.

There are no serial ports, so no program can overflow a communication
buffer.

## Example
    ON ERROR GOTO h
    ERROR 69             ' raise it by hand
    END
    h:
    PRINT ERR            ' 69
    RESUME NEXT
