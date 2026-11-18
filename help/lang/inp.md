# INP Function
keys: INP

Syntax
    INP(port)

Reads a byte, 0 to 255, from the I/O port numbered port.

See also [OUT](OUT), [PEEK](PEEK)

## Details
port is 0 to 65535; a port outside that range is an Illegal function
call.

Under DOS the real port is read. On the other platforms only the VGA
palette ports (&H3C7 to &H3C9) and the retrace status at &H3DA are
emulated; every other port reads 255.

## Example
    PRINT INP(&H3DA) AND 8      ' nonzero in vertical retrace (VGA)
