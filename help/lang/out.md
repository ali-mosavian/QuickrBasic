# OUT Statement
keys: OUT

Syntax
    OUT port, byte

Sends byte (0 to 255) to a hardware I/O port (0 to 65535).

See also [INP](INP), [WAIT](WAIT), [PALETTE](PALETTE), [POKE](POKE)

## Details
On a DOS machine the byte goes to the real port. On other systems
only the VGA palette ports (&H3C7 to &H3C9) are acted on, which is
how palette programs work; &H3DA reads as a retrace flag. All other
ports are ignored. A value out of range is "Illegal function call".

## Example
    SCREEN 13
    OUT &H3C8, 1                   ' color 1: set its palette entry
    OUT &H3C9, 63: OUT &H3C9, 0: OUT &H3C9, 0    ' bright red
