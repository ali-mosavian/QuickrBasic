# WAIT Statement
keys: WAIT

Syntax
    WAIT port, andmask [, xormask]

Pauses until a hardware port shows a given bit. It reads the byte at
port, applies XOR with xormask (0 if omitted), then AND with
andmask, and repeats until the result is not 0. All three are
integers; port is 0 to 65535 and the masks 0 to 255.

Use it only for hardware you know. A condition that never comes
makes the program hang.

## Example
    WAIT &H3DA, 8           ' VGA: until the vertical retrace starts

See also [INP](INP), [OUT](OUT), [SLEEP](SLEEP), [XOR](XOR)
