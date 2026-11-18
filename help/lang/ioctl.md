# IOCTL Statement and IOCTL$ Function
keys: IOCTL, IOCTL$

Syntax
    IOCTL [#]filenumber, controlstring
    IOCTL$([#]filenumber)

Send a control string to a device driver and read its reply. Not
supported: always an error.

See also [OPEN](OPEN), [files](files)

## Details
In QBasic these send a control string to a device driver and read its
reply. No device in QuickrBasic takes control strings, so both raise
Illegal function call on an open file, and Bad file number otherwise.

## Example
    ON ERROR GOTO Oops
    OPEN "LPT1:" FOR OUTPUT AS #1
    IOCTL #1, "x"
    END
    Oops:
    PRINT "Error"; ERR      ' Error 5  (Illegal function call)
    RESUME NEXT
