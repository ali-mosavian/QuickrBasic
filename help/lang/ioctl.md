# IOCTL Statement and IOCTL$ Function
keys: IOCTL, IOCTL$

Syntax
    IOCTL [#]filenumber, controlstring
    IOCTL$([#]filenumber)

In QBasic these send a control string to a device driver and read its
reply. No device in QuickrBasic takes control strings, so both raise
Illegal function call on an open file, and Bad file number otherwise.

See also [OPEN](OPEN), [files](files)
