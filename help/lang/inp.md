# INP Function
keys: INP

Syntax
    INP(port)

Reads a byte, 0 to 255, from the I/O port numbered port.

See also [OUT](OUT), [PEEK](PEEK)

## Details
port is 0 to 65535; a port outside that range is an Illegal function
call.

Under DOS the real port is read. On the other platforms these are
emulated, and every other port reads 255:

    &H60            the keyboard: a key's scan code as it goes down,
                    with 128 added as it comes up
    &H3C7 to &H3C9  the VGA palette
    &H3DA           bit 3 (8): the vertical retrace. It reads 8 once
                    for each frame the window or the page has shown,
                    then 0 until the next, so a program that waits
                    for it steps at the display's rate: 60 a second
                    on most, 120 on some. Without a display it
                    changes at every read.

## Example
    PRINT INP(&H3DA) AND 8      ' nonzero in vertical retrace (VGA)
