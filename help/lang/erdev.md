# ERDEV and ERDEV$ Functions
keys: ERDEV, ERDEV$

Syntax
    ERDEV
    ERDEV$

After a device error, ERDEV returns the code the DOS driver gave
(the low byte is the error, the high byte the device attributes),
and ERDEV$ returns the device's name, such as "A:" or "LPT1".
Both are meaningful only inside an error handler set by
[ON ERROR](ON ERROR); otherwise they hold the last value, or 0 and "".

## Example
    ON ERROR GOTO oops
    OPEN "A:X.TXT" FOR INPUT AS #1
    END
    oops:
    PRINT "Device "; ERDEV$; " code"; ERDEV AND 255
    RESUME NEXT

See also [ERR](ERR), [ON ERROR](ON ERROR), [error codes](error codes)
