# WAIT Statement
keys: WAIT

Syntax
    WAIT port, andmask [, xormask]

Pauses until a hardware port shows a given bit. It reads the byte at
port, applies XOR with xormask (0 if omitted), then AND with
andmask, and repeats until the result is not 0. All three are
integers; port is 0 to 65535 and the masks 0 to 255.

See also [INP](INP), [OUT](OUT), [SLEEP](SLEEP), [XOR](XOR)

## Details

Pauses until a hardware port shows a given bit. It reads the byte at
port, applies XOR with xormask (0 if omitted), then AND with
andmask, and repeats until the result is not 0. All three are
integers; port is 0 to 65535 and the masks 0 to 255.

Use it only for hardware you know. A condition that never comes
makes the program hang.

XOR flips the bits that are set in xormask before the AND, so a bit
that should be 0 is waited for by setting it in xormask.

## Example
    WAIT &H3DA, 8           ' VGA: until the vertical retrace starts
    PRINT "retrace"         ' printed once it has
